#!/usr/bin/env python3
"""Project Visual Forge: offline contracts and evidence gates, Python 3.10+.

All input paths in contracts are relative to --root (the stw-o3de directory).
No command edits assets, launches the editor, or promotes content automatically.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys

DEFAULT_ROOT = Path(__file__).resolve().parents[2]
CONFIG = "Config/VisualForge"


class Invalid(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise Invalid(message)


def number(value, name, minimum=0):
    require(type(value) in (int, float) and math.isfinite(value)
            and value >= minimum, f"{name}: finite number >= {minimum} required")
    return value


def integer(value, name, minimum=0):
    require(type(value) is int and value >= minimum, f"{name}: integer >= {minimum} required")
    return value


def path_in(root, value):
    require(isinstance(value, str) and value and not Path(value).is_absolute(),
            "expected nonempty relative path")
    path = (root / value).resolve()
    require(path.is_relative_to(root.resolve()), f"path escapes root: {value}")
    require(path.is_file(), f"missing file: {value}")
    return path


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f"duplicate JSON key: {key}")
            result[key] = value
        return result
    value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)
    require(isinstance(value, dict), f"JSON object required: {path}")
    return value


def artifact(root, item):
    require(isinstance(item, dict), "artifact must be an object with path and sha256")
    path = path_in(root, item["path"])
    require(re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) is not None,
            f"invalid sha256: {path.name}")
    require(digest(path) == item["sha256"], f"stale artifact: {item['path']}")
    require(path.stat().st_size > 0, f"empty artifact: {path.name}")
    with path.open("rb") as stream:
        require(not stream.read(100).startswith(b"version https://git-lfs.github.com/spec/"),
                f"unhydrated LFS pointer: {path.name}")
    return path


def content_id(files):
    pairs = sorted((f["path"], f["sha256"]) for f in files)
    return hashlib.sha256(json.dumps(pairs, separators=(",", ":")).encode()).hexdigest()


def named(path, policy):
    prefix = path.stem.split("_")[0]
    return (bool(re.fullmatch(r"(?:" + "|".join(policy["prefixes"]) +
                             r")_[A-Za-z0-9]+(?:_[A-Za-z0-9]+)*", path.stem))
            and path.suffix.lower() in policy["prefixes"].get(prefix, []))


def material_references(root, material, closure):
    """Check local material dependencies; only the pinned StandardPBR alias is built in."""
    data = load(material)
    references = []
    for key in ("parentMaterial", "materialType"):
        if data.get(key):
            references.append(data[key])

    def visit(value):
        if isinstance(value, dict):
            for key, item in value.items():
                if key.split(".")[-1] == "textureMap" and item:
                    require(isinstance(item, str), "textureMap must be a path")
                    references.append(item)
                else:
                    visit(item)
    visit(data.get("propertyValues", {}))
    for reference in references:
        if reference == "@gemroot:Atom_Feature_Common@/Assets/Materials/Types/StandardPBR.materialtype":
            continue
        candidates = [(material.parent / reference).resolve(),
                      (root / "Project" / reference).resolve(),
                      (root / "Project/Assets" / reference).resolve()]
        require(any(path in closure for path in candidates),
                f"material dependency missing from file closure: {reference}")


def contracts(root):
    policy = load(path_in(root, f"{CONFIG}/policy.json"))
    profiles = load(path_in(root, f"{CONFIG}/quality_profiles.json"))
    lighting = load(path_in(root, f"{CONFIG}/lighting_recipes.json"))
    for doc in (policy, profiles, lighting):
        require(doc["schema_version"] == 1, "unsupported schema_version")
    require(profiles["status"] == "TARGET_ONLY" and
            profiles["runtime_adapter"] == "NOT_YET_IMPLEMENTED",
            "runtime activation needs a separately verified adapter")
    require(set(profiles["profiles"]) == {f"Quality_{n}" for n in ("Low", "Medium", "High", "Ultra")},
            "four quality profiles required")
    keys = {"output_resolution", "resolution_scale", "shadow_resolution", "shadow_distance_m",
            "dynamic_lights_max", "volumetric_fog", "reflections", "ambient_occlusion",
            "particle_density", "vegetation_density", "view_distance_m", "lod_screen_ratios",
            "texture_max_edge", "bloom", "motion_blur", "depth_of_field", "chromatic_aberration",
            "raytracing", "tone_mapping"}
    for name, profile in profiles["profiles"].items():
        require(set(profile) == keys, f"{name}: profile fields differ from contract")
        for key in ("resolution_scale", "particle_density", "vegetation_density"):
            require(0 < number(profile[key], key) <= 1, f"{name}: {key} outside (0,1]")
        for key in ("shadow_resolution", "dynamic_lights_max", "texture_max_edge"):
            integer(profile[key], key, 1)
        for key in ("shadow_distance_m", "view_distance_m"):
            number(profile[key], key, 1)
        require(profile["output_resolution"] == policy["capture"]["output_resolution"], "resolution mismatch")
        ratios = profile["lod_screen_ratios"]
        require(isinstance(ratios, list) and len(ratios) == 3, "three LOD thresholds required")
        for ratio in ratios:
            require(0 < number(ratio, "LOD ratio") <= 1, "LOD ratio outside (0,1]")
        require(ratios[0] > ratios[1] > ratios[2], "LOD thresholds must descend")
        require(profile["motion_blur"] == profile["depth_of_field"] == profile["raytracing"] == "off",
                "gameplay baseline requires motion blur, DoF and RT off")
    require(lighting["status"] == "TARGET_ONLY" and lighting["runtime_adapter"] == "NOT_YET_IMPLEMENTED",
            "lighting adapter has not been verified")
    path_in(root, lighting["shared_scene"])
    require(set(lighting["recipes"]) == {"LVL_Lighting_Day", "LVL_Lighting_Night", "LVL_Lighting_Overcast"},
            "day/night/overcast recipes required")
    for recipe in lighting["recipes"].values():
        number(recipe["sun_lux"], "sun_lux")
        number(recipe["exposure_ev100"], "exposure_ev100", -20)
    for budget in policy["asset_budgets"].values():
        for key in ("triangles", "materials", "texture_memory_mib", "texture_edge", "lod_count"):
            number(budget[key], key, 1)
    for key, value in policy["scene_budget"].items():
        number(value, key, 1)
    require(policy["capture"]["players"] >= 8, "eight-player target required")
    for key in ("runs", "warmup_seconds", "sample_seconds", "min_samples"):
        integer(policy["capture"][key], key, 1)
    return policy, profiles


def validate_asset(root, manifest, policy):
    require(manifest["schema_version"] == 1, "unsupported asset schema")
    require(manifest["classification"] in policy["classifications"], "unknown classification")
    require(manifest["stage"] in policy["stages"], "unknown stage")
    require(bool(manifest["owner"].strip()), "asset owner required")
    files = manifest["files"]
    require(isinstance(files, list) and files, "nonempty asset file closure required")
    paths = [f["path"] for f in files]
    require(len(set(paths)) == len(paths), "duplicate asset files")
    for item in files:
        path = artifact(root, item)
        relative = path.relative_to(root.resolve())
        require(len(relative.parts) >= 4 and relative.parts[:2] == ("Project", "Assets")
                and relative.parts[2] in policy["asset_folders"], "asset outside runtime asset folders")
        # O3DE scene import sidecars follow the source filename, e.g. SM_Rifle.fbx.assetinfo.
        check_path = path.with_suffix("") if path.suffix == ".assetinfo" else path
        require(named(check_path, policy), f"invalid asset name: {relative}")
    closure = {path_in(root, name) for name in paths}
    for path in closure:
        if path.suffix == ".material":
            material_references(root, path, closure)
    require(manifest["primary"] in paths, "primary must belong to asset file closure")
    budget = policy["asset_budgets"][manifest["budget_class"]]
    inspection = load(artifact(root, manifest["inspection"]))
    require(inspection["content_id"] == content_id(files), "inspection belongs to different asset content")
    require(bool(inspection["tool"].strip()) and bool(inspection["tool_version"].strip()), "exporter version required")
    metrics = inspection["metrics"]
    for key in ("triangles", "materials", "texture_memory_mib", "texture_edge"):
        require(number(metrics[key], key) <= budget[key], f"asset budget exceeded: {key}")
    require(integer(metrics["triangles"], "triangles", 1) > 0, "mesh has no triangles")
    integer(metrics["materials"], "materials", 1)
    require(number(metrics["scale_m_per_unit"], "scale_m_per_unit") == 1, "export must use metres")
    lods = inspection["lods"]
    require(isinstance(lods, list) and len(lods) >= budget["lod_count"], "missing LODs (count includes LOD0)")
    counts = [integer(lod["triangles"], "LOD triangles", 1) for lod in lods]
    require(counts[0] == metrics["triangles"] and all(a > b for a, b in zip(counts, counts[1:])),
            "LOD triangle counts must decrease from LOD0")
    require(len({lod["node"] for lod in lods}) == len(lods), "duplicate LOD nodes")
    for check in policy["required_asset_checks"]:
        require(inspection["checks"][check] == "PASS", f"asset check missing or failed: {check}")
    require(integer(inspection["asset_processor"]["errors"], "AP errors") == 0 and
            integer(inspection["asset_processor"]["warnings"], "AP warnings") == 0, "Asset Processor errors/warnings")
    artifact(root, inspection["asset_processor"]["log"])
    path_in(root, inspection["target_level"])
    # Binary FBX/GLB metrics are supplied by a versioned exporter, not guessed here.
    primary = path_in(root, manifest["primary"])
    if primary.suffix.lower() == ".obj":
        triangles = 0
        for line in primary.read_text(encoding="utf-8").splitlines():
            tokens = line.split()
            if tokens and tokens[0] == "f":
                require(len(tokens) >= 4, "invalid OBJ face")
                triangles += len(tokens) - 3
        require(triangles == metrics["triangles"], "OBJ triangle count disagrees with inspection")
    return {"content_id": content_id(files), "files": len(files),
            "classification": manifest["classification"], "approval": "NOT_GRANTED"}


def percentile(values, quantile):
    return sorted(values)[max(0, math.ceil(len(values) * quantile) - 1)]


def scene_gate(root, evidence, policy, profiles, revision):
    require(re.fullmatch(r"[0-9a-f]{40}", revision) is not None, "exact 40-digit candidate revision required")
    require(evidence["schema_version"] == 1 and evidence["revision"] == revision, "stale scene evidence")
    require(evidence["engine_commit"] == policy["engine_commit"], "engine revision mismatch")
    require(evidence["policy_sha256"] == digest(path_in(root, f"{CONFIG}/policy.json")), "stale budget policy")
    require(evidence["profiles_sha256"] == digest(path_in(root, f"{CONFIG}/quality_profiles.json")), "stale profiles")
    require(evidence["profile"] in profiles["profiles"], "unknown quality profile")
    require(evidence["effective_settings"] == profiles["profiles"][evidence["profile"]], "effective settings mismatch")
    for key in ("gpu", "cpu", "driver", "os", "build_configuration", "capture_tool"):
        require(isinstance(evidence[key], str) and evidence[key].strip()
                and evidence[key] not in ("UNAVAILABLE", "UNVERIFIED"), f"missing capture metadata: {key}")
    artifact(root, evidence["scene"])
    artifact(root, evidence["effective_settings_log"])
    artifact(root, evidence["multiplayer_log"])
    require(integer(evidence["connected_players"], "connected_players") >= policy["capture"]["players"], "eight-player run required")
    require(evidence["output_resolution"] == policy["capture"]["output_resolution"], "capture resolution mismatch")
    runs = evidence["runs"]
    require(isinstance(runs, list) and len(runs) >= policy["capture"]["runs"], "too few benchmark runs")
    require(len({run["sha256"] for run in runs}) == len(runs), "duplicate benchmark data")
    summaries = []
    for run in runs:
        with artifact(root, run).open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        require(bool(rows), "empty benchmark")
        columns = ("time_s", "frame_ms", "cpu_ms", "gpu_ms", "draw_calls", "vram_mib", "ram_mib")
        parsed = []
        for row in rows:
            parsed.append({key: number(float(row[key]), key) for key in columns})
        times = [row["time_s"] for row in parsed]
        require(times[0] <= 1 and all(a < b for a, b in zip(times, times[1:])), "timestamps must start near zero and increase")
        samples = [row for row in parsed if row["time_s"] >= policy["capture"]["warmup_seconds"]]
        require(len(samples) >= policy["capture"]["min_samples"], "insufficient post-warmup samples")
        require(samples[-1]["time_s"] - samples[0]["time_s"] >= policy["capture"]["sample_seconds"], "capture too short")
        require(all(row[k] > 0 for row in samples for k in columns if k != "time_s"), "missing/zero telemetry")
        duration = samples[-1]["time_s"] - samples[0]["time_s"]
        frame_duration = sum(row["frame_ms"] for row in samples[1:]) / 1000
        require(abs(frame_duration - duration) <= duration * .05, "CSV must contain contiguous per-frame telemetry")
        summary = {"frame_p95_ms": percentile([r["frame_ms"] for r in samples], .95),
                   "frame_p99_ms": percentile([r["frame_ms"] for r in samples], .99),
                   "cpu_p95_ms": percentile([r["cpu_ms"] for r in samples], .95),
                   "gpu_p95_ms": percentile([r["gpu_ms"] for r in samples], .95),
                   "draw_calls_peak": max(r["draw_calls"] for r in samples),
                   "vram_mib_peak": max(r["vram_mib"] for r in samples),
                   "ram_mib_peak": max(r["ram_mib"] for r in samples)}
        for key, limit in policy["scene_budget"].items():
            require(summary[key] <= limit, f"scene budget exceeded: {key}={summary[key]} > {limit}")
        summaries.append(summary)
    return {"runs": summaries, "revision": revision, "approval": "NOT_GRANTED"}


def audit(root, policy):
    extensions = {ext for group in policy["prefixes"].values() for ext in group}
    rows = []
    for path in sorted((root / "Project/Assets").rglob("*")):
        if path.is_file() and path.suffix.lower() in extensions:
            rows.append({"path": path.relative_to(root).as_posix(), "bytes": path.stat().st_size,
                         "naming": "PASS" if named(path, policy) else "LEGACY_REQUIRES_MIGRATION",
                         "approval": "UNVERIFIED"})
    require(bool(rows), "asset inventory is empty")
    return {"assets": rows, "count": len(rows), "approval": "NOT_GRANTED"}


def changed_assets(root, policy, base):
    """Require validated manifests for newly added/modified exported content in CI."""
    require(re.fullmatch(r"[0-9a-f]{40}", base) is not None, "exact base revision required")
    repository = root.parent
    command = ["git", "diff", "--name-only", "--no-renames", "--diff-filter=ACM", base,
               "HEAD", "--", "stw-o3de/Project/Assets/"]
    result = subprocess.run(command, cwd=repository, capture_output=True, text=True, check=False)
    require(result.returncode == 0, f"cannot compare candidate with base: {result.stderr.strip()}")
    extensions = {ext for group in policy["prefixes"].values() for ext in group} | {".assetinfo"}
    changed = {name.removeprefix("stw-o3de/") for name in result.stdout.splitlines()
               if Path(name).suffix.lower() in extensions}
    covered = set()
    count = 0
    for path in sorted((root / CONFIG / "Assets").glob("*.json")):
        manifest = load(path)
        paths = {item["path"] for item in manifest["files"]}
        if paths & changed:
            validate_asset(root, manifest, policy)
            covered |= paths
            count += 1
    require(not (changed - covered), "changed assets lack validated manifests: " + ", ".join(sorted(changed - covered)))
    return {"changed_assets": len(changed), "validated_manifests": count,
            "approval": "NOT_GRANTED", "scope": "added/modified exports; level/deletion reviews remain separate"}


def promotion(root, manifest, evidence, policy, profiles, revision):
    asset_result = validate_asset(root, manifest, policy)
    require(manifest["classification"] in ("PRODUCTION_CANDIDATE", "PRODUCTION_READY"), "only production candidates can be reviewed")
    scene_result = scene_gate(root, evidence, policy, profiles, revision)
    inspection = load(artifact(root, manifest["inspection"]))
    require(path_in(root, inspection["target_level"]) == artifact(root, evidence["scene"]),
            "capture scene differs from asset target level")
    require(asset_result["content_id"] in evidence["asset_content_ids"], "asset absent from benchmark scene closure")
    reviews = manifest["reviews"]
    for role in policy["required_reviews"]:
        review = reviews[role]
        require(review["decision"] == "APPROVED" and bool(review["reviewer"].strip()), f"missing approval: {role}")
        require(review["revision"] == revision and review["content_id"] == asset_result["content_id"], f"stale review: {role}")
        artifact(root, review["evidence"])
    return {"asset": asset_result, "scene": scene_result, "approval": "ELIGIBLE_FOR_REVIEWED_PROMOTION"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--output", type=Path, help="write generated JSON report (use ignored Build/VisualForge)")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("contracts")
    sub.add_parser("audit")
    changes = sub.add_parser("changes")
    changes.add_argument("--base", required=True)
    asset = sub.add_parser("asset")
    asset.add_argument("manifest", type=Path)
    scene = sub.add_parser("scene")
    scene.add_argument("evidence", type=Path)
    scene.add_argument("--revision", required=True)
    promote = sub.add_parser("promote")
    promote.add_argument("manifest", type=Path)
    promote.add_argument("evidence", type=Path)
    promote.add_argument("--revision", required=True)
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        policy, profiles = contracts(root)
        if args.command == "contracts":
            result = {"profiles": len(profiles["profiles"]), "runtime_adapter": profiles["runtime_adapter"]}
        elif args.command == "audit":
            result = audit(root, policy)
        elif args.command == "changes":
            result = changed_assets(root, policy, args.base)
        elif args.command == "asset":
            result = validate_asset(root, load(args.manifest), policy)
        elif args.command == "scene":
            result = scene_gate(root, load(args.evidence), policy, profiles, args.revision)
        else:
            result = promotion(root, load(args.manifest), load(args.evidence), policy, profiles, args.revision)
        report, code = {"status": "PASS", "gate": args.command, "result": result}, 0
    except (Invalid, OSError, ValueError, KeyError, TypeError, AttributeError, IndexError) as exc:
        report, code = {"status": "FAIL", "gate": args.command, "error": str(exc)}, 1
    rendered = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        # Restrict generated reports to the build tree; never overwrite source inputs.
        target = args.output.resolve()
        try:
            require(target.is_relative_to((root / "Build/VisualForge").resolve()), "output must be under Build/VisualForge")
            require(not target.exists(), "report already exists; use a new run-specific path")
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("x", encoding="utf-8") as stream:
                stream.write(rendered)
        except (Invalid, OSError) as exc:
            rendered = json.dumps({"status": "FAIL", "gate": args.command, "error": str(exc)}, indent=2) + "\n"
            code = 1
    print(rendered, end="")
    return code


if __name__ == "__main__":
    sys.exit(main())
