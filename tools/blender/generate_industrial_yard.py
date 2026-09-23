"""Generate the original STW_INDUSTRIAL_YARD_01 source and nine FBX groups."""

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


# Flush surface layers sit on distinct heights above the deck top (0.010 m) so no
# two visible faces are coplanar; coplanar decals z-fight non-deterministically
# (T4 gate runs 20260922T185802Z and 190540Z of the same SHA disagreed).
PIECES = [
    ("wet_concrete_deck", "deck", (0, 0, 0.005), (24, 24, 0.01), "concrete"),
    # North wall is split around a 3 m doorway (x -1.5..1.5) into the North
    # Scrapyard/crane landmark instead of one solid facade - see
    # add_north_scrapyard(). Deliberately a different silhouette from the
    # West Annex (open steel-lattice crane, not a walled building) so the
    # map reads as varied, not one building copy-pasted around.
    ("north_brick_facade_left", "wall", (-6.75, 12, 2), (10.5, 0.45, 3.95), "brick"),
    ("north_brick_facade_right", "wall", (6.75, 12, 2), (10.5, 0.45, 3.95), "brick"),
    # South wall is split around a 3 m doorway (x -1.5..1.5) into the South
    # Verladezone - see add_south_verladezone(). Fourth and final cardinal
    # zone: completes the crossing route network (west<->east, north<->
    # south through the center) instead of a hub with three dead-end arms.
    ("south_concrete_facade_left", "wall", (-6.75, -12, 2), (10.5, 0.45, 3.95), "concrete"),
    ("south_concrete_facade_right", "wall", (6.75, -12, 2), (10.5, 0.45, 3.95), "concrete"),
    # East wall is split around a 3 m doorway (y -1.5..1.5) into the East
    # Containerhof - see add_east_containerhof(). A third, again distinct
    # silhouette: no building, no crane - stacked shipping containers as
    # cover with one elevated platform for verticality.
    ("east_steel_facade_left", "wall", (12, -6.75, 2), (0.45, 10.5, 3.95), "steel"),
    ("east_steel_facade_right", "wall", (12, 6.75, 2), (0.45, 10.5, 3.95), "steel"),
    # West wall is split around a 3 m doorway (y -1.5..1.5) into the West Annex
    # building instead of one solid facade - see add_west_annex().
    ("west_steel_facade_left", "wall", (-12, -6.75, 2), (0.45, 10.5, 3.95), "steel"),
    ("west_steel_facade_right", "wall", (-12, 6.75, 2), (0.45, 10.5, 3.95), "steel"),
    ("left_cover_cladding", "cover", (-2.25, 0, 1.25), (1.5, 2, 2.5), "steel"),
    ("right_cover_cladding", "cover", (2.25, 0, 1.25), (1.5, 2, 2.5), "steel"),
    ("service_step_skin", "cover", (5, -2, 0.125), (2, 2, 0.25), "concrete"),
    ("suspended_gallery", "arch", (0, 6, 4.45), (8, 1, 0.5), "steel"),
    ("west_signage", "props", (-12.7, 2, 2.2), (0.5, 2, 1.5), "sign"),
    ("shallow_puddles", "mark", (0, -6, 0.012), (5, 2, 0.004), "water"),
    ("roof_service_truss", "struct", (0, 2, 4.45), (10, 0.6, 0.5), "steel"),
    ("outside_factory_tower", "beacon", (-13, 5, 2.5), (0.8, 1.5, 3), "steel"),
    ("flush_hazard_trim", "trim", (6, -6, 0.0115), (4, 2, 0.003), "hazard"),
]

# West Annex: the first enterable, multi-storey production building. Entered
# through the doorway gap in the split west wall above. Ground floor at
# z=0..3.5, an industrial loading ramp climbs to a real upper floor at
# z=3.5, open above the east wall's 3.5 m height (0.5 m open band, full
# annex width) as an unglazed window/balcony overlooking the yard - the
# player can fire down into the arena from up there. Pieces are bucketed
# into the existing deck/wall/struct groups (no new visual group, no gate
# script change needed - report.json bounds are a union over all pieces in
# a group and the runtime group AABB comes from the live merged mesh, so an
# annex piece just extends that group's envelope).
ANNEX_PIECES = [
    ("annex_ground_floor", "deck", (-16, 0, -0.05), (8, 8, 0.1), "concrete"),
    ("annex_north_wall", "wall", (-16, 4, 1.75), (8, 0.3, 3.5), "brick"),
    ("annex_south_wall", "wall", (-16, -4, 1.75), (8, 0.3, 3.5), "brick"),
    ("annex_far_wall", "wall", (-20, 0, 1.75), (0.3, 8, 3.5), "brick"),
    ("annex_east_wall_north", "wall", (-12, 2.75, 1.75), (0.3, 2.5, 3.5), "brick"),
    ("annex_east_wall_south", "wall", (-12, -2.75, 1.75), (0.3, 2.5, 3.5), "brick"),
    ("annex_upper_floor_north", "deck", (-16, 2.625, 3.5), (6, 2.75, 0.15), "concrete"),
    ("annex_upper_floor_south", "deck", (-16, -2.625, 3.5), (6, 2.75, 0.15), "concrete"),
]
# Ramp is not axis-aligned, built separately in add_west_annex() with
# detail_box_between(). Endpoints recorded here so report.json/LookTemplate
# derive the same envelope the physics/visual ramp actually occupies.
ANNEX_RAMP = {
    "name": "annex_ramp",
    "group": "struct",
    "start": (-13, 0, 0.05),
    "end": (-19, 0, 3.45),
    "width": 2.5,
    "thickness": 0.2,
    "material": "steel",
}

# North Scrapyard + crane: a second landmark, deliberately not a copy of the
# West Annex. An open steel-lattice tower (no walls - a real gantry crane
# reads as girders, not a building), reached by two switchback ramps around
# a small yard footprint, topped with a platform and an 15 m cantilevered
# boom walked out over the whole arena - a sniper/camper perch, as asked
# for. Ground level has short crate cover for close-range hideouts. Doorway
# gap matches the north wall split above.
SCRAPYARD_PIECES = [
    ("scrapyard_ground", "deck", (0, 17, -0.05), (8, 10, 0.1), "concrete"),
    ("scrapyard_cover_a", "cover", (-2.5, 14, 0.6), (1.4, 1.4, 1.2), "hazard"),
    ("scrapyard_cover_b", "cover", (2.5, 15.5, 0.75), (1.6, 1.6, 1.5), "hazard"),
    ("crane_landing", "struct", (-1, 20, 3.5), (2, 1.2, 0.15), "steel"),
    ("crane_top_platform", "struct", (0, 17, 7.05), (4, 2.2, 0.2), "steel"),
    # Straight, axis-aligned - no rotation needed, unlike the ramps.
    ("crane_boom", "struct", (0, 8.5, 7.1), (1.4, 15.0, 0.2), "steel"),
]
CRANE_RAMPS = [
    {
        "name": "crane_ramp_1", "group": "struct",
        "start": (-1.5, 15.5, 0.05), "end": (-1.5, 20.5, 3.5),
        "width": 1.8, "thickness": 0.2, "material": "steel",
    },
    {
        "name": "crane_ramp_2", "group": "struct",
        "start": (1.5, 20.5, 3.55), "end": (1.5, 15.5, 7.0),
        "width": 1.8, "thickness": 0.2, "material": "steel",
    },
]

# East Containerhof: a third landmark, deliberately unlike either of the
# first two - no building, no crane. Stacked shipping-container cover with
# one elevated platform reached by a ramp for verticality. Kept as a real
# greybox pass (simple boxes, no add_high_detail-style dressing pass) per
# the map-design guide: route network / combat-zone layout first, detail
# later. Doorway gap matches the east wall split above.
EAST_CONTAINERHOF_PIECES = [
    ("containerhof_ground", "deck", (16, 0, -0.05), (8, 12, 0.1), "concrete"),
    ("container_low_a", "cover", (14, -4.3, 1.25), (3, 1.6, 2.5), "steel"),
    ("container_low_b", "cover", (14, 0, 1.25), (3, 1.6, 2.5), "steel"),
    ("container_low_c", "cover", (14, 4.3, 1.25), (3, 1.6, 2.5), "steel"),
    ("container_platform_support", "cover", (18.5, 0, 1.2), (3, 3, 2.4), "steel"),
    ("container_platform", "struct", (18.5, 0, 2.5), (3.4, 3.4, 0.2), "steel"),
]
CONTAINER_RAMP = {
    "name": "container_ramp",
    "group": "struct",
    "start": (15.5, -2.0, 0.05),
    "end": (18.5, -2.0, 2.4),
    "width": 1.8,
    "thickness": 0.2,
    "material": "steel",
}

# South Verladezone: fourth and final cardinal landmark, deliberately unlike
# the first three - a raised concrete loading dock platform (not a building,
# crane, or container stack), flanked by two long parked-trailer cover
# blocks that split the approach into two lanes, plus a small crate cluster
# near the doorway. Concrete ramp (not steel, unlike every other ramp so
# far) for material variety. Doorway gap matches the south wall split above.
VERLADEZONE_PIECES = [
    ("verladezone_ground", "deck", (0, -17, -0.05), (8, 10, 0.1), "concrete"),
    ("dock_platform", "struct", (0, -20.5, 0.6), (5, 2.5, 1.2), "concrete"),
    ("truck_trailer_a", "cover", (-2.6, -15, 1.1), (1.8, 4.5, 2.2), "steel"),
    ("truck_trailer_b", "cover", (2.6, -15, 1.1), (1.8, 4.5, 2.2), "steel"),
    ("loading_crates", "cover", (0, -13, 0.75), (2.2, 1.6, 1.5), "hazard"),
]
DOCK_RAMP = {
    "name": "dock_ramp",
    "group": "struct",
    "start": (0, -17.5, 0.05),
    "end": (0, -20.0, 1.2),
    "width": 2.5,
    "thickness": 0.2,
    "material": "concrete",
}

# NW connector: an outdoor L-shaped walkway linking the West Annex to the
# North Scrapyard directly, bypassing the central hof - the first of the
# route-network-first design guide's crossing routes, not just another
# landmark. Flat (no elevation change, no ramp needed). The NW exterior
# corner (x<-12, y>12) has no floor at all today, so this is real new
# walkable space, not decoration.
CONNECTOR_NW_PIECES = [
    ("connector_nw_ground_a", "deck", (-16, 8.5, -0.05), (3, 9, 0.1), "concrete"),
    ("connector_nw_ground_b", "deck", (-10, 13, -0.05), (12, 3, 0.1), "concrete"),
    ("connector_nw_cover_a", "cover", (-16.8, 8.5, 0.75), (1.2, 1.2, 1.5), "hazard"),
    ("connector_nw_cover_b", "cover", (-10.0, 13.8, 0.75), (1.2, 1.2, 1.5), "hazard"),
]

# NE connector: same idea, mirrored - links North Scrapyard directly to
# East Containerhof through the previously-empty NE exterior corner
# (x>12, y>12), bypassing the central hof.
CONNECTOR_NE_PIECES = [
    ("connector_ne_ground_a", "deck", (16, 9.5, -0.05), (3, 7, 0.1), "concrete"),
    ("connector_ne_ground_b", "deck", (10, 13, -0.05), (12, 3, 0.1), "concrete"),
    ("connector_ne_cover_a", "cover", (16.8, 9.5, 0.75), (1.2, 1.2, 1.5), "hazard"),
    ("connector_ne_cover_b", "cover", (10.0, 13.8, 0.75), (1.2, 1.2, 1.5), "hazard"),
]

# SE connector: mirrors NE across y=0 - links the East Containerhof
# directly to the South Verladezone through the previously-empty SE
# exterior corner (x>12, y<-12).
CONNECTOR_SE_PIECES = [
    ("connector_se_ground_a", "deck", (16, -9.5, -0.05), (3, 7, 0.1), "concrete"),
    ("connector_se_ground_b", "deck", (10, -13, -0.05), (12, 3, 0.1), "concrete"),
    ("connector_se_cover_a", "cover", (16.8, -9.5, 0.75), (1.2, 1.2, 1.5), "hazard"),
    ("connector_se_cover_b", "cover", (10.0, -13.8, 0.75), (1.2, 1.2, 1.5), "hazard"),
]


def args():
    values = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--skip-preview", action="store_true")
    return parser.parse_args(values)


def linear_to_srgb(value):
    """Encode a linear color channel with the sRGB transfer function."""
    if value <= 0.0031308:
        return 12.92 * value
    return 1.055 * value ** (1.0 / 2.4) - 0.055


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


def detail_box_between(name, start, end, width, thickness, material, bevel=0.012):
    """A walkable ramp: a box whose local X axis (length) is rotated to
    point from start to end, so its top surface climbs at exactly the
    start->end slope. width is horizontal (local Y), thickness is the slab
    depth (local Z)."""
    start = Vector(start)
    end = Vector(end)
    direction = end - start
    obj = detail_cube(name, (start + end) * 0.5, (direction.length, width, thickness), material, bevel)
    obj.rotation_euler = direction.to_track_quat("X", "Z").to_euler()
    return obj, direction.length


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
        strip = detail_cube(f"IY_HazardStripe_{index:02d}", (x, -6.0, 0.016), (0.22, 1.7, 0.006), materials["hazard"], 0.001)
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


def world_bounds_of(obj):
    """Real min/max corners of obj in world space, computed from its actual
    evaluated bound_box (not hand-derived), so a rotated piece (the ramp)
    reports its true AABB rather than an estimate."""
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    xs = [c.x for c in corners]
    ys = [c.y for c in corners]
    zs = [c.z for c in corners]
    lo = (min(xs), min(ys), min(zs))
    hi = (max(xs), max(ys), max(zs))
    center = tuple((a + b) / 2.0 for a, b in zip(lo, hi))
    size = tuple(b - a for a, b in zip(lo, hi))
    return center, size


def add_west_annex(groups, materials, world_bounds):
    """The first enterable, multi-storey production building: ground floor
    entered through the doorway gap left in the split west wall, a real
    walkable ramp up to a genuine upper floor, open above the 3.5 m annex
    wall height as an unglazed window/balcony over the yard. Every piece is
    bucketed into the existing deck/wall/struct groups (see PIECES/ANNEX_
    PIECES/ANNEX_RAMP module docstrings for why that keeps the 9-group,
    gate-verified contract intact)."""
    for name, group, center, size, material_name in ANNEX_PIECES:
        obj = detail_cube(name, center, size, materials[material_name])
        groups.setdefault(group, []).append(obj)
        world_bounds[name] = {"center": list(center), "size": list(size), "group": group}

    ramp = ANNEX_RAMP
    ramp_obj, _length = detail_box_between(
        ramp["name"], ramp["start"], ramp["end"], ramp["width"], ramp["thickness"],
        materials[ramp["material"]], bevel=0.01,
    )
    bpy.context.view_layer.update()
    ramp_center, ramp_size = world_bounds_of(ramp_obj)
    groups.setdefault(ramp["group"], []).append(ramp_obj)
    world_bounds[ramp["name"]] = {"center": list(ramp_center), "size": list(ramp_size), "group": ramp["group"]}


def add_north_scrapyard(groups, materials, world_bounds):
    """North landmark: open crane lattice + scrapyard cover, structurally
    distinct from add_west_annex() (no walls, two switchback ramps instead
    of one straight ramp, a long cantilevered boom instead of a floor)."""
    for name, group, center, size, material_name in SCRAPYARD_PIECES:
        obj = detail_cube(name, center, size, materials[material_name])
        groups.setdefault(group, []).append(obj)
        world_bounds[name] = {"center": list(center), "size": list(size), "group": group}

    for ramp in CRANE_RAMPS:
        ramp_obj, _length = detail_box_between(
            ramp["name"], ramp["start"], ramp["end"], ramp["width"], ramp["thickness"],
            materials[ramp["material"]], bevel=0.01,
        )
        bpy.context.view_layer.update()
        ramp_center, ramp_size = world_bounds_of(ramp_obj)
        groups.setdefault(ramp["group"], []).append(ramp_obj)
        world_bounds[ramp["name"]] = {"center": list(ramp_center), "size": list(ramp_size), "group": ramp["group"]}


def add_east_containerhof(groups, materials, world_bounds):
    """Third landmark: stacked-container cover + one elevated platform,
    structurally distinct from both add_west_annex() (walled building) and
    add_north_scrapyard() (open crane lattice)."""
    for name, group, center, size, material_name in EAST_CONTAINERHOF_PIECES:
        obj = detail_cube(name, center, size, materials[material_name])
        groups.setdefault(group, []).append(obj)
        world_bounds[name] = {"center": list(center), "size": list(size), "group": group}

    ramp = CONTAINER_RAMP
    ramp_obj, _length = detail_box_between(
        ramp["name"], ramp["start"], ramp["end"], ramp["width"], ramp["thickness"],
        materials[ramp["material"]], bevel=0.01,
    )
    bpy.context.view_layer.update()
    ramp_center, ramp_size = world_bounds_of(ramp_obj)
    groups.setdefault(ramp["group"], []).append(ramp_obj)
    world_bounds[ramp["name"]] = {"center": list(ramp_center), "size": list(ramp_size), "group": ramp["group"]}


def add_south_verladezone(groups, materials, world_bounds):
    """Fourth and final cardinal landmark: raised loading dock + parked
    trailers, structurally distinct from the building/crane/containers
    already built on the other three sides."""
    for name, group, center, size, material_name in VERLADEZONE_PIECES:
        obj = detail_cube(name, center, size, materials[material_name])
        groups.setdefault(group, []).append(obj)
        world_bounds[name] = {"center": list(center), "size": list(size), "group": group}

    ramp = DOCK_RAMP
    ramp_obj, _length = detail_box_between(
        ramp["name"], ramp["start"], ramp["end"], ramp["width"], ramp["thickness"],
        materials[ramp["material"]], bevel=0.01,
    )
    bpy.context.view_layer.update()
    ramp_center, ramp_size = world_bounds_of(ramp_obj)
    groups.setdefault(ramp["group"], []).append(ramp_obj)
    world_bounds[ramp["name"]] = {"center": list(ramp_center), "size": list(ramp_size), "group": ramp["group"]}


def add_connector_nw(groups, materials, world_bounds):
    """Outdoor walkway linking the West Annex directly to the North
    Scrapyard, bypassing the central hof - a real crossing route, not a
    third dead-end arm off the hub."""
    for name, group, center, size, material_name in CONNECTOR_NW_PIECES:
        obj = detail_cube(name, center, size, materials[material_name])
        groups.setdefault(group, []).append(obj)
        world_bounds[name] = {"center": list(center), "size": list(size), "group": group}


def add_connector_ne(groups, materials, world_bounds):
    """Outdoor walkway linking the North Scrapyard directly to the East
    Containerhof, bypassing the central hof."""
    for name, group, center, size, material_name in CONNECTOR_NE_PIECES:
        obj = detail_cube(name, center, size, materials[material_name])
        groups.setdefault(group, []).append(obj)
        world_bounds[name] = {"center": list(center), "size": list(size), "group": group}


def add_connector_se(groups, materials, world_bounds):
    """Outdoor walkway linking the East Containerhof directly to the South
    Verladezone, bypassing the central hof."""
    for name, group, center, size, material_name in CONNECTOR_SE_PIECES:
        obj = detail_cube(name, center, size, materials[material_name])
        groups.setdefault(group, []).append(obj)
        world_bounds[name] = {"center": list(center), "size": list(size), "group": group}


def write_material_sources(output, materials):
    material_dir = output / "Materials"
    texture_dir = output / "Textures"
    material_dir.mkdir(parents=True, exist_ok=True)
    texture_dir.mkdir(parents=True, exist_ok=True)
    definitions = {
        "concrete": ((0.16, 0.17, 0.18, 1.0), 0.0, 0.32),
        "facade": ((0.28, 0.075, 0.045, 1.0), 0.0, 0.62),
        "steel": ((0.045, 0.075, 0.085, 1.0), 0.0, 0.27),
        "hazard": ((0.85, 0.45, 0.015, 1.0), 0.0, 0.38),
        "sign": ((0.75, 0.15, 0.025, 1.0), 0.0, 0.25),
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
                        # Colors are linear (Principled BSDF inputs); the PNG is read as sRGB.
                        value = tuple(linear_to_srgb(min(1.0, channel * wear)) for channel in color[:3]) + (1.0,)
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
        "steel": mat("M_IY_SteelPainted", (0.045, 0.075, 0.085), 0.0, 0.27),
        "sign": mat("M_IY_Sign", (0.75, 0.15, 0.025), 0.0, 0.25),
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

    world_bounds = {
        name: {"center": list(center), "size": list(size), "group": group}
        for name, group, center, size, _ in PIECES
    }
    add_west_annex(groups, materials, world_bounds)
    add_north_scrapyard(groups, materials, world_bounds)
    add_east_containerhof(groups, materials, world_bounds)
    add_south_verladezone(groups, materials, world_bounds)
    add_connector_nw(groups, materials, world_bounds)
    add_connector_ne(groups, materials, world_bounds)
    add_connector_se(groups, materials, world_bounds)

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
        "piece_count": len(world_bounds),
        "detail_object_count": sum(len(objects) for objects in groups.values()),
        "groups": sorted(groups),
        "contract_valid": len(PIECES) == 18 and len(ANNEX_PIECES) == 8
            and len(SCRAPYARD_PIECES) == 6 and len(EAST_CONTAINERHOF_PIECES) == 6
            and len(VERLADEZONE_PIECES) == 5 and len(CONNECTOR_NW_PIECES) == 4
            and len(CONNECTOR_NE_PIECES) == 4 and len(CONNECTOR_SE_PIECES) == 4
            and len(groups) == 9,
        "world_bounds": world_bounds,
        "west_annex": {
            "description": "First enterable multi-storey building: doorway "
                "in the split west wall, ground floor, ramp to a real upper "
                "floor, open window band over the yard.",
            "doorway_y_span": [-1.5, 1.5],
            "ground_floor_height": 3.5,
            "ramp": ANNEX_RAMP,
        },
        "north_scrapyard": {
            "description": "Second landmark, deliberately not a copy of the "
                "West Annex: open steel-lattice crane (no walls), two "
                "switchback ramps, short crate cover at ground level, a "
                "15 m cantilevered boom walked out over the whole arena as "
                "a sniper/camper perch.",
            "doorway_x_span": [-1.5, 1.5],
            "platform_height": 7.05,
            "ramps": CRANE_RAMPS,
        },
        "east_containerhof": {
            "description": "Third landmark: stacked shipping-container cover "
                "with one elevated platform reached by a ramp - no walls, no "
                "crane, a deliberately different silhouette from the first "
                "two landmarks. Built as a real greybox pass (simple boxes, "
                "no high-detail dressing) per the route-network-first map "
                "design guide.",
            "doorway_y_span": [-1.5, 1.5],
            "platform_height": 2.5,
            "ramp": CONTAINER_RAMP,
        },
        "south_verladezone": {
            "description": "Fourth and final cardinal landmark: raised "
                "concrete loading dock platform flanked by two parked-"
                "trailer cover lanes and a small crate cluster near the "
                "doorway. Completes the crossing route network (west<->east, "
                "north<->south through the central hof) per the route-"
                "network-first map design guide.",
            "doorway_x_span": [-1.5, 1.5],
            "platform_height": 1.2,
            "ramp": DOCK_RAMP,
        },
        "connector_nw": {
            "description": "Outdoor L-shaped walkway linking the West Annex "
                "directly to the North Scrapyard, bypassing the central hof "
                "- the first genuine crossing route beyond the hub-and-spoke "
                "cardinal landmarks, per the route-network-first map design "
                "guide. Flat, no ramp.",
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
