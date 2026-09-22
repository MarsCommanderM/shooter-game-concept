"""Generate original STW first-person arms, gloves, rifle, rig, clips and preview."""

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def arguments():
    values = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args(values)


def material(name, color, metallic, roughness):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1.0)
    value.use_nodes = True
    bsdf = value.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return value


def finish_mesh(obj, name, mat, bevel=0.015):
    obj.name = name
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    if bevel:
        modifier = obj.modifiers.new("ProductionBevel", "BEVEL")
        modifier.width = bevel
        modifier.segments = 3
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def cube(name, location, scale, mat, bevel=0.02):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.scale = scale
    return finish_mesh(obj, name, mat, bevel)


def cylinder(name, location, radius, depth, mat, rotation=(0.0, 0.0, 0.0), vertices=32):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=rotation)
    return finish_mesh(bpy.context.object, name, mat, 0.012)


def cylinder_between(name, start, end, radius, mat, vertices=32):
    start = Vector(start)
    end = Vector(end)
    direction = end - start
    obj = cylinder(name, (start + end) * 0.5, radius, direction.length, mat, vertices=vertices)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    return obj


def sphere(name, location, scale, mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, location=location)
    obj = bpy.context.object
    obj.scale = scale
    return finish_mesh(obj, name, mat, 0.0)


def make_armature():
    data = bpy.data.armatures.new("STW_FP_01_Skeleton")
    armature = bpy.data.objects.new("STW_FP_01_Rig", data)
    bpy.context.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    definitions = {
        "root": ((0.0, 0.0, 0.2), (0.0, 0.0, 0.5), None),
        "weapon": ((0.0, 0.0, 0.5), (0.0, 0.35, 0.5), "root"),
        "upperarm_L": ((-0.58, -0.18, 0.72), (-0.42, 0.12, 0.70), "root"),
        "forearm_L": ((-0.42, 0.12, 0.70), (-0.24, 0.48, 0.68), "upperarm_L"),
        "hand_L": ((-0.24, 0.48, 0.68), (-0.16, 0.67, 0.67), "forearm_L"),
        "upperarm_R": ((0.58, -0.18, 0.72), (0.42, 0.12, 0.70), "root"),
        "forearm_R": ((0.42, 0.12, 0.70), (0.24, 0.48, 0.68), "upperarm_R"),
        "hand_R": ((0.24, 0.48, 0.68), (0.16, 0.67, 0.67), "forearm_R"),
    }
    bones = {}
    for name, (head, tail, parent) in definitions.items():
        bone = data.edit_bones.new(name)
        bone.head = head
        bone.tail = tail
        if parent:
            bone.parent = bones[parent]
        bones[name] = bone
    bpy.ops.object.mode_set(mode="OBJECT")
    armature.show_in_front = True
    return armature, list(definitions)


def skin_to_bone(obj, armature, bone):
    group = obj.vertex_groups.new(name=bone)
    group.add(range(len(obj.data.vertices)), 1.0, "REPLACE")
    modifier = obj.modifiers.new("ArmatureDeform", "ARMATURE")
    modifier.object = armature
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.matrix_world = world


def build_arms(armature, skin, glove):
    meshes = []
    for side, sign in (("L", -1.0), ("R", 1.0)):
        shoulder = (0.58 * sign, -0.18, 0.72)
        elbow = (0.42 * sign, 0.12, 0.70)
        wrist = (0.24 * sign, 0.48, 0.68)
        upper = cylinder_between(f"FP_UpperArm_{side}", shoulder, elbow, 0.115, skin)
        forearm = cylinder_between(f"FP_Forearm_{side}", elbow, wrist, 0.105, skin)
        hand = sphere(f"FP_Glove_{side}", (0.19 * sign, 0.57, 0.68), (0.12, 0.18, 0.09), glove)
        cuff = cylinder_between(f"FP_Cuff_{side}", wrist, (0.20 * sign, 0.56, 0.68), 0.125, glove)
        for obj, bone in ((upper, f"upperarm_{side}"), (forearm, f"forearm_{side}"), (hand, f"hand_{side}"), (cuff, f"hand_{side}")):
            skin_to_bone(obj, armature, bone)
            meshes.append(obj)
        for index, y in enumerate((0.55, 0.61, 0.67)):
            plate = cube(
                f"FP_KnucklePlate_{side}_{index}",
                (0.19 * sign, y, 0.755),
                (0.035, 0.022, 0.012),
                glove,
                bevel=0.006,
            )
            skin_to_bone(plate, armature, f"hand_{side}")
            meshes.append(plate)
        for index, y in enumerate((0.58, 0.66)):
            finger_channel = cylinder(
                f"FP_GloveFingerChannel_{side}_{index}",
                (0.19 * sign, y, 0.69),
                0.018,
                0.095,
                skin,
                rotation=(math.radians(90), 0.0, 0.0),
                vertices=16,
            )
            skin_to_bone(finger_channel, armature, f"hand_{side}")
            meshes.append(finger_channel)
        seam = cylinder_between(
            f"FP_GloveCuffSeam_{side}",
            (0.20 * sign, 0.515, 0.68),
            (0.20 * sign, 0.565, 0.68),
            0.132,
            glove,
            vertices=24,
        )
        skin_to_bone(seam, armature, f"hand_{side}")
        meshes.append(seam)
    return meshes


def build_rifle(armature, steel, polymer, accent):
    parts = [
        cube("Rifle_Receiver", (0.0, 0.63, 0.78), (0.13, 0.34, 0.11), steel),
        cube("Rifle_Handguard", (0.0, 1.00, 0.79), (0.11, 0.30, 0.09), polymer),
        cylinder("Rifle_Barrel", (0.0, 1.42, 0.80), 0.035, 0.58, steel, rotation=(math.radians(90), 0.0, 0.0), vertices=24),
        cylinder("Rifle_Muzzle", (0.0, 1.72, 0.80), 0.055, 0.16, steel, rotation=(math.radians(90), 0.0, 0.0), vertices=24),
        cube("Rifle_Stock", (0.0, 0.22, 0.77), (0.12, 0.28, 0.13), polymer),
        cube("Rifle_Grip", (0.0, 0.48, 0.58), (0.07, 0.10, 0.18), polymer),
        cube("Rifle_Magazine", (0.0, 0.72, 0.53), (0.08, 0.13, 0.21), accent),
        cube("Rifle_Optic", (0.0, 0.67, 0.96), (0.07, 0.12, 0.07), steel),
        cylinder("Rifle_OpticGlass", (0.0, 0.80, 0.96), 0.052, 0.025, accent, rotation=(math.radians(90), 0.0, 0.0), vertices=32),
    ]
    detail_parts = []

    # Continuous top rail broken into machined segments for a readable
    # first-person silhouette and physically plausible attachment points.
    for index, y in enumerate((0.76, 0.84, 0.92, 1.00, 1.08, 1.16, 1.24)):
        detail_parts.append(cube(f"Rifle_TopRail_{index:02d}", (0.0, y, 0.895), (0.052, 0.030, 0.012), steel, 0.006))
    for side, sign in (("L", -1.0), ("R", 1.0)):
        for index, y in enumerate((0.84, 0.98, 1.12, 1.26)):
            detail_parts.append(cube(f"Rifle_MLOKVent_{side}_{index:02d}", (0.108 * sign, y, 0.79), (0.008, 0.045, 0.026), polymer, 0.004))
        for index, y in enumerate((0.52, 0.69)):
            detail_parts.append(cylinder(f"Rifle_ReceiverPin_{side}_{index}", (0.137 * sign, y, 0.78), 0.022, 0.018, steel, rotation=(0.0, math.radians(90), 0.0), vertices=20))

    detail_parts.extend(
        [
            cube("Rifle_BoltCarrier", (0.0, 0.57, 0.825), (0.06, 0.11, 0.028), steel, 0.008),
            cube("Rifle_ChargingHandle", (0.0, 0.48, 0.92), (0.045, 0.028, 0.014), accent, 0.006),
            cube("Rifle_Trigger", (0.0, 0.47, 0.425), (0.022, 0.050, 0.014), accent, 0.006),
            cube("Rifle_TriggerGuard", (0.0, 0.47, 0.465), (0.055, 0.075, 0.012), polymer, 0.008),
            cube("Rifle_StockCheekPad", (0.0, 0.04, 0.88), (0.095, 0.085, 0.025), polymer, 0.010),
            cube("Rifle_StockButtPad", (0.0, -0.065, 0.77), (0.125, 0.016, 0.10), accent, 0.008),
        ]
    )
    for index, y in enumerate((0.62, 0.70, 0.78)):
        detail_parts.append(cube(f"Rifle_MagazineRib_{index:02d}", (0.083, y, 0.53), (0.008, 0.018, 0.14), polymer, 0.004))
    for name, y in (("Front", 0.57), ("Rear", 0.88)):
        detail_parts.append(cylinder(f"Rifle_OpticRing_{name}", (0.0, y, 0.96), 0.066, 0.018, steel, rotation=(math.radians(90), 0.0, 0.0), vertices=32))
    detail_parts.extend(
        [
            cylinder("Rifle_OpticDial", (0.0, 0.72, 1.035), 0.018, 0.022, accent, vertices=20),
            cube("Rifle_OpticMount", (0.0, 0.70, 0.895), (0.052, 0.095, 0.018), steel, 0.006),
            cylinder("Rifle_MuzzleRing_A", (0.0, 1.56, 0.80), 0.064, 0.025, steel, rotation=(math.radians(90), 0.0, 0.0), vertices=28),
            cylinder("Rifle_MuzzleRing_B", (0.0, 1.68, 0.80), 0.065, 0.025, accent, rotation=(math.radians(90), 0.0, 0.0), vertices=28),
            cube("Rifle_MuzzlePort_L", (-0.052, 1.63, 0.80), (0.010, 0.025, 0.018), accent, 0.004),
            cube("Rifle_MuzzlePort_R", (0.052, 1.63, 0.80), (0.010, 0.025, 0.018), accent, 0.004),
        ]
    )
    parts.extend(detail_parts)
    for obj in parts:
        skin_to_bone(obj, armature, "weapon")
    return parts


def reset_pose(armature):
    for bone in armature.pose.bones:
        bone.rotation_mode = "XYZ"
        bone.rotation_euler = (0.0, 0.0, 0.0)
        bone.location = (0.0, 0.0, 0.0)


def key_bones(armature, frame):
    for name in ("weapon", "upperarm_L", "forearm_L", "hand_L", "upperarm_R", "forearm_R", "hand_R"):
        bone = armature.pose.bones[name]
        bone.keyframe_insert("rotation_euler", frame=frame, group=name)
        bone.keyframe_insert("location", frame=frame, group=name)


def make_actions(armature):
    armature.animation_data_create()
    actions = []
    for name, end in (("idle", 60), ("ads", 24), ("reload", 72)):
        action = bpy.data.actions.new(name)
        armature.animation_data.action = action
        reset_pose(armature)
        key_bones(armature, 1)
        if name == "idle":
            armature.pose.bones["weapon"].rotation_euler.x = math.radians(1.2)
            armature.pose.bones["forearm_L"].rotation_euler.y = math.radians(-1.5)
            key_bones(armature, 30)
            reset_pose(armature)
        elif name == "ads":
            armature.pose.bones["weapon"].location.z = 0.085
            armature.pose.bones["weapon"].location.y = -0.05
            armature.pose.bones["upperarm_L"].rotation_euler.z = math.radians(-5)
            armature.pose.bones["upperarm_R"].rotation_euler.z = math.radians(5)
        else:
            armature.pose.bones["weapon"].rotation_euler.y = math.radians(-16)
            armature.pose.bones["hand_L"].location = (-0.08, -0.16, -0.22)
            armature.pose.bones["forearm_L"].rotation_euler.x = math.radians(18)
            key_bones(armature, 36)
            armature.pose.bones["hand_L"].location = (0.0, 0.0, 0.0)
            armature.pose.bones["weapon"].rotation_euler.y = 0.0
        key_bones(armature, end)
        action.frame_start = 1
        action.frame_end = end
        actions.append(action)
    armature.animation_data.action = actions[0]
    reset_pose(armature)
    return actions


def point_camera(camera, target):
    camera.rotation_euler = (Vector(target) - camera.location).to_track_quat("-Z", "Y").to_euler()


def preview(output, meshes):
    bpy.ops.object.camera_add(location=(3.1, -4.8, 2.8))
    camera = bpy.context.object
    point_camera(camera, (0.0, 0.65, 0.70))
    camera.data.lens = 58
    bpy.context.scene.camera = camera
    bpy.ops.object.light_add(type="AREA", location=(2.0, -1.5, 4.0))
    bpy.context.object.data.energy = 1100
    bpy.context.object.data.shape = "DISK"
    bpy.context.object.data.size = 4.0
    bpy.ops.object.light_add(type="AREA", location=(-2.5, 1.5, 2.2))
    bpy.context.object.data.energy = 700
    bpy.context.object.data.color = (0.25, 0.45, 1.0)
    bpy.context.object.data.size = 3.0
    bpy.ops.mesh.primitive_plane_add(size=20, location=(0.0, 0.5, 0.0))
    floor = bpy.context.object
    floor.data.materials.append(material("PreviewFloor", (0.025, 0.03, 0.035), 0.0, 0.3))
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(output / "STW_FP_01_preview.png")
    scene.world.color = (0.008, 0.012, 0.02)
    bpy.ops.render.render(write_still=True)


def export_action_fbx(output, armature, meshes, action, filename):
    """Export one O3DE-safe animation source per action.

    O3DE's SceneAPI accepts one animation per bone in a source FBX.  The
    editable Blender file still contains every action; exchange files are
    deliberately split so AssetProcessor receives one unambiguous clip per
    source and cannot silently discard or invalidate animation data.
    """
    armature.animation_data.action = action
    bpy.context.scene.frame_start = int(action.frame_start)
    bpy.context.scene.frame_end = int(action.frame_end)
    bpy.context.scene.frame_set(int(action.frame_start))

    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.export_scene.fbx(
        filepath=str(output / filename),
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        add_leaf_bones=False,
        bake_anim=True,
        bake_anim_use_all_actions=False,
        path_mode="AUTO",
        axis_forward="-Y",
        axis_up="Z",
    )


def main():
    args = arguments()
    args.output.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    skin = material("M_FP_Skin", (0.32, 0.12, 0.07), 0.0, 0.48)
    glove = material("M_FP_Glove", (0.025, 0.035, 0.045), 0.0, 0.62)
    steel = material("M_Rifle_Steel", (0.055, 0.065, 0.075), 0.82, 0.25)
    polymer = material("M_Rifle_Polymer", (0.055, 0.07, 0.06), 0.0, 0.50)
    accent = material("M_Rifle_Accent", (0.45, 0.08, 0.025), 0.55, 0.28)
    armature, required_bones = make_armature()
    meshes = build_arms(armature, skin, glove) + build_rifle(armature, steel, polymer, accent)
    actions = make_actions(armature)

    action_by_name = {action.name: action for action in actions}
    armature.animation_data.action = action_by_name["idle"]
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 72
    bpy.ops.wm.save_as_mainfile(filepath=str(args.output / "STW_FP_01.blend"))

    animation_exports = {
        "idle": "STW_FP_01.fbx",
        "ads": "STW_FP_01_ads.fbx",
        "reload": "STW_FP_01_reload.fbx",
    }
    for action_name, filename in animation_exports.items():
        export_action_fbx(args.output, armature, meshes, action_by_name[action_name], filename)
    armature.animation_data.action = action_by_name["idle"]
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = 72
    bpy.context.scene.frame_set(1)
    preview(args.output, meshes)

    evaluated = bpy.context.evaluated_depsgraph_get()
    vertices = 0
    triangles = 0
    for obj in meshes:
        mesh = obj.evaluated_get(evaluated).to_mesh()
        mesh.calc_loop_triangles()
        vertices += len(mesh.vertices)
        triangles += len(mesh.loop_triangles)
        obj.evaluated_get(evaluated).to_mesh_clear()
    world_points = [obj.matrix_world @ Vector(corner) for obj in meshes for corner in obj.bound_box]
    minimum = [min(point[axis] for point in world_points) for axis in range(3)]
    maximum = [max(point[axis] for point in world_points) for axis in range(3)]
    grip = bpy.data.objects["Rifle_Grip"].matrix_world.translation
    glove_centers = [bpy.data.objects[f"FP_Glove_{side}"].matrix_world.translation for side in ("L", "R")]
    skinned = [obj for obj in meshes if obj.name.startswith("FP_")]
    report = {
        "asset_family": "STW_FP_01",
        "generator": "Blender 4.5.14 LTS",
        "mesh_objects": len(meshes),
        "weapon_detail_objects": sum(1 for obj in meshes if obj.name.startswith("Rifle_") and obj.name not in {"Rifle_Receiver", "Rifle_Handguard", "Rifle_Barrel", "Rifle_Muzzle", "Rifle_Stock", "Rifle_Grip", "Rifle_Magazine", "Rifle_Optic", "Rifle_OpticGlass"}),
        "glove_detail_objects": sum(1 for obj in meshes if obj.name.startswith("FP_KnucklePlate_") or obj.name.startswith("FP_GloveFingerChannel_") or obj.name.startswith("FP_GloveCuffSeam_")),
        "vertices": vertices,
        "triangles": triangles,
        "required_bones": required_bones,
        "actions": sorted(action.name for action in actions),
        "animation_exports": animation_exports,
        "all_meshes_have_uvs": all(bool(obj.data.uv_layers) for obj in meshes),
        "all_skinned_meshes_have_armature_modifier": all(
            any(mod.type == "ARMATURE" and mod.object == armature for mod in obj.modifiers)
            for obj in skinned
        ),
        "material_slots": len({slot.material.name for obj in meshes for slot in obj.material_slots}),
        "binding_pose_bounds": {"minimum": minimum, "maximum": maximum},
        "hand_to_grip_distance_max": max((center - grip).length for center in glove_centers),
        "coordinate_contract": "Z-up, forward -Y on FBX export",
        "quality_tier": "high_end_procedural_foundation",
        "quality_status": "source-generated; visual and in-engine playback require review",
    }
    (args.output / "STW_FP_01.report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
