#!/usr/bin/env python3
"""Deterministic LOD meshes for the ten original STW weapon profiles.

Each profile is a fictional sci-fi presentation silhouette made of convex blocks.
LOD0 is the primary mesh, LOD1 drops the fine parts, and LOD2 keeps the core mass.
The script downloads nothing, copies no third-party mesh, and does not assign a
production classification. Gameplay state stays in PlayerSliceModel.

Axis convention, matching create_stw_smg_01.py:

    +X = right      +Y = aim / forward      +Z = up

Usage:
    python3 tools/assets/create_stw_weapons.py [--check]
"""

from pathlib import Path
import sys

from create_stw_smg_01 import ObjBuilder, add_block, add_offset_block, build as build_smg

REPO = Path(__file__).resolve().parents[2]
WEAPON_ROOT = REPO / "stw-o3de" / "Project" / "Assets" / "Weapons"
BUDGET_TRIANGLES = 85000

SMG_DETAIL = {
    "receiver_body": 0,
    "forward_shroud": 0,
    "grip": 0,
    "magazine_well": 0,
    "upper_rail": 1,
    "optic_housing": 1,
    "muzzle_device": 1,
    "cell_housing": 1,
    "rear_brace": 1,
    "trigger_guard": 1,
    "optic_lens_hood": 2,
    "shroud_vents": 2,
    "side_plate": 2,
    "ejection_block": 2,
}

NOTES = {
    "STW_SMG_01": "Compact primary presentation mesh.",
    "STW_RIFLE_02": "Long secondary presentation mesh with side fins.",
    "STW_RIFLE_03": "Slim long primary presentation mesh.",
    "STW_LMG_04": "Wide heavy primary presentation mesh.",
    "STW_SIDEARM_01": "Short secondary presentation mesh.",
    "STW_LAUNCHER_01": "Thick-tube primary presentation mesh.",
    "STW_TACTICAL_FLASH_01": "Small tactical canister presentation mesh.",
    "STW_TACTICAL_SMOKE_01": "Small tactical canister presentation mesh.",
    "STW_LETHAL_FRAG_01": "Small lethal presentation mesh.",
    "STW_MELEE_01": "Long hand-held presentation mesh.",
}


class Part(object):
    def __init__(self, name, detail, y0, y1, bw, bh, fw, fh, z0, z1, x):
        self.name = name
        self.detail = detail
        self.y0 = y0
        self.y1 = y1
        self.bw = bw
        self.bh = bh
        self.fw = fw
        self.fh = fh
        self.z0 = z0
        self.z1 = z1
        self.x = x


def part(name, detail, y0, y1, bw, bh, fw=None, fh=None, z0=0.0, z1=0.0, x=0.0):
    return Part(name, detail, y0, y1, bw, bh, bw if fw is None else fw, bh if fh is None else fh, z0, z1, x)


def add_part(builder, item):
    if item.x:
        add_offset_block(builder, item.y0, item.y1, (item.bw, item.bh), (item.fw, item.fh),
                         item.x, item.z0, item.z1)
    else:
        add_block(builder, item.y0, item.y1, (item.bw, item.bh), (item.fw, item.fh), item.z0, item.z1)


def _rifle_02():
    return [
        part("body", 0, -0.02, 0.30, 0.040, 0.048),
        part("stock", 0, -0.36, -0.02, 0.022, 0.036, 0.034, 0.044, 0.008, 0.004),
        part("grip", 0, -0.20, -0.08, 0.022, 0.058, z0=-0.100, z1=-0.086),
        part("barrel", 0, 0.30, 0.56, 0.024, 0.026, 0.018, 0.020),
        part("spine", 1, -0.04, 0.34, 0.016, 0.010, z0=0.056, z1=0.052),
        part("optic", 1, -0.02, 0.12, 0.026, 0.024, 0.022, 0.020, 0.086, 0.080),
        part("cell", 1, 0.04, 0.22, 0.026, 0.020, z0=-0.070, z1=-0.062),
        part("muzzle", 1, 0.56, 0.64, 0.022, 0.024, 0.018, 0.020),
        part("magazine", 1, -0.04, 0.06, 0.024, 0.070, z0=-0.120, z1=-0.108),
        part("trigger", 1, -0.10, -0.02, 0.018, 0.010, z0=-0.078, z1=-0.078),
        part("fin_l", 2, 0.08, 0.28, 0.006, 0.020, x=-0.052),
        part("fin_r", 2, 0.08, 0.28, 0.006, 0.020, x=0.052),
        part("stabilizer", 2, 0.18, 0.40, 0.016, 0.008, z0=-0.072, z1=-0.064),
        part("latch", 2, -0.08, 0.00, 0.006, 0.016, x=0.046),
        part("vent_0", 2, 0.34, 0.38, 0.026, 0.006, z0=0.030, z1=0.030),
        part("vent_1", 2, 0.42, 0.46, 0.024, 0.006, z0=0.028, z1=0.028),
    ]


def _rifle_03():
    return [
        part("receiver", 0, -0.08, 0.26, 0.032, 0.040),
        part("shroud", 0, 0.26, 0.62, 0.022, 0.024, 0.016, 0.018),
        part("stock", 0, -0.46, -0.08, 0.016, 0.030, 0.028, 0.038, 0.012, 0.002),
        part("grip", 0, -0.24, -0.10, 0.020, 0.052, z0=-0.092, z1=-0.078),
        part("barrel", 0, 0.62, 0.78, 0.014, 0.016),
        part("optic", 1, 0.02, 0.20, 0.024, 0.028, z0=0.078, z1=0.074),
        part("rail", 1, -0.06, 0.40, 0.014, 0.008, z0=0.048, z1=0.044),
        part("magazine", 1, -0.02, 0.08, 0.020, 0.064, z0=-0.108, z1=-0.096),
        part("muzzle", 1, 0.78, 0.86, 0.016, 0.018),
        part("cheek", 1, -0.34, -0.16, 0.018, 0.012, z0=0.046, z1=0.040),
        part("bipod_l", 2, 0.30, 0.36, 0.006, 0.028, x=-0.036, z0=-0.070, z1=-0.090),
        part("bipod_r", 2, 0.30, 0.36, 0.006, 0.028, x=0.036, z0=-0.070, z1=-0.090),
        part("vent_0", 2, 0.32, 0.36, 0.024, 0.005, z0=0.026, z1=0.026),
        part("vent_1", 2, 0.40, 0.44, 0.022, 0.005, z0=0.024, z1=0.024),
        part("vent_2", 2, 0.48, 0.52, 0.020, 0.005, z0=0.022, z1=0.022),
        part("latch", 2, 0.00, 0.06, 0.005, 0.014, x=0.038),
        part("hood", 2, 0.20, 0.28, 0.020, 0.016, z0=0.074, z1=0.066),
    ]


def _lmg_04():
    return [
        part("body", 0, -0.06, 0.28, 0.058, 0.064),
        part("barrel", 0, 0.28, 0.52, 0.032, 0.034, 0.026, 0.028),
        part("boxmag", 0, 0.02, 0.16, 0.046, 0.070, z0=-0.130, z1=-0.120),
        part("grip", 0, -0.18, -0.06, 0.024, 0.056, z0=-0.110, z1=-0.096),
        part("stock", 0, -0.34, -0.06, 0.030, 0.040, 0.046, 0.052),
        part("spine", 1, -0.02, 0.36, 0.020, 0.012, z0=0.074, z1=0.068),
        part("optic", 1, 0.04, 0.16, 0.030, 0.022, z0=0.096, z1=0.090),
        part("muzzle", 1, 0.52, 0.60, 0.030, 0.032),
        part("heatshield", 1, 0.22, 0.46, 0.040, 0.012, z0=0.040, z1=0.036),
        part("trigger", 1, -0.08, 0.00, 0.020, 0.010, z0=-0.084, z1=-0.084),
        part("foregrip", 1, 0.16, 0.24, 0.016, 0.040, z0=-0.100, z1=-0.092),
        part("bipod_l", 2, 0.24, 0.32, 0.008, 0.036, x=-0.062, z0=-0.090, z1=-0.120),
        part("bipod_r", 2, 0.24, 0.32, 0.008, 0.036, x=0.062, z0=-0.090, z1=-0.120),
        part("rib_0", 2, 0.30, 0.34, 0.036, 0.008, z0=0.040, z1=0.040),
        part("rib_1", 2, 0.36, 0.40, 0.034, 0.008, z0=0.038, z1=0.038),
        part("rib_2", 2, 0.42, 0.46, 0.032, 0.008, z0=0.036, z1=0.036),
        part("rib_3", 2, 0.08, 0.14, 0.060, 0.008, z0=0.020, z1=0.020),
        part("latch", 2, -0.02, 0.06, 0.008, 0.018, x=0.064),
        part("sight", 2, -0.04, 0.02, 0.012, 0.016, z0=0.088, z1=0.088),
    ]


def _sidearm_01():
    return [
        part("slide", 0, -0.02, 0.14, 0.022, 0.026),
        part("frame", 0, -0.06, 0.08, 0.024, 0.030),
        part("grip", 0, -0.14, -0.02, 0.022, 0.048, z0=-0.070, z1=-0.056),
        part("guard", 1, -0.04, 0.04, 0.018, 0.008, z0=-0.048, z1=-0.048),
        part("muzzle", 1, 0.14, 0.19, 0.016, 0.018),
        part("sight", 1, 0.00, 0.04, 0.008, 0.010, z0=0.040, z1=0.040),
        part("cell", 1, -0.02, 0.05, 0.016, 0.012, z0=-0.046, z1=-0.042),
        part("latch", 2, -0.03, 0.02, 0.005, 0.012, x=0.028),
        part("ridge", 2, 0.04, 0.12, 0.008, 0.004, z0=0.030, z1=0.030),
        part("pin", 2, -0.08, -0.05, 0.008, 0.008, x=-0.020, z0=-0.020, z1=-0.020),
        part("catch", 2, 0.06, 0.09, 0.006, 0.008, x=0.026),
    ]


def _launcher_01():
    return [
        part("tube", 0, -0.08, 0.42, 0.070, 0.070),
        part("breech", 0, -0.24, -0.08, 0.078, 0.078, 0.070, 0.070),
        part("cap", 0, 0.42, 0.52, 0.074, 0.074, 0.060, 0.060),
        part("grip", 0, -0.04, 0.06, 0.022, 0.050, z0=-0.120, z1=-0.108),
        part("sight", 1, 0.02, 0.16, 0.024, 0.020, z0=0.100, z1=0.096),
        part("shoulder", 1, -0.30, -0.18, 0.060, 0.050, z0=-0.010, z1=0.000),
        part("rail", 1, 0.00, 0.30, 0.016, 0.010, z0=0.082, z1=0.078),
        part("fore", 1, 0.18, 0.30, 0.020, 0.036, z0=-0.110, z1=-0.100),
        part("lug_0", 2, 0.08, 0.14, 0.010, 0.016, x=-0.078),
        part("lug_1", 2, 0.22, 0.28, 0.010, 0.016, x=0.078),
        part("band", 2, 0.16, 0.20, 0.076, 0.012),
        part("latch", 2, -0.12, -0.04, 0.010, 0.020, x=0.084),
        part("vent_0", 2, 0.30, 0.34, 0.050, 0.008, z0=0.040, z1=0.040),
        part("vent_1", 2, 0.36, 0.40, 0.046, 0.008, z0=0.036, z1=0.036),
    ]


def _flash_01():
    return [
        part("body", 0, -0.03, 0.07, 0.032, 0.032),
        part("cap", 0, 0.07, 0.10, 0.028, 0.028, 0.020, 0.020),
        part("band", 1, 0.01, 0.025, 0.036, 0.010),
        part("lever", 1, -0.01, 0.05, 0.008, 0.014, x=0.036),
        part("base", 1, -0.055, -0.03, 0.030, 0.024),
        part("window", 2, 0.045, 0.065, 0.012, 0.012, z0=0.030, z1=0.030),
        part("ridge", 2, -0.01, 0.04, 0.006, 0.006, z0=0.034, z1=0.034),
        part("lug", 2, 0.00, 0.02, 0.008, 0.010, x=-0.034),
    ]


def _smoke_01():
    return [
        part("body", 0, -0.04, 0.09, 0.034, 0.034),
        part("cap", 0, 0.09, 0.125, 0.030, 0.026, 0.024, 0.020),
        part("base", 0, -0.07, -0.04, 0.032, 0.020),
        part("ring", 1, 0.02, 0.04, 0.038, 0.008),
        part("band", 1, 0.055, 0.075, 0.036, 0.008),
        part("vent_0", 2, 0.00, 0.015, 0.008, 0.010, x=0.036),
        part("vent_1", 2, 0.03, 0.045, 0.008, 0.010, x=-0.036),
        part("vent_2", 2, 0.06, 0.075, 0.008, 0.010, x=0.036),
        part("lug", 2, -0.02, 0.00, 0.010, 0.012, z0=0.036, z1=0.036),
    ]


def _frag_01():
    return [
        part("core", 0, -0.02, 0.04, 0.036, 0.036),
        part("upper", 0, 0.04, 0.075, 0.030, 0.028, 0.018, 0.016),
        part("lower", 0, -0.06, -0.02, 0.030, 0.026, 0.022, 0.018),
        part("ring", 1, 0.00, 0.018, 0.040, 0.008),
        part("cap", 1, 0.075, 0.098, 0.016, 0.014),
        part("band", 1, -0.03, -0.012, 0.038, 0.008),
        part("seam_0", 2, -0.01, 0.03, 0.006, 0.020, x=0.034),
        part("seam_1", 2, -0.01, 0.03, 0.006, 0.020, x=-0.034),
        part("lug", 2, 0.05, 0.07, 0.008, 0.008, z0=0.030, z1=0.030),
        part("ridge", 2, -0.04, 0.02, 0.004, 0.006, z0=-0.034, z1=-0.034),
    ]


def _melee_01():
    return [
        part("handle", 0, -0.18, -0.02, 0.016, 0.018),
        part("blade", 0, -0.02, 0.36, 0.010, 0.028, 0.004, 0.008),
        part("guard", 0, -0.04, -0.01, 0.046, 0.012),
        part("pommel", 1, -0.22, -0.18, 0.022, 0.022),
        part("ridge", 1, 0.02, 0.30, 0.004, 0.006, z0=0.030, z1=0.012),
        part("fuller", 1, 0.04, 0.28, 0.004, 0.006, z0=-0.010, z1=-0.004),
        part("wrap_0", 2, -0.16, -0.13, 0.018, 0.008),
        part("wrap_1", 2, -0.12, -0.09, 0.018, 0.008),
        part("wrap_2", 2, -0.08, -0.05, 0.018, 0.008),
        part("rivet", 2, -0.15, -0.12, 0.006, 0.006, x=0.018),
        part("tip", 2, 0.36, 0.40, 0.006, 0.008, 0.002, 0.003),
        part("latch", 2, -0.06, -0.03, 0.008, 0.008, x=-0.020),
    ]


PROFILES = (
    ("STW_SMG_01", None),
    ("STW_RIFLE_02", _rifle_02),
    ("STW_RIFLE_03", _rifle_03),
    ("STW_LMG_04", _lmg_04),
    ("STW_SIDEARM_01", _sidearm_01),
    ("STW_LAUNCHER_01", _launcher_01),
    ("STW_TACTICAL_FLASH_01", _flash_01),
    ("STW_TACTICAL_SMOKE_01", _smoke_01),
    ("STW_LETHAL_FRAG_01", _frag_01),
    ("STW_MELEE_01", _melee_01),
)


def triangles_of(builder):
    return sum(len(face) - 2 for _, faces in builder.groups for face in faces)


def rebuild(source, keep, lod_name):
    builder = ObjBuilder()
    builder.begin_group(lod_name)
    for name, faces in source.groups:
        if name not in keep:
            continue
        for corner in faces:
            points = [source.positions[position - 1] for position, _uv, _normal in corner]
            normal = source.normals[corner[0][2] - 1]
            builder.add_polygon(points, normal)
    return builder


def build_parts(parts, max_detail, lod_name):
    builder = ObjBuilder()
    builder.begin_group(lod_name)
    kept = [item for item in parts if item.detail <= max_detail]
    for item in kept:
        add_part(builder, item)
    return builder, [item.name for item in kept]


def smg_lods():
    source = build_smg()
    names = [name for name, _faces in source.groups]
    if set(names) != set(SMG_DETAIL):
        raise SystemExit("STW_SMG_01 groups drifted: {0}".format(", ".join(names)))
    built = {}
    for lod_name, max_detail in (("LOD0", 2), ("LOD1", 1), ("LOD2", 0)):
        keep = {name for name, detail in SMG_DETAIL.items() if detail <= max_detail}
        built[lod_name] = (rebuild(source, keep, lod_name), sorted(keep))
    return built


def catalog():
    """Return profile -> {LOD0, LOD1, LOD2: (builder, part names)}."""
    built = {"STW_SMG_01": smg_lods()}
    for profile, factory in PROFILES:
        if factory is None:
            continue
        parts = factory()
        built[profile] = {}
        for lod_name, max_detail in (("LOD0", 2), ("LOD1", 1), ("LOD2", 0)):
            built[profile][lod_name] = build_parts(parts, max_detail, lod_name)
    return built


def lod_counts():
    return {
        profile: [triangles_of(lods[name][0]) for name in ("LOD0", "LOD1", "LOD2")]
        for profile, lods in catalog().items()
    }


def bounds_of(builder):
    xs = [position[0] for position in builder.positions]
    ys = [position[1] for position in builder.positions]
    zs = [position[2] for position in builder.positions]
    return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))


def validate_catalog(built=None):
    built = catalog() if built is None else built
    problems = []
    if tuple(built) != tuple(profile for profile, _factory in PROFILES):
        problems.append("profile order drifted")
    counts = {}
    extents = {}
    for profile, lods in built.items():
        trio = [triangles_of(lods[name][0]) for name in ("LOD0", "LOD1", "LOD2")]
        counts[profile] = trio
        if not trio[0] > trio[1] > trio[2] > 0:
            problems.append("{0} LOD counts {1} do not strictly decrease".format(profile, trio))
        if trio[0] > BUDGET_TRIANGLES:
            problems.append("{0} exceeds triangle budget".format(profile))
        low, high = bounds_of(lods["LOD0"][0])
        extents[profile] = tuple(round(high[axis] - low[axis], 3) for axis in range(3))
        if any(value <= 0.0 for value in extents[profile]):
            problems.append("{0} has a zero extent".format(profile))
        for lod_name, (builder, _parts) in lods.items():
            if [name for name, _faces in builder.groups] != [lod_name]:
                problems.append("{0} {1} group name drifted".format(profile, lod_name))
    if len(set(item[0] for item in counts.values())) != len(counts):
        problems.append("LOD0 triangle counts are not unique: {0}".format(counts))
    if len(set(extents.values())) != len(extents):
        problems.append("LOD0 extents are not unique: {0}".format(extents))
    if problems:
        raise SystemExit("weapon catalog failed: " + "; ".join(problems))
    return counts, extents


def serialise(builder, asset_name, lod_name, part_names):
    lines = [
        "# {0} - original fictional STW first-person weapon presentation asset".format(asset_name),
        "# Generated deterministically by tools/assets/create_stw_weapons.py",
        "# Axis convention: +X right, +Y aim/forward, +Z up",
        "# LOD node: {0}".format(lod_name),
        "# parts: {0}".format(", ".join(part_names)),
        "# No mtllib / usemtl is emitted: the runtime binds one Atom StandardPBR material.",
        "o {0}".format(asset_name),
    ]
    for position in builder.positions:
        lines.append("v {0:.6f} {1:.6f} {2:.6f}".format(*position))
    for uv in builder.uvs:
        lines.append("vt {0:.6f} {1:.6f}".format(*uv))
    for normal in builder.normals:
        lines.append("vn {0:.6f} {1:.6f} {2:.6f}".format(*normal))
    for _name, faces in builder.groups:
        lines.append("g {0}".format(lod_name))
        for corner in faces:
            lines.append("f " + " ".join("{0}/{1}/{2}".format(position, uv, normal)
                                         for position, uv, normal in corner))
    return "\n".join(lines) + "\n"


def provenance_text(asset_name):
    return "\n".join([
        "STW ASSET PROVENANCE",
        "====================",
        "",
        "Asset name:      {0}".format(asset_name),
        "Asset kind:      static first-person presentation mesh (fictional sci-fi weapon body)",
        "Origin:          original fictional STW game art",
        "",
        "Generation",
        "----------",
        "Generated deterministically by tools/assets/create_stw_weapons.py.",
        "The generator uses no randomness, no timestamps and no downloaded geometry.",
        "",
        "Source files",
        "------------",
        "    {0}.obj           LOD0 primary mesh".format(asset_name),
        "    {0}_LOD1.obj      reduced presentation mesh".format(asset_name),
        "    {0}_LOD2.obj      core presentation mesh".format(asset_name),
        "    {0}.material      one Atom StandardPBR material, authored as text".format(asset_name),
        "    Textures/{0}_basecolor.png".format(asset_name),
        "    Textures/{0}_metallic.png".format(asset_name),
        "    Textures/{0}_roughness.png".format(asset_name),
        "    Textures/{0}_normal.png".format(asset_name),
        "    Textures/{0}_ao.png".format(asset_name),
        "    ASSET_PROVENANCE.txt",
        "",
        "Factual statements",
        "------------------",
        "- The geometry was authored locally in this repository.",
        "- No external model source was used and nothing was downloaded to produce this asset.",
        "- BaseColor, metallic, roughness, normal and AO maps are original procedural PNGs",
        "  from tools/assets/create_stw_weapon_textures.py. No image was downloaded.",
        "- No commercial, sample or third-party asset was copied, converted or adapted.",
        "- This is an original fictional sci-fi silhouette, not a replica of any real-world weapon.",
        "- The asset deliberately contains no real-world construction, internal mechanism,",
        "  functional dimensions or manufacturing information.",
        "- The asset is presentation only. It owns no gameplay state. Ammunition, fire",
        "  acceptance, reload completion, hit detection, damage, player movement and target",
        "  state remain with PlayerSliceModel, PhysXPlayerRuntime and ViewmodelPresentation.",
        "- LOD0 is the primary mesh. LOD1 and LOD2 omit fine and then mid parts.",
        "- Geometry uses the engine-local first-person presentation proportions.",
        "- This record assigns no production classification. A Visual-Forge review is still required.",
        "- {0}".format(NOTES[asset_name]),
        "",
    ])


def write_assets(built):
    for profile, lods in built.items():
        directory = WEAPON_ROOT / profile
        directory.mkdir(parents=True, exist_ok=True)
        material = directory / "{0}.material".format(profile)
        if not material.is_file():
            raise SystemExit("missing material for {0}".format(profile))
        for lod_name, (builder, part_names) in lods.items():
            suffix = "" if lod_name == "LOD0" else "_{0}".format(lod_name)
            path = directory / "{0}{1}.obj".format(profile, suffix)
            path.write_text(serialise(builder, profile, lod_name, part_names), encoding="utf-8", newline="\n")
        (directory / "ASSET_PROVENANCE.txt").write_text(provenance_text(profile), encoding="utf-8", newline="\n")


def main(argv=None):
    check_only = "--check" in (argv if argv is not None else sys.argv[1:])
    built = catalog()
    counts, extents = validate_catalog(built)
    for profile, _factory in PROFILES:
        trio = counts[profile]
        extent = extents[profile]
        print("STW_WEAPON profile={0} lod0={1} lod1={2} lod2={3} extent={4:.3f},{5:.3f},{6:.3f}".format(
            profile, trio[0], trio[1], trio[2], *extent))
    if check_only:
        return 0
    write_assets(built)
    print("STW_WEAPONS_WRITTEN={0}".format(WEAPON_ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
