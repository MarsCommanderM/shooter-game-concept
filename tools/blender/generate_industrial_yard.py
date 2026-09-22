"""Generate the original STW_INDUSTRIAL_YARD_01 source and nine FBX groups."""

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


PIECES = [
    ("wet_concrete_deck", "deck", (0, 0, 0.005), (24, 24, 0.01), "concrete"),
    ("north_brick_facade", "wall", (0, 12, 2), (23.95, 0.45, 3.95), "brick"),
    ("south_concrete_facade", "wall", (0, -12, 2), (23.95, 0.45, 3.95), "concrete"),
    ("east_steel_facade", "wall", (12, 0, 2), (0.45, 23.95, 3.95), "steel"),
    ("west_steel_facade", "wall", (-12, 0, 2), (0.45, 23.95, 3.95), "steel"),
    ("left_cover_cladding", "cover", (-2.25, 0, 1.25), (1.5, 2, 2.5), "steel"),
    ("right_cover_cladding", "cover", (2.25, 0, 1.25), (1.5, 2, 2.5), "steel"),
    ("service_step_skin", "cover", (5, -2, 0.125), (2, 2, 0.25), "concrete"),
    ("suspended_gallery", "arch", (0, 6, 4.45), (8, 1, 0.5), "steel"),
    ("west_signage", "props", (-12.7, 2, 2.2), (0.5, 2, 1.5), "sign"),
    ("shallow_puddles", "mark", (0, -6, 0.006), (5, 2, 0.008), "water"),
    ("roof_service_truss", "struct", (0, 2, 4.45), (10, 0.6, 0.5), "steel"),
    ("outside_factory_tower", "beacon", (-13, 5, 2.5), (0.8, 1.5, 3), "steel"),
    ("flush_hazard_trim", "trim", (6, -6, 0.005), (4, 2, 0.01), "hazard"),
]


def args():
    values = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--skip-preview", action="store_true")
    return parser.parse_args(values)


def mat(name, color, metallic, roughness):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1)
    value.use_nodes = True
    bsdf = value.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return value


def look_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def render_preview(output):
    bpy.ops.object.camera_add(location=(19, -23, 15))
    camera = bpy.context.object
    look_at(camera, (0, 0, 1.4))
    camera.data.lens = 34
    bpy.context.scene.camera = camera
    bpy.ops.object.light_add(type="SUN", location=(4, -6, 12))
    bpy.context.object.rotation_euler = (0.55, -0.45, -0.35)
    bpy.context.object.data.energy = 4.5
    bpy.context.object.data.color = (1.0, 0.63, 0.38)
    bpy.ops.object.light_add(type="AREA", location=(-4, -1, 8))
    bpy.context.object.data.energy = 3200
    bpy.context.object.data.color = (0.22, 0.42, 1.0)
    bpy.context.object.data.size = 12
    look_at(bpy.context.object, (0.0, 0.0, 0.0))
    bpy.ops.object.light_add(type="AREA", location=(10, -10, 6))
    bpy.context.object.data.energy = 2200
    bpy.context.object.data.color = (1.0, 0.34, 0.12)
    bpy.context.object.data.size = 7
    look_at(bpy.context.object, (4.0, -2.0, 0.8))
    bpy.ops.object.light_add(type="AREA", location=(-8, 8, 10))
    bpy.context.object.data.energy = 2400
    bpy.context.object.data.color = (0.18, 0.35, 1.0)
    bpy.context.object.data.size = 8
    look_at(bpy.context.object, (-2.0, 4.0, 1.8))
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(output / "STW_INDUSTRIAL_YARD_01_preview.png")
    scene.render.film_transparent = False
    scene.world.color = (0.012, 0.018, 0.035)
    scene.view_settings.exposure = 1.15
    bpy.ops.render.render(write_still=True)


def detail_cube(name, center, size, material, bevel=0.012):
    bpy.ops.mesh.primitive_cube_add(location=center)
    obj = bpy.context.object
    obj.name = name
    obj.scale = tuple(value * 0.5 for value in size)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    if bevel:
        modifier = obj.modifiers.new("ProductionEdgeBevel", "BEVEL")
        modifier.width = min(bevel, min(size) * 0.2)
        modifier.segments = 2
    return obj


def detail_cylinder(name, center, radius, depth, material, vertices=12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    bevel = obj.modifiers.new("ProductionEdgeBevel", "BEVEL")
    bevel.width = min(0.008, radius * 0.2)
    bevel.segments = 2
    return obj


def detail_cylinder_between(name, start, end, radius, material, vertices=12):
    start = Vector(start)
    end = Vector(end)
    direction = end - start
    obj = detail_cylinder(name, (start + end) * 0.5, radius, direction.length, material, vertices)
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    return obj


def add_high_detail(groups, materials):
    def add(group, obj):
        groups[group].append(obj)

    # Deck: drains, expansion joints and flush lane strips stay within the
    # floor plane and never create a gameplay obstacle.
    for index, x in enumerate((-9.0, -6.0, -3.0, 3.0, 6.0, 9.0)):
        add("deck", detail_cube(f"IY_DeckDrain_{index:02d}", (x, -10.8, 0.012), (0.42, 0.18, 0.012), materials["steel"], 0.004))
        add("deck", detail_cube(f"IY_DeckDrainTop_{index:02d}", (x, -10.8, 0.020), (0.30, 0.05, 0.008), materials["water"], 0.002))
    for index, y in enumerate((-8.0, -4.0, 0.0, 4.0, 8.0)):
        add("deck", detail_cube(f"IY_DeckExpansion_{index:02d}", (0.0, y, 0.011), (22.0, 0.035, 0.006), materials["hazard"], 0.001))

    # Facades: repeating panel ribs, service bands and inset maintenance plates.
    for side, y in (("North", 11.76), ("South", -11.76)):
        for index, x in enumerate((-10.0, -7.5, -5.0, -2.5, 0.0, 2.5, 5.0, 7.5, 10.0)):
            add("wall", detail_cube(f"IY_{side}Rib_{index:02d}", (x, y, 2.0), (0.12, 0.045, 3.70), materials["steel"], 0.008))
        for index, z in enumerate((0.55, 1.95, 3.35)):
            add("wall", detail_cube(f"IY_{side}Band_{index:02d}", (0.0, y, z), (23.4, 0.04, 0.07), materials["hazard"], 0.005))
    for side, x in (("East", 11.76), ("West", -11.76)):
        for index, y in enumerate((-10.0, -7.5, -5.0, -2.5, 0.0, 2.5, 5.0, 7.5, 10.0)):
            add("wall", detail_cube(f"IY_{side}Rib_{index:02d}", (x, y, 2.0), (0.045, 0.12, 3.70), materials["steel"], 0.008))

    # Cover cladding: corner caps and visible fasteners remain inside the
    # existing left/right cover collision volumes.
    for side, x in (("Left", -2.25), ("Right", 2.25)):
        for index, y in enumerate((-0.72, 0.72)):
            for z in (0.45, 2.05):
                add("cover", detail_cube(f"IY_{side}CoverPlate_{index}_{int(z * 10)}", (x, y, z), (1.30, 0.06, 0.32), materials["hazard"], 0.006))
                add("cover", detail_cylinder(f"IY_{side}Bolt_{index}_{int(z * 10)}", (x + (0.68 if side == "Right" else -0.68), y, z), 0.035, 0.025, materials["steel"], 12))

    # Suspended gallery and roof truss: repeated braces are above the playable
    # height and carry no physics authority.
    for index, x in enumerate((-3.2, -1.6, 0.0, 1.6, 3.2)):
        add("arch", detail_cube(f"IY_GalleryBrace_{index:02d}", (x, 6.0, 4.45), (0.10, 0.82, 0.38), materials["steel"], 0.006))
    for index, x in enumerate((-4.0, -2.0, 0.0, 2.0, 4.0)):
        add("struct", detail_cube(f"IY_TrussVertical_{index:02d}", (x, 2.0, 4.45), (0.12, 0.48, 0.44), materials["steel"], 0.006))
        add("struct", detail_cube(f"IY_TrussCap_{index:02d}", (x, 2.0, 4.71), (0.34, 0.58, 0.06), materials["hazard"], 0.004))

    # Exterior signage and beacon details are beyond the west gameplay wall.
    for index, z in enumerate((1.35, 2.05, 2.75)):
        add("props", detail_cube(f"IY_SignFrame_{index:02d}", (-12.73, 2.0, z), (0.08, 2.15, 0.08), materials["steel"], 0.004))
    for index, z in enumerate((1.4, 2.1, 2.8)):
        add("beacon", detail_cylinder(f"IY_BeaconRing_{index:02d}", (-13.0, 5.0, z), 0.46, 0.08, materials["hazard"], 16))
    add("beacon", detail_cylinder("IY_BeaconAntenna", (-13.0, 5.0, 4.15), 0.06, 0.90, materials["steel"], 12))

    # Flush hazard trim is a non-blocking surface graphic built from repeated
    # strips, not raised geometry.
    for index, x in enumerate((4.6, 5.2, 5.8, 6.4, 7.0)):
        strip = detail_cube(f"IY_HazardStripe_{index:02d}", (x, -6.0, 0.013), (0.22, 1.7, 0.006), materials["hazard"], 0.001)
        add("trim", strip)

    # Non-authoritative industrial dressing: service conduits, panel doors,
    # catwalk rails, cable trays, pallets, crates and drums. These objects are
    # exported as visual-only meshes and must not acquire gameplay collision.
    for side, y in (("North", 11.48), ("South", -11.48)):
        for index, z in enumerate((0.82, 1.18, 1.54)):
            add("wall", detail_cube(f"IY_{side}ServicePipe_{index:02d}", (0.0, y, z), (10.6, 0.055, 0.055), materials["steel"], 0.012))
        for index, x in enumerate((-9.5, -6.0, 6.0, 9.5)):
            add("wall", detail_cube(f"IY_{side}ServicePipeVertical_{index:02d}", (x, y, 1.15), (0.055, 0.055, 0.82), materials["steel"], 0.012))
        for index, x in enumerate((-7.2, 0.0, 7.2)):
            add("wall", detail_cube(f"IY_{side}AccessPanel_{index:02d}", (x, y + (0.035 if side == "North" else -0.035), 1.65), (0.92, 0.025, 0.58), materials["facade" if "facade" in materials else "brick"], 0.025))

    for side, y in (("Front", 5.42), ("Rear", 6.58)):
        add("arch", detail_cube(f"IY_GalleryRail_{side}", (0.0, y, 4.98), (3.85, 0.055, 0.055), materials["steel"], 0.012))
        for index, x in enumerate((-3.5, -1.75, 0.0, 1.75, 3.5)):
            add("arch", detail_cube(f"IY_GalleryPost_{side}_{index:02d}", (x, y, 4.72), (0.045, 0.045, 0.28), materials["steel"], 0.010))
    for index, x in enumerate((-4.0, -2.0, 0.0, 2.0, 4.0)):
        add("struct", detail_cube(f"IY_CableTray_{index:02d}", (x, 1.25, 4.78), (0.72, 0.08, 0.045), materials["steel"], 0.012))

    # No free-standing pallets, crates or drums inside the walls: the arena has
    # exactly eight PhysX colliders shared with STW_ARENA_01, and solid dressing in
    # the playable space without a matching collider could be walked through. Props
    # stay on the exterior signage, which is beyond the west wall.

    for index, z in enumerate((0.9, 1.35, 1.8, 2.25, 2.7, 3.15)):
        add("beacon", detail_cylinder_between(f"IY_BeaconLadder_{index:02d}", (-13.44, 5.0, z), (-12.56, 5.0, z), 0.026, materials["steel"], 12))


def write_material_sources(output, materials):
    material_dir = output / "Materials"
    texture_dir = output / "Textures"
    material_dir.mkdir(parents=True, exist_ok=True)
    texture_dir.mkdir(parents=True, exist_ok=True)
    definitions = {
        "concrete": ((0.16, 0.17, 0.18, 1.0), 0.0, 0.32),
        "facade": ((0.28, 0.075, 0.045, 1.0), 0.0, 0.62),
        "steel": ((0.045, 0.075, 0.085, 1.0), 0.62, 0.27),
        "hazard": ((0.85, 0.45, 0.015, 1.0), 0.0, 0.38),
        "sign": ((0.75, 0.15, 0.025, 1.0), 0.25, 0.25),
        "water": ((0.025, 0.075, 0.10, 1.0), 0.0, 0.07),
    }
    generated_maps = {}
    texture_size = 128
    for name, (color, metallic, roughness) in definitions.items():
        generated_maps[name] = {}
        for map_name in ("basecolor", "metallic", "roughness", "normal", "ao"):
            image = bpy.data.images.new(
                f"IY_{name}_{map_name}", width=texture_size, height=texture_size, alpha=True
            )
            pixels = []
            for y in range(texture_size):
                for x in range(texture_size):
                    u = x / (texture_size - 1)
                    v = y / (texture_size - 1)
                    grain = 0.5 + 0.5 * math.sin((u * 37.0 + v * 19.0 + len(name)) * math.pi)
                    broad = 0.5 + 0.5 * math.sin((u * 5.0 - v * 3.0 + len(map_name)) * math.pi)
                    wear = 0.84 + 0.16 * (0.65 * grain + 0.35 * broad)
                    if map_name == "basecolor":
                        value = tuple(min(1.0, channel * wear) for channel in color[:3]) + (1.0,)
                    elif map_name == "metallic":
                        metallic_variation = 0.012 if metallic == 0.0 else metallic * 0.18
                        metal = min(
                            1.0,
                            max(0.0, metallic + metallic_variation * (grain - 0.5)),
                        )
                        value = (metal, metal, metal, 1.0)
                    elif map_name == "roughness":
                        rough = min(1.0, roughness * (0.88 + 0.18 * broad))
                        value = (rough, rough, rough, 1.0)
                    elif map_name == "normal":
                        value = (
                            0.5 + 0.08 * (grain - 0.5) + 0.025 * math.sin(u * math.pi * 18.0),
                            0.5 + 0.08 * (broad - 0.5) + 0.025 * math.cos(v * math.pi * 14.0),
                            0.92 + 0.08 * grain,
                            1.0,
                        )
                    else:
                        value = (wear, wear, wear, 1.0)
                    pixels.extend(value)
            image.pixels = pixels
            path = texture_dir / f"STW_INDUSTRIAL_YARD_01_{name}_{map_name}.png"
            image.filepath_raw = str(path)
            image.file_format = "PNG"
            image.save()
            generated_maps[name][map_name] = image

    material_by_semantic = {
        "concrete": materials["concrete"],
        "facade": materials["brick"],
        "steel": materials["steel"],
        "hazard": materials["hazard"],
        "sign": materials["sign"],
        "water": materials["water"],
    }
    for semantic, material in material_by_semantic.items():
        nodes = material.node_tree.nodes
        links = material.node_tree.links
        bsdf = nodes.get("Principled BSDF")
        base = nodes.new("ShaderNodeTexImage")
        base.name = f"IY_{semantic}_BaseColor"
        base.image = generated_maps[semantic]["basecolor"]
        links.new(base.outputs["Color"], bsdf.inputs["Base Color"])
        roughness_node = nodes.new("ShaderNodeTexImage")
        roughness_node.name = f"IY_{semantic}_Roughness"
        roughness_node.image = generated_maps[semantic]["roughness"]
        roughness_node.image.colorspace_settings.name = "Non-Color"
        links.new(roughness_node.outputs["Color"], bsdf.inputs["Roughness"])
        metallic_node = nodes.new("ShaderNodeTexImage")
        metallic_node.name = f"IY_{semantic}_Metallic"
        metallic_node.image = generated_maps[semantic]["metallic"]
        metallic_node.image.colorspace_settings.name = "Non-Color"
        links.new(metallic_node.outputs["Color"], bsdf.inputs["Metallic"])
        normal_texture = nodes.new("ShaderNodeTexImage")
        normal_texture.name = f"IY_{semantic}_Normal"
        normal_texture.image = generated_maps[semantic]["normal"]
        normal_texture.image.colorspace_settings.name = "Non-Color"
        normal_map = nodes.new("ShaderNodeNormalMap")
        links.new(normal_texture.outputs["Color"], normal_map.inputs["Color"])
        links.new(normal_map.outputs["Normal"], bsdf.inputs["Normal"])

    for name in definitions:
        payload = {
            "description": "STW Industrial Yard production PBR source; original procedural foundation.",
            "materialType": "Materials/Types/StandardPBR.materialtype",
            "materialTypeVersion": 5,
            "properties": {
                "baseColor": {"textureMap": f"../Textures/STW_INDUSTRIAL_YARD_01_{name}_basecolor.png"},
                "metallic": {"textureMap": f"../Textures/STW_INDUSTRIAL_YARD_01_{name}_metallic.png"},
                "roughness": {"textureMap": f"../Textures/STW_INDUSTRIAL_YARD_01_{name}_roughness.png"},
                "normal": {
                    "textureMap": f"../Textures/STW_INDUSTRIAL_YARD_01_{name}_normal.png",
                    "factor": 1.0,
                },
                "occlusion": {"diffuseTextureMap": f"../Textures/STW_INDUSTRIAL_YARD_01_{name}_ao.png"},
                "specularF0": {"factor": 0.50},
            },
        }
        (material_dir / f"STW_INDUSTRIAL_YARD_01_{name}.material").write_text(
            json.dumps(payload, indent=4) + "\n", encoding="utf-8"
        )


def main():
    options = args()
    options.output.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    materials = {
        "concrete": mat("M_IY_ConcreteWet", (0.16, 0.17, 0.18), 0.0, 0.32),
        "brick": mat("M_IY_BrickAged", (0.28, 0.075, 0.045), 0.0, 0.62),
        "steel": mat("M_IY_SteelPainted", (0.045, 0.075, 0.085), 0.62, 0.27),
        "sign": mat("M_IY_Sign", (0.75, 0.15, 0.025), 0.25, 0.25),
        "water": mat("M_IY_Water", (0.025, 0.075, 0.10), 0.0, 0.07),
        "hazard": mat("M_IY_Hazard", (0.85, 0.45, 0.015), 0.0, 0.38),
    }
    groups = {}
    for name, group, center, size, material_name in PIECES:
        bpy.ops.mesh.primitive_cube_add(location=center)
        obj = bpy.context.object
        obj.name = name
        obj.scale = tuple(value * 0.5 for value in size)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        obj.data.materials.append(materials[material_name])
        bevel = obj.modifiers.new("EdgeWearBevel", "BEVEL")
        bevel.width = min(0.04, min(size) * 0.15)
        bevel.segments = 3
        groups.setdefault(group, []).append(obj)

    add_high_detail(
        groups,
        {
            "concrete": materials["concrete"],
            "brick": materials["brick"],
            "steel": materials["steel"],
            "hazard": materials["hazard"],
            "sign": materials["sign"],
            "water": materials["water"],
        },
    )

    write_material_sources(options.output, materials)

    for group, objects in sorted(groups.items()):
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        bpy.ops.export_scene.fbx(
            filepath=str(options.output / f"STW_INDUSTRIAL_YARD_01_{group}.fbx"),
            use_selection=True,
            object_types={"MESH"},
            apply_unit_scale=True,
            path_mode="AUTO",
            axis_forward="-Y",
            axis_up="Z",
        )
    bpy.ops.wm.save_as_mainfile(filepath=str(options.output / "STW_INDUSTRIAL_YARD_01.blend"))
    if not options.skip_preview:
        render_preview(options.output)
    report = {
        "asset_family": "STW_INDUSTRIAL_YARD_01",
        "piece_count": len(PIECES),
        "detail_object_count": sum(len(objects) for objects in groups.values()),
        "groups": sorted(groups),
        "contract_valid": len(PIECES) == 14 and len(groups) == 9,
        "world_bounds": {
            name: {"center": list(center), "size": list(size), "group": group}
            for name, group, center, size, _ in PIECES
        },
        "materials": sorted(value.name for value in materials.values()),
        "material_sources": sorted(
            path.name for path in (options.output / "Materials").glob("*.material")
        ),
        "quality_tier": "high_end_procedural_foundation",
        "quality_status": "source-generated; O3DE import and T4 visual review pending",
    }
    (options.output / "STW_INDUSTRIAL_YARD_01.report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
