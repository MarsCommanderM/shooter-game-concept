"""Gate regression tests. Synthetic fixtures prove tooling, never game performance."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

import forge


class ForgeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ("policy", "quality_profiles", "lighting_recipes"):
            self.write(f"Config/VisualForge/{name}.json",
                       (forge.DEFAULT_ROOT / f"Config/VisualForge/{name}.json").read_text())
        self.write("Project/Levels/DefaultLevel/DefaultLevel.prefab", "{}")
        self.policy, self.profiles = forge.contracts(self.root)

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return {"path": relative, "sha256": forge.digest(path)}

    def asset_fixture(self):
        mesh = self.write("Project/Assets/Weapons/SM_Rifle_Main.obj",
                          "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\nf 1 3 2\nf 2 1 3\n")
        log = self.write("Build/VisualForge/ap.log", "synthetic test log; not production evidence\n")
        manifest = {"schema_version": 1, "owner": "test", "classification": "PRODUCTION_CANDIDATE",
                    "stage": "Performance Validation", "files": [mesh], "primary": mesh["path"],
                    "budget_class": "weapon_fp"}
        inspection = {"content_id": forge.content_id([mesh]), "tool": "synthetic-test-exporter",
                      "tool_version": "1", "metrics": {"triangles": 3, "materials": 1,
                      "texture_memory_mib": 1, "texture_edge": 1024, "scale_m_per_unit": 1},
                      "lods": [{"node": "lod0", "triangles": 3}, {"node": "lod1", "triangles": 2},
                               {"node": "lod2", "triangles": 1}],
                      "checks": dict.fromkeys(self.policy["required_asset_checks"], "PASS"),
                      "asset_processor": {"errors": 0, "warnings": 0, "log": log},
                      "target_level": "Project/Levels/DefaultLevel/DefaultLevel.prefab"}
        self.update_inspection(manifest, inspection)
        return manifest, inspection

    def update_inspection(self, manifest, inspection):
        manifest["inspection"] = self.write("Build/VisualForge/inspection.json", json.dumps(inspection))

    def scene_fixture(self):
        evidence = {"schema_version": 1, "revision": "a" * 40,
                    "engine_commit": self.policy["engine_commit"], "profile": "Quality_High",
                    "policy_sha256": forge.digest(self.root / "Config/VisualForge/policy.json"),
                    "profiles_sha256": forge.digest(self.root / "Config/VisualForge/quality_profiles.json"),
                    "effective_settings": copy.deepcopy(self.profiles["profiles"]["Quality_High"]),
                    "connected_players": 8, "output_resolution": [1920, 1080], "runs": [],
                    "scene": self.write("Project/Levels/DefaultLevel/DefaultLevel.prefab", "synthetic scene"),
                    "effective_settings_log": self.write("Build/VisualForge/settings.log", "synthetic settings"),
                    "multiplayer_log": self.write("Build/VisualForge/network.log", "synthetic network")}
        for key in ("gpu", "cpu", "driver", "os", "build_configuration", "capture_tool"):
            evidence[key] = "synthetic-test-only"
        for run in range(3):
            data = "time_s,frame_ms,cpu_ms,gpu_ms,draw_calls,vram_mib,ram_mib\n"
            data += "".join(f"{i / 100:.2f},10,5,{6 + run / 10},300,1000,2000\n" for i in range(9002))
            evidence["runs"].append(self.write(f"Build/VisualForge/run{run}.csv", data))
        return evidence

    def scene(self, evidence):
        return forge.scene_gate(self.root, evidence, self.policy, self.profiles, "a" * 40)

    def test_contracts_have_four_target_only_profiles(self):
        self.assertEqual(len(self.profiles["profiles"]), 4)
        self.assertEqual(self.profiles["runtime_adapter"], "NOT_YET_IMPLEMENTED")

    def test_asset_validation_does_not_grant_approval(self):
        manifest, _ = self.asset_fixture()
        self.assertEqual(forge.validate_asset(self.root, manifest, self.policy)["approval"], "NOT_GRANTED")

    def test_stale_asset_hash_rejected(self):
        manifest, _ = self.asset_fixture()
        self.write(manifest["primary"], "changed mesh")
        with self.assertRaisesRegex(forge.Invalid, "stale artifact"):
            forge.validate_asset(self.root, manifest, self.policy)

    def test_missing_lods_and_bad_scale_rejected(self):
        for field in ("lods", "scale"):
            manifest, inspection = self.asset_fixture()
            if field == "lods":
                inspection["lods"] = []
            else:
                inspection["metrics"]["scale_m_per_unit"] = .01
            self.update_inspection(manifest, inspection)
            with self.subTest(field=field), self.assertRaises(forge.Invalid):
                forge.validate_asset(self.root, manifest, self.policy)

    def test_budgets_missing_checks_and_nan_rejected(self):
        for key, value in (("triangles", 85001), ("materials", 9), ("texture_edge", 4096),
                           ("texture_memory_mib", 181), ("texture_memory_mib", float("nan"))):
            manifest, inspection = self.asset_fixture()
            inspection["metrics"][key] = value
            self.update_inspection(manifest, inspection)
            with self.subTest(key=key, value=value), self.assertRaises(forge.Invalid):
                forge.validate_asset(self.root, manifest, self.policy)

    def test_failed_import_and_collision_rejected(self):
        for failure in ("warning", "collision"):
            manifest, inspection = self.asset_fixture()
            if failure == "warning":
                inspection["asset_processor"]["warnings"] = 1
            else:
                inspection["checks"]["collision"] = "UNVERIFIED"
            self.update_inspection(manifest, inspection)
            with self.subTest(failure=failure), self.assertRaises(forge.Invalid):
                forge.validate_asset(self.root, manifest, self.policy)

    def test_obj_triangle_mismatch_rejected(self):
        manifest, inspection = self.asset_fixture()
        inspection["metrics"]["triangles"] = inspection["lods"][0]["triangles"] = 4
        self.update_inspection(manifest, inspection)
        with self.assertRaisesRegex(forge.Invalid, "OBJ triangle count"):
            forge.validate_asset(self.root, manifest, self.policy)

    def test_missing_file_path_escape_and_symlink_rejected(self):
        for path in ("missing.obj", "../escape.obj", "/etc/passwd"):
            with self.subTest(path=path), self.assertRaises(forge.Invalid):
                forge.path_in(self.root, path)
        (self.root / "escape").symlink_to("/etc/passwd")
        with self.assertRaises(forge.Invalid):
            forge.path_in(self.root, "escape")

    def test_lfs_pointer_rejected(self):
        item = self.write("pointer.fbx", "version https://git-lfs.github.com/spec/v1\n")
        with self.assertRaisesRegex(forge.Invalid, "unhydrated"):
            forge.artifact(self.root, item)

    def test_scene_metrics_computed_from_samples(self):
        result = self.scene(self.scene_fixture())
        self.assertEqual(len(result["runs"]), 3)
        self.assertEqual(result["runs"][0]["frame_p99_ms"], 10)
        self.assertEqual(result["approval"], "NOT_GRANTED")

    def test_scene_budget_breach_rejected(self):
        evidence = self.scene_fixture()
        self.policy["scene_budget"]["gpu_p95_ms"] = 5
        with self.assertRaisesRegex(forge.Invalid, "scene budget exceeded"):
            self.scene(evidence)

    def test_wrong_revision_missing_players_and_effective_settings_rejected(self):
        evidence = self.scene_fixture()
        for key, value in (("revision", "b" * 40), ("connected_players", 2),
                           ("effective_settings", {}), ("policy_sha256", "0" * 64),
                           ("output_resolution", [1280, 720])):
            changed = copy.deepcopy(evidence)
            changed[key] = value
            with self.subTest(key=key), self.assertRaises(forge.Invalid):
                self.scene(changed)

    def test_duplicate_runs_rejected(self):
        evidence = self.scene_fixture()
        evidence["runs"] = [evidence["runs"][0]] * 3
        with self.assertRaisesRegex(forge.Invalid, "duplicate benchmark"):
            self.scene(evidence)

    def test_nan_missing_gpu_short_and_sparse_capture_rejected(self):
        evidence = self.scene_fixture()
        original = (self.root / evidence["runs"][0]["path"]).read_text()
        for variant in (original.replace(",6.0,", ",NaN,"), original.replace(",6.0,", ",0,"),
                        "\n".join(original.splitlines()[:30]), original.replace(",10,", ",1,")):
            evidence["runs"][0] = self.write("Build/VisualForge/run0.csv", variant)
            with self.subTest(variant=variant[:90]), self.assertRaises(forge.Invalid):
                self.scene(evidence)

    def test_empty_inventory_rejected(self):
        with self.assertRaisesRegex(forge.Invalid, "empty"):
            forge.audit(self.root, self.policy)

    def test_legacy_inventory_is_not_approval(self):
        self.write("Project/Assets/Weapons/old_rifle.obj", "v 0 0 0\n")
        result = forge.audit(self.root, self.policy)
        self.assertEqual(result["assets"][0]["naming"], "LEGACY_REQUIRES_MIGRATION")
        self.assertEqual(result["approval"], "NOT_GRANTED")

    def test_duplicate_json_keys_rejected(self):
        self.write("duplicate.json", '{"revision": 1, "revision": 2}')
        with self.assertRaisesRegex(forge.Invalid, "duplicate JSON"):
            forge.load(self.root / "duplicate.json")

    def test_promotion_requires_matching_human_reviews(self):
        manifest, _ = self.asset_fixture()
        evidence = self.scene_fixture()
        cid = forge.content_id(manifest["files"])
        evidence["asset_content_ids"] = [cid]
        manifest["reviews"] = {}
        for role in self.policy["required_reviews"]:
            manifest["reviews"][role] = {"decision": "APPROVED", "reviewer": "synthetic-reviewer",
                                         "revision": "a" * 40, "content_id": cid,
                                         "evidence": self.write(f"Build/VisualForge/{role}.txt", "synthetic review")}
        result = forge.promotion(self.root, manifest, evidence, self.policy, self.profiles, "a" * 40)
        self.assertEqual(result["approval"], "ELIGIBLE_FOR_REVIEWED_PROMOTION")
        manifest["reviews"]["art"]["revision"] = "b" * 40
        with self.assertRaisesRegex(forge.Invalid, "stale review"):
            forge.promotion(self.root, manifest, evidence, self.policy, self.profiles, "a" * 40)

    def test_bad_profile_contract_rejected(self):
        self.profiles["profiles"]["Quality_Low"]["resolution_scale"] = 0
        self.write("Config/VisualForge/quality_profiles.json", json.dumps(self.profiles))
        with self.assertRaises(forge.Invalid):
            forge.contracts(self.root)

    def test_material_missing_texture_is_rejected(self):
        item = self.write("Project/Assets/Materials/MI_Metal.material",
                          json.dumps({"propertyValues": {"baseColor.textureMap": "T_Missing.png"}}))
        with self.assertRaisesRegex(forge.Invalid, "dependency missing"):
            forge.material_references(self.root, self.root / item["path"], set())

    def test_material_relative_texture_in_closure_accepted(self):
        texture = self.write("Project/Assets/Materials/T_Metal_BaseColor_2K.png", "synthetic texture")
        item = self.write("Project/Assets/Materials/MI_Metal.material",
                          json.dumps({"propertyValues": {"baseColor": {"textureMap": "T_Metal_BaseColor_2K.png"}}}))
        forge.material_references(self.root, self.root / item["path"], {self.root / texture["path"]})

    def test_ci_changed_export_requires_manifest(self):
        result = SimpleNamespace(returncode=0, stdout="stw-o3de/Project/Assets/Weapons/SM_New.fbx\n", stderr="")
        with patch.object(forge.subprocess, "run", return_value=result):
            with self.assertRaisesRegex(forge.Invalid, "lack validated manifests"):
                forge.changed_assets(self.root, self.policy, "a" * 40)

    def test_ci_validates_changed_export(self):
        manifest, _ = self.asset_fixture()
        self.write("Config/VisualForge/Assets/rifle.json", json.dumps(manifest))
        result = SimpleNamespace(returncode=0, stdout="stw-o3de/" + manifest["primary"] + "\n", stderr="")
        with patch.object(forge.subprocess, "run", return_value=result):
            self.assertEqual(forge.changed_assets(self.root, self.policy, "a" * 40)["validated_manifests"], 1)


if __name__ == "__main__":
    unittest.main()
