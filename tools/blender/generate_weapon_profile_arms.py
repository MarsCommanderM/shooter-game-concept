"""Generate one first-person arm rig, four poses, and hip/ADS/reload frames per profile."""

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

PROFILES = (
    ("STW_SMG_01", 0.62, 0.07, (0.12, 0.16, 0.12)),
    ("STW_RIFLE_02", 0.98, 0.08, (0.10, 0.12, 0.16)),
    ("STW_RIFLE_03", 1.08, 0.075, (0.16, 0.12, 0.08)),
    ("STW_LMG_04", 1.22, 0.11, (0.08, 0.09, 0.08)),
    ("STW_SIDEARM_01", 0.38, 0.05, (0.18, 0.14, 0.10)),
    ("STW_LAUNCHER_01", 1.15, 0.14, (0.20, 0.16, 0.08)),
    ("STW_TACTICAL_FLASH_01", 0.28, 0.05, (0.22, 0.20, 0.10)),
    ("STW_TACTICAL_SMOKE_01", 0.30, 0.055, (0.14, 0.16, 0.14)),
    ("STW_LETHAL_FRAG_01", 0.22, 0.07, (0.24, 0.12, 0.08)),
    ("STW_MELEE_01", 0.74, 0.03, (0.16, 0.15, 0.14)),
)


def arguments():
    values = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args(values)


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.armatures, bpy.data.actions):
        for item in list(block):
            block.remove(item)


def material(name, color):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1.0)
    value.use_nodes = True
    bsdf = value.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.45
    return value


def cube(name, location, scale, mat):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.name = name
    obj.data.materials.append(mat)
    if not obj.data.uv_layers:
        obj.data.uv_layers.new(name="UVMap")
    return obj


def make_armature(name):
    data = bpy.data.armatures.new(f"{name}_Skeleton")
    armature = bpy.data.objects.new(f"{name}_Rig", data)
    bpy.context.collection.objects.link(armature)
    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    definitions = {
        "root": ((0.0, 0.0, 0.2), (0.0, 0.0, 0.45), None),
        "weapon": ((0.0, 0.0, 0.45), (0.0, 0.35, 0.45), "root"),
        "upperarm_L": ((-0.46, -0.12, 0.62), (-0.30, 0.10, 0.58), "root"),
        "forearm_L": ((-0.30, 0.10, 0.58), (-0.16, 0.36, 0.54), "upperarm_L"),
        "hand_L": ((-0.16, 0.36, 0.54), (-0.08, 0.48, 0.52), "forearm_L"),
        "upperarm_R": ((0.46, -0.12, 0.62), (0.30, 0.10, 0.58), "root"),
        "forearm_R": ((0.30, 0.10, 0.58), (0.16, 0.36, 0.54), "upperarm_R"),
        "hand_R": ((0.16, 0.36, 0.54), (0.08, 0.48, 0.52), "forearm_R"),
    }
    bones = {}
    for bone_name, (head, tail, parent) in definitions.items():
        bone = data.edit_bones.new(bone_name)
        bone.head = head
        bone.tail = tail
        if parent:
            bone.parent = bones[parent]
        bones[bone_name] = bone
    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


def skin(obj, armature, bone_name):
    group = obj.vertex_groups.new(name=bone_name)
    group.add(range(len(obj.data.vertices)), 1.0, "REPLACE")
    modifier = obj.modifiers.new("ArmatureDeform", "ARMATURE")
    modifier.object = armature
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.matrix_world = world


def build_profile(name, length, thickness, color):
    clear_scene()
    skin_mat = material(f"{name}_Skin", (0.34, 0.16, 0.10))
    glove = material(f"{name}_Glove", (0.04, 0.05, 0.06))
    weapon_mat = material(f"{name}_Weapon", color)
    armature = make_armature(name)
    meshes = []
    for side, sign in (("L", -1.0), ("R", 1.0)):
        upper = cube(f"{name}_Upper_{side}", (0.38 * sign, -0.02, 0.60), (0.06, 0.14, 0.06), skin_mat)
        fore = cube(f"{name}_Fore_{side}", (0.22 * sign, 0.22, 0.56), (0.05, 0.14, 0.05), skin_mat)
        hand = cube(f"{name}_Hand_{side}", (0.12 * sign, 0.42, 0.53), (0.05, 0.06, 0.04), glove)
        skin(upper, armature, f"upperarm_{side}")
        skin(fore, armature, f"forearm_{side}")
        skin(hand, armature, f"hand_{side}")
        meshes.extend((upper, fore, hand))
    body = cube(f"{name}_Body", (0.0, 0.25 + length * 0.15, 0.48), (thickness, length * 0.35, thickness), weapon_mat)
    skin(body, armature, "weapon")
    meshes.append(body)
    return armature, meshes


def reset_pose(armature):
    for bone in armature.pose.bones:
        bone.rotation_mode = "XYZ"
        bone.rotation_euler = (0.0, 0.0, 0.0)
        bone.location = (0.0, 0.0, 0.0)


def key_pose(armature, frame):
    for bone_name in ("weapon", "upperarm_L", "forearm_L", "hand_L", "upperarm_R", "forearm_R", "hand_R"):
        bone = armature.pose.bones[bone_name]
        bone.keyframe_insert("rotation_euler", frame=frame)
        bone.keyframe_insert("location", frame=frame)


def make_actions(armature, scale):
    armature.animation_data_create()
    actions = []
    ends = {"hip": 24, "ads": 18, "reload": 36, "inspect": 28}
    for name, end in ends.items():
        action = bpy.data.actions.new(name)
        armature.animation_data.action = action
        reset_pose(armature)
        key_pose(armature, 1)
        if name == "hip":
            armature.pose.bones["weapon"].rotation_euler.x = math.radians(2.0 * scale)
        elif name == "ads":
            armature.pose.bones["weapon"].location.z = 0.08 * scale
            armature.pose.bones["weapon"].location.y = -0.04 * scale
        elif name == "reload":
            armature.pose.bones["weapon"].rotation_euler.y = math.radians(-18.0 * scale)
            armature.pose.bones["hand_L"].location = (-0.06, -0.12 * scale, -0.16)
        else:
            armature.pose.bones["weapon"].rotation_euler.z = math.radians(22.0 * scale)
            armature.pose.bones["hand_R"].location = (0.04, -0.08, 0.10 * scale)
        key_pose(armature, end)
        action.frame_start = 1
        action.frame_end = end
        actions.append(action)
    return {action.name: action for action in actions}


def export_fbx(path, armature, meshes, action):
    armature.animation_data.action = action
    bpy.context.scene.frame_start = int(action.frame_start)
    bpy.context.scene.frame_end = int(action.frame_end)
    bpy.context.scene.frame_set(int(action.frame_end))
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.export_scene.fbx(
        filepath=str(path),
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        add_leaf_bones=False,
        bake_anim=True,
        bake_anim_use_all_actions=False,
        path_mode="AUTO",
        axis_forward="-Y",
        axis_up="Z",
    )


def render_pose(path, armature, action):
    armature.animation_data.action = action
    bpy.context.scene.frame_set(int(action.frame_end))
    bpy.context.view_layer.update()
    scene = bpy.context.scene
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)


def prepare_render():
    bpy.ops.object.camera_add(location=(2.4, -3.4, 1.8))
    camera = bpy.context.object
    camera.rotation_euler = (Vector((0.0, 0.4, 0.55)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = camera
    bpy.ops.object.light_add(type="AREA", location=(1.5, -1.2, 2.4))
    bpy.context.object.data.energy = 400
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 320
    scene.render.resolution_y = 180
    scene.render.image_settings.file_format = "PNG"


def main():
    args = arguments()
    profile_root = args.output / "profiles"
    frame_root = args.output / "frames"
    profile_root.mkdir(parents=True, exist_ok=True)
    frame_root.mkdir(parents=True, exist_ok=True)
    for name, length, thickness, color in PROFILES:
        armature, meshes = build_profile(name, length, thickness, color)
        prepare_render()
        actions = make_actions(armature, length)
        folder = profile_root / name
        folder.mkdir(parents=True, exist_ok=True)
        export_fbx(folder / f"{name}.fbx", armature, meshes, actions["hip"])
        for pose in ("hip", "ads", "reload", "inspect"):
            export_fbx(folder / f"{name}_{pose}.fbx", armature, meshes, actions[pose])
        for pose in ("hip", "ads", "reload"):
            render_pose(frame_root / f"{name}_{pose}.png", armature, actions[pose])
        print(f"STW_PROFILE_ARMS_EXPORTED {name}", flush=True)


if __name__ == "__main__":
    main()
