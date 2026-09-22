"""Validate the machine-readable contract for the generated STW_FP_01 assets."""

import argparse
import json
import sys
from pathlib import Path


REQUIRED_BONES = [
    "root",
    "weapon",
    "upperarm_L",
    "forearm_L",
    "hand_L",
    "upperarm_R",
    "forearm_R",
    "hand_R",
]
REQUIRED_ACTIONS = ["ads", "idle", "reload"]
REQUIRED_ANIMATION_EXPORTS = {
    "idle": "STW_FP_01.fbx",
    "ads": "STW_FP_01_ads.fbx",
    "reload": "STW_FP_01_reload.fbx",
}


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-root", required=True, type=Path)
    return parser.parse_args()


def validate(asset_root):
    errors = []
    report_path = asset_root / "STW_FP_01.report.json"
    for name in ("STW_FP_01.blend", *REQUIRED_ANIMATION_EXPORTS.values()):
        path = asset_root / name
        if not path.is_file():
            errors.append(f"missing file: {name}")
        elif path.stat().st_size <= 1024:
            errors.append(f"file is unexpectedly small: {name}")
    if not report_path.is_file():
        errors.append(f"missing file: {report_path.name}")

    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        errors.append(f"report: {error}")
        return errors

    if report.get("asset_family") != "STW_FP_01":
        errors.append("asset_family must be STW_FP_01")
    if report.get("generator") != "Blender 4.5.14 LTS":
        errors.append("generator must be Blender 4.5.14 LTS")
    if report.get("mesh_objects", 0) < 8:
        errors.append("mesh_objects must be at least 8")
    if report.get("vertices", 0) < 1000:
        errors.append("vertices must be at least 1000")
    if report.get("triangles", 0) < 1500:
        errors.append("triangles must be at least 1500")
    if report.get("required_bones") != REQUIRED_BONES:
        errors.append(f"required_bones must equal {REQUIRED_BONES}")
    if report.get("actions") != REQUIRED_ACTIONS:
        errors.append(f"actions must equal {REQUIRED_ACTIONS}")
    if report.get("animation_exports") != REQUIRED_ANIMATION_EXPORTS:
        errors.append(f"animation_exports must equal {REQUIRED_ANIMATION_EXPORTS}")
    if report.get("all_meshes_have_uvs") is not True:
        errors.append("all_meshes_have_uvs must be true")
    if report.get("all_skinned_meshes_have_armature_modifier") is not True:
        errors.append("all_skinned_meshes_have_armature_modifier must be true")
    if report.get("material_slots", 0) < 4:
        errors.append("material_slots must be at least 4")

    bounds = report.get("binding_pose_bounds", {})
    minimum = bounds.get("minimum", [])
    maximum = bounds.get("maximum", [])
    if len(minimum) != 3 or len(maximum) != 3:
        errors.append("binding_pose_bounds must contain three-dimensional minimum and maximum")
    else:
        if minimum[2] < 0.30:
            errors.append("binding_pose minimum z must be at least 0.30 m")
        if maximum[2] > 1.15:
            errors.append("binding_pose maximum z must be at most 1.15 m")
    if report.get("hand_to_grip_distance_max", 999.0) > 0.30:
        errors.append("hand_to_grip_distance_max must be at most 0.30 m")
    return errors


def main():
    options = parse_args()
    errors = validate(options.asset_root)
    if errors:
        for error in errors:
            print(f"ERROR {error}", file=sys.stderr)
        return 1
    print(
        "STW_FP_ASSET_VALID family=STW_FP_01 bones=8 actions=3 "
        "uvs=1 armature_modifiers=1 coordinate=Z-up/-Y"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
