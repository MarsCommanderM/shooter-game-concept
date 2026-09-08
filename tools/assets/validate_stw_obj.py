#!/usr/bin/env python3
"""Deterministic geometry and material validation for the STW arena kit.

This is the CPU-side gate for Block 26E. It does not need the engine, a build
tree, or a GPU: it parses the generated OBJ sources, the .material JSON and the
runtime's own VisualAssets table, and asserts the invariants that would
otherwise only surface on the T4 run.

Checks
    * OBJ topology: every f index in range, no unreferenced vertices
    * TOPOLOGICAL SHELLS (Block 26E-R4): faces are grouped into components by
      shared UNDIRECTED geometric edges, and each closed component's signed
      volume is checked on its own - see `face_components` / `classify_shell`.
      This replaced the Block 26E-R1 scan of the OBJ face stream in emission
      order, which an inward-wound primitive escaped simply by being interleaved
      with an outward one or by having the face order shuffled. Signed volumes
      are never summed across components, so a large outward solid can no longer
      mask a small inward one, and an edge that cannot be resolved into oriented
      sheets fails closed rather than being skipped.
    * player and required enemy spawn capsule clearance from authoritative source
    * no degenerate or zero-area faces, no non-finite coordinates
    * vt present on every face corner; UVs finite
    * vn present, unit length, and agreeing with the face winding (dot > 0)
    * per-mesh bounds inside the arena visual envelope
    * PLAYABLE-SPACE COHERENCE (Block 26E-R1, envelope corrected in R4): every
      solid component that reaches into the traversable collision volume must be
      justified by its own position and extent - see `classify_component`. This
      replaced the Block 26E check that only looked at groups whose name
      contained "wall", which exempted the free-standing structural pylons by
      naming alone.
    * REACH ENVELOPE (Block 26E-R4): the traversable volume is bounded by a
      conservative envelope derived from the real movement constants - mantle
      reach, jump apex, standing capsule, capsule radius - not by the standing
      capsule height alone.
    * UV density: declared metres-per-tile matches both the OBJ header and the
      measured edge-length / UV-length ratio of the emitted geometry
    * mesh -> material pairing parsed FROM ArenaPresentation.cpp, not mirrored
    * .material JSON: known StandardPBR v5 keys only, every textureMap resolves
    * numpy major version (generate_stw_materials.py uses ndarray.ptp())

Usage:
    python3 tools/assets/validate_stw_obj.py [--verbose]
"""

import argparse
import json
import math
import random
import re
import sys
from pathlib import Path

REPO = Path(__file__).parents[2]
ASSET_DIR = REPO / "stw-o3de/Project/Assets/Environment/STW_ARENA_01"
GEM = REPO / "stw-o3de/Gems/STWGameplay/Code"
RUNTIME_TABLE = GEM / "Source/ArenaPresentation.cpp"
RUNTIME_HEADER = GEM / "Include/STWGameplay/ArenaPresentation.h"
PHYSX_SOURCE = GEM / "Source/Clients/PhysXPlayerRuntime.cpp"
PHYSX_HEADER = GEM / "Source/Clients/PhysXPlayerRuntime.h"
SLICE_HEADER = GEM / "Include/STWGameplay/PlayerSliceModel.h"
KIT_GENERATOR = Path(__file__).with_name("generate_stw_arena_kit.py")

# No world-gravity constant is committed in this repository (the O3DE PhysX
# default is ~9.81 m/s^2 and CharacterGameplayConfiguration only sets a
# multiplier of 1.0). The jump apex therefore cannot be proven exactly, so it is
# computed against a deliberately LOW gravity: that overstates the apex, which is
# the safe direction for a presentation clearance bound.
G_CONSERVATIVE = 9.0

EPSILON = 1e-6
AREA_EPSILON = 1e-9
ENVELOPE_MIN = (-12.60, -12.60, -1.00)
ENVELOPE_MAX = (12.60, 12.60, 12.00)

FLUSH_Z = 0.15              # floor relief a player walks over, never into
DECAL_T = 0.02              # thinner than this in any axis == a decal plane
UV_TOLERANCE = 1e-4


class AuthoritativeSourceError(RuntimeError):
    """Raised when a value that must be derived cannot be derived.

    Never caught to substitute a literal: if the authoritative source stops
    parsing, this validator has to fail loudly rather than silently fall back to
    a copy that may already be stale.
    """


def _floats(text):
    return [float(token.rstrip("f")) for token in text.split(",")]


def parse_collision_world():
    """Derive the gameplay collision world from PhysXPlayerRuntime.

    Gameplay owns collision. This validator must never carry its own editable
    copy of the collider list, or the two can drift and the arena would validate
    against a collision world that no longer exists. The CreateStaticBox calls
    and the capsule constants are read straight from the pinned source.
    """
    if not PHYSX_SOURCE.is_file() or not PHYSX_HEADER.is_file():
        raise AuthoritativeSourceError("PhysXPlayerRuntime sources are not readable")

    calls = re.findall(
        r'CreateStaticBox\(\s*"([^"]+)"\s*,\s*AZ::Vector3\(([^)]*)\)\s*,'
        r'\s*AZ::Vector3\(([^)]*)\)\s*\)',
        PHYSX_SOURCE.read_text(encoding="utf-8"))
    if not calls:
        raise AuthoritativeSourceError("no CreateStaticBox calls found")

    colliders = []
    for name, centre_text, size_text in calls:
        centre, size = _floats(centre_text), _floats(size_text)
        if len(centre) != 3 or len(size) != 3:
            raise AuthoritativeSourceError("malformed CreateStaticBox '{0}'".format(name))
        colliders.append((name,
                          tuple(centre[i] - size[i] * 0.5 for i in range(3)),
                          tuple(centre[i] + size[i] * 0.5 for i in range(3))))

    header = PHYSX_HEADER.read_text(encoding="utf-8")
    capsule = {}
    for key in ("CapsuleHeight", "CapsuleRadius", "StepHeight"):
        found = re.search(r"{0}\s*=\s*([0-9.]+)f".format(key), header)
        if not found:
            raise AuthoritativeSourceError("{0} not found in PhysXPlayerRuntime.h".format(key))
        capsule[key] = float(found.group(1))

    # Traversal reach beyond standing height. Both values are gameplay-owned; this
    # validator reads them rather than carrying an editable copy.
    #   MantleMaxHeight  - PhysXPlayerRuntime.cpp, anonymous namespace
    #   JumpImpulseSpeed - PlayerSliceModel.h, the single-tick takeoff velocity
    #                      (PlayerSliceModel::GetDesiredVelocity -> velocity.SetZ)
    movement = {}
    for key, path in (("MantleMaxHeight", PHYSX_SOURCE), ("JumpImpulseSpeed", SLICE_HEADER)):
        if not path.is_file():
            raise AuthoritativeSourceError("{0} is not readable".format(path.name))
        found = re.search(r"{0}\s*=\s*([0-9.]+)f".format(key),
                          path.read_text(encoding="utf-8"))
        if not found:
            raise AuthoritativeSourceError("{0} not found in {1}".format(key, path.name))
        movement[key] = float(found.group(1))
    jump_apex = movement["JumpImpulseSpeed"] ** 2 / (2.0 * G_CONSERVATIVE)

    # The floor is the collider with the largest horizontal footprint; its top
    # face is the plane the player stands on.
    floor = max(colliders, key=lambda c: (c[2][0] - c[1][0]) * (c[2][1] - c[1][1]))
    floor_top = floor[2][2]

    # Perimeter walls: everything above the floor that is thin on exactly one
    # horizontal axis. Their innermost faces bound the traversable volume.
    inner = []
    interior = []
    for name, lo, hi in colliders:
        if name == floor[0]:
            continue
        ex, ey = hi[0] - lo[0], hi[1] - lo[1]
        thin_x, thin_y = ex * 4.0 < ey, ey * 4.0 < ex
        if thin_x or thin_y:
            axis = 0 if thin_x else 1
            inner.append(min(abs(lo[axis]), abs(hi[axis])))
        else:
            interior.append((name, lo, hi))
    if not inner:
        raise AuthoritativeSourceError("no perimeter wall colliders identified")

    return {
        "colliders": colliders,
        "interior": interior,
        "floor_name": floor[0],
        "floor_top": floor_top,
        "wall_inner": min(inner),
        "reach_z": floor_top + capsule["CapsuleHeight"],
        "jump_apex": jump_apex,
        "mantle_max_height": movement["MantleMaxHeight"],
        "jump_impulse_speed": movement["JumpImpulseSpeed"],
        # Standing height alone is not reach. The highest point a player capsule
        # can occupy anywhere in the arena is: traverse/mantle onto a ledge, jump
        # from it, stand full height, plus the capsule's own rounded-cap radius.
        # Block 26E-R1 stopped at floor_top + CapsuleHeight, which is why a solid
        # at 1.95 m read as "above the player" when a plain jump from the deck
        # already puts the capsule top near 3.5 m.
        "conservative_reach_z": (floor_top + movement["MantleMaxHeight"] + jump_apex
                                 + capsule["CapsuleHeight"] + capsule["CapsuleRadius"]),
        # Trim standing proud of a real collider by less than the capsule radius
        # can only ever be clipped by the capsule surface - the collider behind
        # it already stops the player, so it is not a walk-through. That bound is
        # the player's own radius, not a number chosen to make this pass.
        "detail_margin": capsule["CapsuleRadius"],
        "capsule": capsule,
    }


def parse_uv_targets():
    """Read the authored metres-per-tile straight from the kit generator.

    The generator's TILE_* constants are the authoritative design intent; the
    emitted OBJ header and the emitted geometry are both checked against them, so
    a regression in either is caught without this file holding a third copy.
    """
    if not KIT_GENERATOR.is_file():
        raise AuthoritativeSourceError("kit generator is not readable")
    text = KIT_GENERATOR.read_text(encoding="utf-8")
    tiles = {key: float(value) for key, value in
             re.findall(r"^TILE_([A-Z]+)\s*=\s*([0-9.]+)", text, re.MULTILINE)}
    builders = re.findall(r'ObjKit\(\s*"([A-Z0-9_]+)"\s*,\s*TILE_([A-Z]+)\s*\)', text)
    if not builders:
        raise AuthoritativeSourceError("no ObjKit(name, TILE_*) constructions found")
    targets = {}
    for mesh, tile_key in builders:
        if tile_key not in tiles:
            raise AuthoritativeSourceError("TILE_{0} is not defined".format(tile_key))
        targets[mesh] = tiles[tile_key]
    return targets


WORLD = parse_collision_world()
COLLIDERS = WORLD["colliders"]
WALL_INNER = WORLD["wall_inner"]
FLOOR_TOP = WORLD["floor_top"]
REACH_Z = WORLD["reach_z"]                          # standing capsule top only
JUMP_REACH_Z = FLOOR_TOP + WORLD["jump_apex"] + WORLD["capsule"]["CapsuleHeight"]
MANTLE_REACH_Z = (FLOOR_TOP + WORLD["mantle_max_height"]
                  + WORLD["capsule"]["CapsuleHeight"])
CONSERVATIVE_REACH_Z = WORLD["conservative_reach_z"]
DETAIL_MARGIN = WORLD["detail_margin"]
UV_TILE_TARGET = parse_uv_targets()

# StandardPBR materialtype version 5, from the engine's
# Assets/Materials/Types/MaterialInputs/*.json property groups.
VALID_KEYS = {
    "baseColor": {"color", "factor", "textureMap", "useTexture", "textureMapUv",
                  "textureBlendMode"},
    "metallic": {"factor", "textureMap", "useTexture", "textureMapUv"},
    "roughness": {"textureMap", "useTexture", "textureMapUv", "lowerBound",
                  "upperBound", "factor"},
    "normal": {"textureMap", "useTexture", "textureMapUv", "flipX", "flipY", "factor"},
    "occlusion": {"diffuseTextureMap", "diffuseUseTexture", "diffuseTextureMapUv",
                  "diffuseFactor", "specularTextureMap", "specularUseTexture",
                  "specularTextureMapUv", "specularFactor"},
    "specularF0": {"factor", "textureMap", "useTexture", "textureMapUv",
                   "enableMultiScatterCompensation"},
    "emissive": {"enable", "unit", "color", "intensity", "affectedByAlpha",
                 "textureMap", "useTexture", "textureMapUv"},
    "general": {"doubleSided", "applySpecularAA"},
    "opacity": {"mode", "alphaSource", "factor", "textureMap"},
}


class Failures:
    def __init__(self):
        self.items = []

    def add(self, where, message):
        self.items.append("{0}: {1}".format(where, message))

    def __len__(self):
        return len(self.items)


# --------------------------------------------------------------------------- #
# runtime table (M-1: parsed, not mirrored)
# --------------------------------------------------------------------------- #
def parse_runtime_visual_assets(failures):
    """Derive the mesh -> material pairing from ArenaPresentation.cpp itself.

    Block 26E kept a hand-copied EXPECTED_PAIRS tuple here and claimed it caught
    drift against the runtime table. A blind second copy cannot do that, so the
    table is now read from the source of truth. The parse is deliberately narrow:
    it matches the brace-initialised VisualAssets array and nothing else, and a
    parse failure is a validation failure rather than a silent fallback.
    """
    if not RUNTIME_TABLE.is_file():
        failures.add("runtime", "cannot read {0}".format(RUNTIME_TABLE.name))
        return []
    text = RUNTIME_TABLE.read_text(encoding="utf-8")

    start = text.find("VisualAssets[")
    if start < 0:
        failures.add("runtime", "VisualAssets table not found in ArenaPresentation.cpp")
        return []
    brace = text.find("{", start)
    depth = 0
    end = -1
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                end = index
                break
    if end < 0:
        failures.add("runtime", "VisualAssets table is not brace-balanced")
        return []

    entries = re.findall(
        r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,?\s*\}',
        text[brace:end + 1])
    if not entries:
        failures.add("runtime", "VisualAssets table parsed to zero entries")
        return []

    declared = re.search(r"VisualAssetCount\s*=\s*(\d+)", RUNTIME_HEADER.read_text(
        encoding="utf-8")) if RUNTIME_HEADER.is_file() else None
    if declared and int(declared.group(1)) != len(entries):
        failures.add("runtime", "VisualAssetCount={0} but the table has {1} entries"
                     .format(declared.group(1), len(entries)))

    pairs = []
    for visual_id, model_path, material_path in entries:
        for label, path, suffix in (("model", model_path, ".obj.azmodel"),
                                    ("material", material_path, ".azmaterial")):
            if path != path.lower():
                failures.add("runtime", "{0} path is not lowercase: {1}"
                             .format(label, path))
            if not path.endswith(suffix):
                failures.add("runtime", "{0} path does not end in {1}: {2}"
                             .format(label, suffix, path))
        mesh = Path(model_path).name[:-len(".obj.azmodel")].upper()
        material = Path(material_path).name[:-len(".azmaterial")].upper()
        pairs.append((visual_id, mesh, material))
    return pairs


# --------------------------------------------------------------------------- #
# OBJ parsing
# --------------------------------------------------------------------------- #
def parse_obj(path):
    positions, uvs, normals = [], [], []
    groups = []
    current = None
    header_tile = None
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if line.startswith("#"):
            found = re.search(r"scaled to ([0-9.]+) m per tile", line)
            if found:
                header_tile = float(found.group(1))
            continue
        if not line:
            continue
        head, _, rest = line.partition(" ")
        parts = rest.split()
        if head == "v":
            positions.append(tuple(float(v) for v in parts[:3]))
        elif head == "vt":
            uvs.append(tuple(float(v) for v in parts[:2]))
        elif head == "vn":
            normals.append(tuple(float(v) for v in parts[:3]))
        elif head == "g":
            current = (rest, [])
            groups.append(current)
        elif head == "f":
            if current is None:
                current = ("default", [])
                groups.append(current)
            corner = []
            for token in parts:
                bits = token.split("/")
                vi = int(bits[0])
                ti = int(bits[1]) if len(bits) > 1 and bits[1] else None
                ni = int(bits[2]) if len(bits) > 2 and bits[2] else None
                corner.append((vi, ti, ni, number))
            current[1].append(corner)
        elif head in ("o", "s", "usemtl", "mtllib"):
            if head in ("usemtl", "mtllib"):
                raise ValueError("{0} emits {1}, which would create material slots"
                                 .format(path.name, head))
    return positions, uvs, normals, groups, header_tile


def newell(points):
    nx = ny = nz = 0.0
    count = len(points)
    for i in range(count):
        cx, cy, cz = points[i]
        dx, dy, dz = points[(i + 1) % count]
        nx += (cy - dy) * (cz + dz)
        ny += (cz - dz) * (cx + dx)
        nz += (cx - dx) * (cy + dy)
    return (nx, ny, nz)


# --------------------------------------------------------------------------- #
# playable-space coherence (H-1)
# --------------------------------------------------------------------------- #
def _collider_bounds(margin=0.0):
    """Colliders geometry may be judged to belong to, grown by `margin`.

    The floor is deliberately excluded. It lies under the whole arena, so
    granting it a margin would expand it upward into playable space and quietly
    legitimise any obstacle shorter than the margin anywhere on the deck - which
    is precisely the "call the obstacle non-blocking until it passes" failure
    this validator exists to prevent. Geometry that genuinely sits inside the
    floor slab never reaches the traversable volume, so it needs no rule of its
    own.
    """
    return [(name,
             tuple(lo[i] - margin for i in range(3)),
             tuple(hi[i] + margin for i in range(3)))
            for name, lo, hi in COLLIDERS if name != WORLD["floor_name"]]


# The open box a player's capsule can occupy: inside the perimeter collision
# walls, from the floor plane up to the CONSERVATIVE reach envelope - not the
# standing capsule height. A jump, or a mantle onto a ledge followed by a jump,
# carries the capsule far above standing height, and presentation geometry that
# hangs in that band with no collider under it is walk-through.
TRAVERSABLE = ((-WALL_INNER, WALL_INNER), (-WALL_INNER, WALL_INNER),
               (FLOOR_TOP, CONSERVATIVE_REACH_Z))


def _face_intrudes(flo, fhi):
    """Does one face actually enter the traversable volume?

    Per axis the face either overlaps the slab with real extent, or - if it is
    planar on that axis, as every emitted face is on at least one - it must lie
    strictly inside the slab. The strictness is what keeps the four perimeter
    wall runs out: their inner faces sit exactly on the collision plane, so they
    bound the playable volume without ever entering it. Testing faces rather
    than the component AABB is also what stops a welded shell (the wall ring
    welds into one component spanning the whole arena) from being judged by an
    AABB that is mostly empty space.
    """
    for i in range(3):
        low, high = TRAVERSABLE[i]
        overlap = min(fhi[i], high) - max(flo[i], low)
        if overlap > EPSILON:
            continue
        planar = (fhi[i] - flo[i]) <= EPSILON
        if planar and low + EPSILON < flo[i] < high - EPSILON:
            continue
        return False
    return True


def _inside_a_collider(lo, hi, margin=DETAIL_MARGIN):
    for name, blo, bhi in _collider_bounds(margin):
        if all(lo[i] >= blo[i] - EPSILON and hi[i] <= bhi[i] + EPSILON for i in range(3)):
            return name
    return None


def _outside_wall_plane(lo, hi):
    return (lo[0] >= WALL_INNER - EPSILON or hi[0] <= -WALL_INNER + EPSILON
            or lo[1] >= WALL_INNER - EPSILON or hi[1] <= -WALL_INNER + EPSILON)


def _supported_by_collider(lo, hi):
    """Is this component standing ON an approved collider rather than inside one?

    A column bolted to the top of a cover box is carried by that collider - the
    player is stopped by the cover long before reaching it - but it is not
    *inside* the collider, so `_inside_a_collider` cannot see it. The test is
    still purely positional and still tied to a real collider: the component's
    horizontal footprint must sit within the collider's own footprint (plus the
    capsule-radius detail margin, so a slightly proud cap still counts), and its
    underside must rest at or above that collider's top face.

    Only the INTERIOR colliders qualify - the two covers and the step. The floor
    is excluded for the same reason it is excluded from rule A (it lies under the
    whole arena, so "carried on the floor" would legitimise any obstacle
    anywhere), and the perimeter walls are excluded because geometry against a
    wall is judged by the wall plane (rule B), not by sitting on the wall's top
    face and overhanging the room from there.
    """
    for name, clo, chi in WORLD["interior"]:
        blo = tuple(clo[i] - DETAIL_MARGIN for i in range(3))
        bhi = tuple(chi[i] + DETAIL_MARGIN for i in range(3))
        if lo[2] < chi[2] - EPSILON:
            continue                      # hangs below the surface it claims to sit on
        if all(lo[i] >= blo[i] - EPSILON and hi[i] <= bhi[i] + EPSILON for i in range(2)):
            return name
    return None


def classify_component(lo, hi, faces, closed=False):
    """Return (rule, detail) for one welded solid component.

    `closed` says whether the component's topology is a closed shell (see
    `classify_shell`). It only ever makes the classifier stricter: a closed shell
    can never take the open floor-decal exception.

    Geometry that reaches into the traversable volume must satisfy exactly one of
    the Block 26E rules. The test is purely positional: nothing about the group
    name, the mesh name, or the authoring intent can exempt it.

        A  inside an approved existing collider (+ DETAIL_MARGIN, so surface
           detail standing proud of a real collider still passes)
        S  carried on top of an approved existing collider
        B  outside the traversable collision boundary
        C  above the CONSERVATIVE reach envelope (Block 26E-R4: mantle + jump +
           standing capsule + capsule radius, not standing height alone)
        D  genuinely non-blocking presentation detail: a decal-thin plane, or
           floor relief no taller than FLUSH_Z that the player walks over

    Rules D, C and S are decided on the whole component, because "is this object a
    decal / is it overhead / is it carried by something" is a property of the
    object. Rules A and B fall out of a per-face intrusion test, which is what
    keeps a large welded shell (the four wall runs weld into one) from being
    judged by an AABB that spans the arena and is mostly empty space.
    """
    extent = [hi[i] - lo[i] for i in range(3)]
    # D is the floor-detail exception and nothing else. Both of its forms require
    # the geometry to actually lie in the authorised floor-relief band:
    # thinness is NOT an exemption on its own, because a paper-thin plane hanging
    # at chest height is still something the player walks through. The open-decal
    # arm additionally requires the component to be an open surface that is flat
    # and roughly horizontal - a closed shell is categorically ineligible for it
    # and has to earn the flush-relief arm instead. Every part of this is
    # geometric: no group name, mesh name, or authoring intent can reach it, so
    # renaming a group cannot change the classification.
    if hi[2] <= FLOOR_TOP + FLUSH_Z + EPSILON:
        flat = extent[2] <= DECAL_T + EPSILON
        horizontal = flat and extent[2] <= min(extent[0], extent[1])
        if flat and horizontal and not closed:
            return "D", "open floor decal plane at z={0:.3f}".format(hi[2])
        return "D", "flush floor relief (tops out at {0:.3f} m)".format(hi[2])
    if lo[2] >= CONSERVATIVE_REACH_Z - EPSILON:
        return "C", "above the conservative reach envelope ({0:.3f} m)".format(
            CONSERVATIVE_REACH_Z)

    if _outside_wall_plane(lo, hi):
        return "B", "outside the traversable boundary"
    collider = _inside_a_collider(lo, hi)
    if collider is not None:
        return "A", "within {0} (+{1:.2f} m detail margin)".format(collider, DETAIL_MARGIN)
    carrier = _supported_by_collider(lo, hi)
    if carrier is not None:
        return "S", "carried on {0}".format(carrier)

    worst = None
    for points in faces:
        flo = tuple(min(p[i] for p in points) for i in range(3))
        fhi = tuple(max(p[i] for p in points) for i in range(3))
        if not _face_intrudes(flo, fhi):
            continue
        if worst is None:
            worst = ([*flo], [*fhi])
        else:
            for i in range(3):
                worst[0][i] = min(worst[0][i], flo[i])
                worst[1][i] = max(worst[1][i], fhi[i])
    if worst is None:
        return "B", "no face enters the traversable volume"
    if _inside_a_collider(tuple(worst[0]), tuple(worst[1])) is not None:
        return "A", "intruding faces stay within an approved collider"

    return None, ("solid reaching x[{0:.2f},{1:.2f}] y[{2:.2f},{3:.2f}] "
                  "z[{4:.2f},{5:.2f}] with no collider".format(
                      worst[0][0], worst[1][0], worst[0][1], worst[1][1],
                      worst[0][2], worst[1][2]))


def connected_components(positions, groups):
    """Weld faces into components through shared position indices.

    This is the PLAYABLE-SPACE grouping, deliberately coarser than the topology
    grouping in `face_components`: it welds through single shared vertices too,
    so a whole wall run is judged as one object. Face indices are returned in the
    same order `validate_mesh` builds its flat polygon list, so a component can
    be cross-referenced against the topology classification.
    """
    parent = {}

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    for _, faces in groups:
        for corner in faces:
            for vi, _, _, _ in corner:
                parent.setdefault(vi, vi)
            first = corner[0][0]
            for vi, _, _, _ in corner[1:]:
                union(first, vi)

    buckets = {}
    index = 0
    for group_name, faces in groups:
        for corner in faces:
            root = find(corner[0][0])
            entry = buckets.setdefault(root, [group_name, [], []])
            entry[1].append([positions[vi - 1] for vi, _, _, _ in corner])
            entry[2].append(index)
            index += 1
    out = []
    for group_name, component_faces, face_indices in buckets.values():
        points = [p for face in component_faces for p in face]
        lo = tuple(min(p[i] for p in points) for i in range(3))
        hi = tuple(max(p[i] for p in points) for i in range(3))
        out.append((group_name, lo, hi, component_faces, face_indices))
    return out


def signed_volume(faces):
    """Signed tetrahedral volume, independent of OBJ normals and generator code."""
    origin = faces[0][0]
    volume = 0.0
    for face in faces:
        a = tuple(face[0][j] - origin[j] for j in range(3))
        for i in range(1, len(face) - 1):
            b = tuple(face[i][j] - origin[j] for j in range(3))
            c = tuple(face[i + 1][j] - origin[j] for j in range(3))
            volume += (a[0] * (b[1]*c[2] - b[2]*c[1])
                       + a[1] * (b[2]*c[0] - b[0]*c[2])
                       + a[2] * (b[0]*c[1] - b[1]*c[0])) / 6.0
    return volume


# --------------------------------------------------------------------------- #
# topology (H-1): edge connectivity, manifold class, per-shell signed volume
# --------------------------------------------------------------------------- #
SHELL_CLOSED = "closed"
SHELL_OPEN = "open"
SHELL_BROKEN = "broken"


def _weld_key(point):
    """Quantised position key.

    The generator rounds every emitted coordinate to six decimals, so this welds
    exactly the vertices it intended to weld - and, unlike an OBJ `v` index, it
    keeps working when two primitives were emitted with independent vertex lists.
    `+ 0.0` normalises -0.0 to 0.0 so mirrored geometry hashes identically.
    """
    return tuple(round(c, 6) + 0.0 for c in point)


def _undirected(a, b):
    return (a, b) if a <= b else (b, a)


def _edge_sheets(key, uses, faces):
    """Pair the face sides meeting at one edge into individual sheets.

    This kit is built from abutting closed primitives, so one geometric edge
    legitimately carries four or more face sides - two solids that merely touch
    along it. Welding those into a single component would let their signed
    volumes be summed, which is exactly the masking this validator exists to
    prevent, and treating them as non-manifold would reject correct geometry.

    Sorting the incident faces by their dihedral angle about the edge recovers
    the individual solids. Going counter-clockwise about the edge, a solid's
    sector OPENS at the face that traverses the edge q->p and CLOSES at the next
    face, which must traverse it p->q; coincident angles are ordered closes-first
    so two solids meeting face to face still pair with themselves.

    Returns the list of (use, use) pairs, or None when no such pairing exists -
    deliberately overlapping primitives put two coincident sheets at the same
    angle, and those are simply left unwelded rather than called a defect.
    """
    p, q = key
    axis = [q[i] - p[i] for i in range(3)]
    length = math.sqrt(sum(c * c for c in axis))
    if length <= EPSILON:
        return None
    axis = [c / length for c in axis]
    seed = (0.0, 0.0, 1.0) if abs(axis[2]) < 0.9 else (1.0, 0.0, 0.0)
    projection = sum(seed[i] * axis[i] for i in range(3))
    u = [seed[i] - axis[i] * projection for i in range(3)]
    u_length = math.sqrt(sum(c * c for c in u))
    if u_length <= EPSILON:
        return None
    u = [c / u_length for c in u]
    v = [axis[1] * u[2] - axis[2] * u[1],
         axis[2] * u[0] - axis[0] * u[2],
         axis[0] * u[1] - axis[1] * u[0]]
    mid = [(p[i] + q[i]) * 0.5 for i in range(3)]

    ordered = []
    for index, pair in uses:
        points = faces[index]
        centroid = [sum(point[i] for point in points) / len(points) for i in range(3)]
        offset = [centroid[i] - mid[i] for i in range(3)]
        along = sum(offset[i] * axis[i] for i in range(3))
        offset = [offset[i] - along * axis[i] for i in range(3)]
        if sum(c * c for c in offset) <= AREA_EPSILON:
            return None
        angle = math.atan2(sum(offset[i] * v[i] for i in range(3)),
                           sum(offset[i] * u[i] for i in range(3)))
        opens = pair != key
        ordered.append((angle, 1 if opens else 0, index, pair, opens))
    ordered.sort(key=lambda item: (item[0], item[1]))

    count = len(ordered)
    pairs = []
    for position, item in enumerate(ordered):
        if not item[4]:
            continue
        following = ordered[(position + 1) % count]
        if following[4]:
            return None                 # two sectors open in a row
        pairs.append(((item[2], item[3]), (following[2], following[3])))
    if len(pairs) * 2 != count:
        return None
    return pairs


def face_components(faces):
    """Split a face list into components joined by shared UNDIRECTED edges.

    Two faces join when they share a whole geometric edge, normalised as
    (min(vertex, vertex), max(vertex, vertex)) on welded coordinates. Faces that
    merely touch at a single vertex do NOT join.

    This replaces the Block 26E-R1 `closed_shells` face-stream scan, which walked
    the OBJ faces in emission order and cut a "shell" wherever edge incidence
    happened to close. That made the result depend on contiguous primitive
    emission: interleaving two primitives, or shuffling the face order, hid an
    inward-wound solid. Nothing here reads a group name, a face index range, an
    emission order, or the generator's primitive functions.

    A shared edge welds two faces into one shell only when it carries exactly two
    face sides traversing it in opposite directions - a clean manifold edge. A
    balanced edge carrying four or more sides is two or more independent sheets
    that merely touch along it: this kit is built from abutting and overlapping
    closed primitives, and welding those into a single "shell" would let their
    signed volumes be summed, which is precisely the masking this validator
    exists to prevent. An edge that cannot be resolved into oriented sheets at
    all - an odd number of sides, or more sides running one way than the other -
    is a real defect: those faces ARE welded together so the defect is reported
    as one broken component rather than silently split apart.

    Returns [(face_indices, edge_uses, defect)] where edge_uses is the component's
    own edge incidence and defect is None or a description.
    """
    parent = list(range(len(faces)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    edges = {}
    for index, face in enumerate(faces):
        keys = [_weld_key(point) for point in face]
        for a, b in zip(keys, keys[1:] + keys[:1]):
            edges.setdefault(_undirected(a, b), []).append((index, (a, b)))

    defects = {}
    for key, uses in edges.items():
        if len(uses) == 1:
            continue                            # honest boundary edge
        forward = sum(1 for _, pair in uses if pair == key)
        if forward * 2 != len(uses):
            # An odd number of sides, or more sides running one way than the
            # other, cannot be resolved into oriented sheets at all. Weld the
            # faces so the defect surfaces as one broken component instead of
            # quietly splitting into pieces that each look fine.
            for index, _ in uses[1:]:
                union(uses[0][0], index)
            defects[uses[0][0]] = (
                "non-manifold edge: {0} face sides, {1} one way and {2} the other"
                .format(len(uses), forward, len(uses) - forward))
            continue
        sheets = _edge_sheets(key, uses, faces)
        if sheets is None:
            continue                            # coincident sheets; weld nothing
        for (first, _), (second, _) in sheets:
            union(first, second)

    members = {}
    for index in range(len(faces)):
        members.setdefault(find(index), []).append(index)

    out = []
    for indices in members.values():
        local = {}
        for index in indices:
            keys = [_weld_key(point) for point in faces[index]]
            for a, b in zip(keys, keys[1:] + keys[:1]):
                local.setdefault(_undirected(a, b), []).append((index, (a, b)))
        defect = next((defects[i] for i in indices if i in defects), None)
        out.append((sorted(indices), local, defect))
    return out


def classify_shell(edge_uses, defect=None):
    """Topological class of one connected face component, from edge incidence.

        closed  every undirected edge of the component is used by exactly two of
                its face sides, traversing it in opposite directions: a closed,
                consistently oriented 2-manifold whose signed volume is
                meaningful and therefore REQUIRED to be positive.
        broken  an edge that cannot be resolved into oriented sheets, or two
                sides traversing a shared edge the same way. Never silently
                accepted as a valid solid and never skipped just because its
                volume cannot be trusted - it fails closed.
        open    has genuine boundary edges and is otherwise a consistently
                oriented manifold-with-boundary: a marking plane, a conduit tube,
                a bolt cap. It carries no signed-volume requirement, but it is
                counted and reported rather than dropped.
    """
    if defect is not None:
        return SHELL_BROKEN, defect
    boundary = 0
    shared = 0
    for uses in edge_uses.values():
        if len(uses) == 1:
            boundary += 1
            continue
        if len(uses) > 2:
            return SHELL_BROKEN, "edge shared by {0} face sides inside one shell".format(
                len(uses))
        shared += 1
        if uses[0][1] != uses[1][1][::-1]:
            return SHELL_BROKEN, "two faces traverse a shared edge the same way"
    if boundary == 0:
        return SHELL_CLOSED, "closed 2-manifold ({0} edges)".format(shared)
    return SHELL_OPEN, "open surface ({0} boundary / {1} shared edges)".format(
        boundary, shared)


def shell_report(faces):
    """Per-component topology and, for closed shells only, signed volume."""
    report = []
    for indices, edge_uses, defect in face_components(faces):
        klass, reason = classify_shell(edge_uses, defect)
        volume = (signed_volume([faces[i] for i in indices])
                  if klass == SHELL_CLOSED else None)
        report.append((indices, klass, reason, volume))
    return report


def validate_outward_winding(faces, where, failures):
    """Per-connected-shell orientation check.

    Signed volume is computed PER closed component and never aggregated across
    components, so OUTWARD + INWARD fails even though the net total is positive
    and a large outward box can no longer mask a small inward one.
    """
    closed = 0
    for indices, klass, reason, volume in shell_report(faces):
        anchor = min(indices) + 1
        if klass == SHELL_BROKEN:
            failures.add(where, "component at face {0}: {1}".format(anchor, reason))
            continue
        if klass != SHELL_CLOSED:
            continue
        closed += 1
        if not math.isfinite(volume) or volume <= AREA_EPSILON:
            failures.add(where, "closed solid at face {0}: non-outward winding "
                                "signed_volume={1:.9g}".format(anchor, volume))
    return closed


def closed_shell_face_indices(faces):
    """Indices of every face belonging to a closed shell.

    A closed shell is a solid: it can never claim the open floor-detail
    exception, however thin or however low it sits.
    """
    out = set()
    for indices, klass, _, _ in shell_report(faces):
        if klass == SHELL_CLOSED:
            out.update(indices)
    return out


def spawn_bounds():
    """Capsule bounds derived from actual player/enemy spawn sources."""
    layout = (GEM / "Include/STWGameplay/ArenaLayout.h").read_text()
    player = re.search(r"PlayerSpawn\s*=\s*AZ::Vector3\(([^)]+)\)", layout)
    enemy_source = (GEM / "Source/EnemyCollectionModel.cpp").read_text()
    table = re.search(r"s_defaultSpawnPositions\s*=\s*\{(.*?)\};", enemy_source, re.S)
    header = (GEM / "Source/Clients/PhysXEnemyRuntime.h").read_text()
    if not player or not table:
        raise AuthoritativeSourceError("cannot derive spawn positions")
    def constant(key):
        match = re.search(key + r"\s*=\s*([0-9.]+)f", header)
        if not match:
            raise AuthoritativeSourceError("missing enemy " + key)
        return float(match.group(1))
    specs = [("player", _floats(player.group(1)), WORLD["capsule"]["CapsuleRadius"],
              WORLD["capsule"]["CapsuleHeight"], 0.0)]
    enemies = re.findall(r"AZ::Vector3\(([^)]+)\)", table.group(1))
    if not enemies:
        raise AuthoritativeSourceError("empty enemy spawn table")
    for i, raw in enumerate(enemies):
        specs.append(("enemy_{0}".format(i + 1), _floats(raw), constant("CapsuleRadius"),
                      constant("CapsuleHeight"), constant("CenterHeight")))
    return [(name, (p[0]-r, p[1]-r, p[2]-offset),
             (p[0]+r, p[1]+r, p[2]-offset+h)) for name, p, r, h, offset in specs]


def bounds_intrude(lo, hi, target_lo, target_hi):
    # Strict overlap allows contact with the floor; planar faces inside count.
    return all((min(hi[i], target_hi[i]) - max(lo[i], target_lo[i]) > EPSILON)
               or (abs(hi[i]-lo[i]) <= EPSILON
                   and target_lo[i]+EPSILON < lo[i] < target_hi[i]-EPSILON)
               for i in range(3))


def capsule_intrudes(flo, fhi, lo, hi):
    """Capsule segment versus obstacle AABB distance; floor tangency is allowed.

    A face's AABB is conservative for sloping polygons, but rounded capsule
    ends are respected (a full enclosing box would reject nearby floor paint).
    """
    radius = (hi[0] - lo[0]) * 0.5
    segment_lo = ((lo[0]+hi[0])*0.5, (lo[1]+hi[1])*0.5, lo[2]+radius)
    segment_hi = (segment_lo[0], segment_lo[1], hi[2]-radius)
    distance_sq = sum(max(flo[i]-segment_hi[i], segment_lo[i]-fhi[i], 0.0)**2
                      for i in range(3))
    return distance_sq < (radius-EPSILON)**2


def validate_spawn_clearance(faces, where, failures, solid_faces=None):
    # Open horizontal floor paint is presentation-only relief, not a solid
    # obstacle. Never exempt a closed solid merely because it is short - the
    # solid set comes from real topology, not from a face-stream scan.
    if solid_faces is None:
        solid_faces = closed_shell_face_indices(faces)
    for name, lo, hi in spawn_bounds():
        for index, face in enumerate(faces):
            flo = tuple(min(p[i] for p in face) for i in range(3))
            fhi = tuple(max(p[i] for p in face) for i in range(3))
            floor_paint = (index not in solid_faces and abs(fhi[2]-flo[2]) <= EPSILON
                           and FLOOR_TOP-EPSILON <= flo[2] <= FLOOR_TOP+FLUSH_Z)
            if not floor_paint and capsule_intrudes(flo, fhi, lo, hi):
                failures.add(where, "visual face intrudes into {0} spawn capsule".format(name))
                break


def winding_selftest():
    # Hand-authored tetrahedron; never import the generator under test.
    a, b, c, d = (0.,0.,0.), (1.,0.,0.), (0.,1.,0.), (0.,0.,1.)
    outward = [[a,c,b], [a,b,d], [a,d,c], [b,c,d]]
    inward = [list(reversed(f)) for f in outward]
    mixed = [list(reversed(outward[0]))] + outward[1:]
    large = [[tuple(10*x+20 for x in p) for p in f] for f in outward]
    problems = []
    for label, faces, expected in (("outward", outward, False), ("inward", inward, True),
                                    ("mixed", mixed, True), ("masked", large+inward, True)):
        failures = Failures()
        validate_outward_winding(faces, label, failures)
        if bool(len(failures)) != expected:
            problems.append("winding regression: " + label)
    if not bounds_intrude((-.1,-.1,.2), (.1,.1,.4), (-.35,-.35,0), (.35,.35,1.8)):
        problems.append("spawn obstruction regression")
    if bounds_intrude((-1,-1,-1), (1,1,0), (-.35,-.35,0), (.35,.35,1.8)):
        problems.append("spawn floor contact regression")
    if capsule_intrudes((-.1,-.1,-1), (.1,.1,0), (-.35,-.35,0), (.35,.35,1.8)):
        problems.append("capsule floor tangency regression")
    if not capsule_intrudes((-.1,-.1,.2), (.1,.1,.4), (-.35,-.35,0), (.35,.35,1.8)):
        problems.append("capsule obstacle regression")
    return problems


# --------------------------------------------------------------------------- #
# H-1 topology regression set
# --------------------------------------------------------------------------- #
def _outward_box(lo, hi):
    """Hand-authored closed box, every face wound counter-clockwise from outside.

    Written out here rather than imported from the generator: a regression suite
    that borrows the winding it is supposed to be checking proves nothing.
    """
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    return [
        [(x1, y1, z0), (x1, y0, z0), (x0, y0, z0), (x0, y1, z0)],   # -Z
        [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)],   # +Z
        [(x1, y0, z0), (x1, y0, z1), (x0, y0, z1), (x0, y0, z0)],   # -Y
        [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)],   # +Y
        [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)],   # -X
        [(x1, y1, z0), (x1, y1, z1), (x1, y0, z1), (x1, y0, z0)],   # +X
    ]


def _reversed_faces(faces):
    return [list(reversed(face)) for face in faces]


def _winding_rejects(faces):
    failures = Failures()
    validate_outward_winding(faces, "regression", failures)
    return len(failures) > 0


def validate_mesh_component_verdict(group_name):
    """Classify one fixed solid presented under an arbitrary OBJ group name.

    The box sits in open deck at chest height with no collider under it, so it
    has to be rejected whatever it is called - naming it "floor_paint" must buy
    it exactly nothing. This runs the real path (`connected_components` ->
    `closed_shell_face_indices` -> `classify_component`), not a shortcut.
    """
    lo, hi = (-1.0, 5.0, 0.40), (0.0, 6.0, 1.40)
    positions = []
    index = {}
    corners = []
    for face in _outward_box(lo, hi):
        corner = []
        for point in face:
            key = _weld_key(point)
            if key not in index:
                positions.append(key)
                index[key] = len(positions)
            corner.append((index[key], 1, 1, 0))
        corners.append(corner)
    groups = [(group_name, corners)]
    polygon_faces = [[positions[vi - 1] for vi, _, _, _ in corner] for corner in corners]
    solid = closed_shell_face_indices(polygon_faces)
    verdicts = []
    for _, clo, chi, component_faces, face_indices in connected_components(positions, groups):
        closed = bool(face_indices) and all(i in solid for i in face_indices)
        rule, _ = classify_component(clo, chi, component_faces, closed)
        verdicts.append(rule or "REJECT")
    return tuple(sorted(verdicts))


def topology_selftest():
    """Pin the Block 26E-R4 topology rewrite.

    The Block 26E-R1 `closed_shells` walked the OBJ face stream in emission order
    and cut a shell wherever edge incidence happened to close, so an inward solid
    survived simply by being interleaved with an outward one, or by having the
    face order shuffled. Every case below is built from the SAME geometry in
    different orders, or under different group names, and must give the same
    answer. Returns (problems, tokens).
    """
    # The two boxes are deliberately different sizes: the big one has volume +8
    # and the small one -1, so a validator that sums disconnected shells sees a
    # comfortable +7 and reports PASS. That is exactly the masking the R1
    # face-stream scan allowed, and these cases fail unless the volume is decided
    # per connected shell.
    outward = _outward_box((0.0, 0.0, 0.0), (2.0, 2.0, 2.0))
    inward = _reversed_faces(outward)
    far_outward = _outward_box((5.0, 5.0, 5.0), (6.0, 6.0, 6.0))
    far_inward = _reversed_faces(far_outward)

    def interleave(first, second):
        out = []
        for a, b in zip(first, second):
            out.extend((a, b))
        return out

    def shuffled(faces):
        out = list(faces)
        random.Random(26041).shuffle(out)
        return out

    open_plane = [[(0.0, 0.0, 0.5), (1.0, 0.0, 0.5), (1.0, 1.0, 0.5), (0.0, 1.0, 0.5)]]
    # A quad glued along one edge of the closed box: that edge now carries three
    # face sides, which no orientation can resolve.
    fin = [[(2.0, 0.0, 0.0), (2.0, 2.0, 0.0), (3.0, 2.0, 0.0), (3.0, 0.0, 0.0)]]

    problems = []
    tokens = {}

    def record(token, faces, required):
        observed = "FAIL" if _winding_rejects(faces) else "PASS"
        tokens[token] = observed
        if observed != required:
            problems.append("topology regression: {0} required {1}, got {2}"
                            .format(token, required, observed))

    record("OUTWARD_SINGLE_BOX", outward, "PASS")
    record("INWARD_SINGLE_BOX", inward, "FAIL")
    record("TWO_DISCONNECTED_OUTWARD_BOXES", outward + far_outward, "PASS")
    record("ONE_OUTWARD_ONE_INWARD", outward + far_inward, "FAIL")
    record("INTERLEAVED_FACE_STREAM_ONE_INWARD", interleave(outward, far_inward), "FAIL")
    record("SHUFFLED_FACE_ORDER_OUTWARD", shuffled(outward + far_outward), "PASS")
    record("SHUFFLED_FACE_ORDER_INWARD", shuffled(outward + far_inward), "FAIL")
    record("NON_MANIFOLD_COMPONENT", outward + fin, "FAIL")

    # An open plane has no signed-volume requirement, but it must be recognised
    # as an OPEN component rather than quietly skipped.
    shells = shell_report(open_plane)
    open_ok = (not _winding_rejects(open_plane) and len(shells) == 1
               and shells[0][1] == SHELL_OPEN)
    tokens["OPEN_PLANE_NOT_CLOSED"] = "PASS_AS_OPEN_COMPONENT" if open_ok else "FAIL"
    if not open_ok:
        problems.append("topology regression: OPEN_PLANE_NOT_CLOSED did not classify "
                        "as an open component")

    # A closed shell may never take the open floor-decal exception. The same
    # decal-thin closed box is rejected when it hangs inside the reach envelope
    # and accepted as flush relief only where it actually lies on the deck.
    thin = 0.5 * DECAL_T
    high_lo, high_hi = (-1.0, 5.0, 2.50), (0.0, 6.0, 2.50 + thin)
    low_lo, low_hi = (-1.0, 5.0, FLOOR_TOP - thin), (0.0, 6.0, FLOOR_TOP)
    high_rule, _ = classify_component(high_lo, high_hi, _box_faces(high_lo, high_hi), True)
    low_rule, _ = classify_component(low_lo, low_hi, _box_faces(low_lo, low_hi), True)
    open_high_rule, _ = classify_component(
        high_lo, high_hi, _box_faces(high_lo, high_hi), False)
    exception_ok = high_rule is None and low_rule == "D" and open_high_rule is None
    tokens["CLOSED_SOLID_CANNOT_USE_OPEN_DETAIL_EXCEPTION"] = (
        "PASS" if exception_ok else "FAIL")
    if not exception_ok:
        problems.append("topology regression: a decal-thin closed shell escaped the "
                        "floor-detail rule (elevated={0}, flush={1})"
                        .format(high_rule, low_rule))

    # Group names reach no part of the classification. Same geometry, different
    # names, in a different order: identical verdicts.
    mixed = outward + far_inward
    first = Failures()
    second = Failures()
    validate_outward_winding(mixed, "group_alpha", first)
    validate_outward_winding(shuffled(mixed), "totally_different_group_name", second)
    renamed = [validate_mesh_component_verdict("floor_paint"),
               validate_mesh_component_verdict("hero_arch_support")]
    rename_ok = len(first) == len(second) and len(set(renamed)) == 1
    tokens["GROUP_RENAME_CANNOT_CHANGE_RESULT"] = "PASS" if rename_ok else "FAIL"
    if not rename_ok:
        problems.append("topology regression: result depends on the group name")

    return problems, tokens


# --------------------------------------------------------------------------- #
# UV density (M-2)
# --------------------------------------------------------------------------- #
def measure_uv_tile(positions, uvs, groups):
    """Median world-metres per UV unit across every face edge.

    World-planar UVs put a constant ratio on every edge that is not foreshortened
    by the projection, so the median is the authored metres-per-tile and the tail
    above it is the chamfer / diagonal edges.
    """
    ratios = []
    for _, faces in groups:
        for corner in faces:
            for i in range(len(corner)):
                a = corner[i]
                b = corner[(i + 1) % len(corner)]
                if a[1] is None or b[1] is None:
                    continue
                p, q = positions[a[0] - 1], positions[b[0] - 1]
                u, w = uvs[a[1] - 1], uvs[b[1] - 1]
                world = math.dist(p, q)
                texel = math.dist(u, w)
                if world > 1e-6 and texel > 1e-9:
                    ratios.append(world / texel)
    if not ratios:
        return None
    ratios.sort()
    return ratios[len(ratios) // 2]


# --------------------------------------------------------------------------- #
# per-mesh validation
# --------------------------------------------------------------------------- #
def validate_mesh(path, failures, verbose=False):
    positions, uvs, normals, groups, header_tile = parse_obj(path)
    name = path.stem
    used = set()
    faces = triangles = 0
    initial_failures = len(failures)

    for value in positions:
        if not all(math.isfinite(c) for c in value):
            failures.add(name, "non-finite vertex {0}".format(value))
    for value in uvs:
        if not all(math.isfinite(c) for c in value):
            failures.add(name, "non-finite uv {0}".format(value))

    for value in normals:
        if len(value) != 3 or not all(math.isfinite(c) for c in value):
            failures.add(name, "non-finite or malformed normal")

    for group_name, group_faces in groups:
        for corner in group_faces:
            if len(corner) < 3 or len({c[0] for c in corner}) != len(corner):
                failures.add(name, "face has fewer than three or repeated position indices")
                continue
            faces += 1
            triangles += len(corner) - 2
            points = []
            for vi, ti, ni, line in corner:
                if not 1 <= vi <= len(positions):
                    failures.add(name, "line {0}: v index {1} out of range".format(line, vi))
                    break
                if ti is None:
                    failures.add(name, "line {0}: face corner has no vt".format(line))
                    break
                if not 1 <= ti <= len(uvs):
                    failures.add(name, "line {0}: vt index {1} out of range".format(line, ti))
                    break
                if ni is None:
                    failures.add(name, "line {0}: face corner has no vn".format(line))
                    break
                if not 1 <= ni <= len(normals):
                    failures.add(name, "line {0}: vn index {1} out of range".format(line, ni))
                    break
                used.add(vi)
                points.append(positions[vi - 1])
            else:
                area_vector = newell(points)
                area = math.sqrt(sum(c * c for c in area_vector)) * 0.5
                if area <= AREA_EPSILON:
                    failures.add(name, "degenerate / zero-area face in group '{0}'"
                                 .format(group_name))
                    continue
                geometric = [c / (area * 2.0) for c in area_vector]
                for _, _, ni, line in corner:
                    emitted = normals[ni - 1]
                    length = math.sqrt(sum(c * c for c in emitted))
                    if not math.isfinite(length) or abs(length - 1.0) > 1e-4:
                        failures.add(name, "line {0}: vn not finite/unit length".format(line))
                        continue
                    dot = sum(a * b for a, b in zip(geometric, emitted))
                    if dot <= 0.0:
                        failures.add(name, "line {0}: vn disagrees with winding".format(line))

    if not positions or not faces or len(failures) > initial_failures:
        if not positions or not faces:
            failures.add(name, "empty geometry")
        return len(positions), faces, triangles, path.stat().st_size, dict.fromkeys("ABSCD", 0), 0

    unreferenced = len(positions) - len(used)
    if unreferenced:
        failures.add(name, "{0} unreferenced vertices".format(unreferenced))

    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    for i, axis in enumerate("xyz"):
        if lo[i] < ENVELOPE_MIN[i] - EPSILON or hi[i] > ENVELOPE_MAX[i] + EPSILON:
            failures.add(name, "bounds escape envelope on {0}: {1:.3f}..{2:.3f}"
                         .format(axis, lo[i], hi[i]))

    polygon_faces = [[positions[vi - 1] for vi, _, _, _ in corner]
                     for _, group_faces in groups for corner in group_faces]
    shells = shell_report(polygon_faces)
    solid_faces = {i for indices, klass, _, _ in shells if klass == SHELL_CLOSED
                   for i in indices}
    closed_count = validate_outward_winding(polygon_faces, name, failures)
    validate_spawn_clearance(polygon_faces, name, failures, solid_faces)
    if verbose:
        open_count = sum(1 for _, klass, _, _ in shells if klass == SHELL_OPEN)
        broken_count = sum(1 for _, klass, _, _ in shells if klass == SHELL_BROKEN)
        print("  topology components={0} closed_shells={1} open_surfaces={2} "
              "non_manifold={3}".format(len(shells), closed_count, open_count,
                                        broken_count))

    # ---- playable-space coherence -------------------------------------------
    components = connected_components(positions, groups)
    tally = {"A": 0, "S": 0, "B": 0, "C": 0, "D": 0}
    offending = 0
    for group_name, clo, chi, component_faces, face_indices in components:
        # A welded component counts as a closed solid when every face in it
        # belongs to a closed shell, so the floor-decal exception can never be
        # claimed by something that is actually a solid.
        closed = bool(face_indices) and all(i in solid_faces for i in face_indices)
        rule, detail = classify_component(clo, chi, component_faces, closed)
        if rule is None:
            offending += 1
            failures.add(name, "group '{0}': {1}".format(group_name, detail))
        else:
            tally[rule] += 1

    # ---- UV density ---------------------------------------------------------
    target = UV_TILE_TARGET.get(name)
    measured = measure_uv_tile(positions, uvs, groups)
    if target is None:
        failures.add(name, "no declared UV tile target")
    else:
        if header_tile is None:
            failures.add(name, "OBJ header does not declare a metres-per-tile value")
        elif abs(header_tile - target) > 5e-3:
            failures.add(name, "header declares {0:.2f} m per tile, target is {1:.2f}"
                         .format(header_tile, target))
        if measured is None:
            failures.add(name, "no measurable UV edges")
        elif abs(measured - target) > UV_TOLERANCE:
            failures.add(name, "measured UV density {0:.5f} m per tile, target is {1:.2f}"
                         .format(measured, target))

    if verbose:
        print("  groups={0} faces={1} tris={2} components={3} "
              "[A={4} S={5} B={6} C={7} D={8} bad={9}] uv={10:.3f} m/tile".format(
                  len(groups), faces, triangles, len(components),
                  tally["A"], tally["S"], tally["B"], tally["C"], tally["D"], offending,
                  measured if measured is not None else float("nan")))
    return len(positions), faces, triangles, path.stat().st_size, tally, offending


def validate_material(path, failures):
    data = json.loads(path.read_text(encoding="utf-8"))
    name = path.stem
    if data.get("materialType") != "Materials/Types/StandardPBR.materialtype":
        failures.add(name, "materialType is not StandardPBR")
    if data.get("materialTypeVersion") != 5:
        failures.add(name, "materialTypeVersion is not 5")
    properties = data.get("properties")
    if properties is None:
        properties = {}
        for flat, value in (data.get("propertyValues") or {}).items():
            group, _, key = flat.partition(".")
            properties.setdefault(group, {})[key] = value
    for group, entries in properties.items():
        if group not in VALID_KEYS:
            failures.add(name, "unknown property group '{0}'".format(group))
            continue
        for key, value in entries.items():
            if key not in VALID_KEYS[group]:
                failures.add(name, "unknown key '{0}.{1}'".format(group, key))
            if key.endswith("TextureMap") or key == "textureMap":
                if not (path.parent / value).is_file():
                    failures.add(name, "{0}.{1} -> missing file '{2}'"
                                 .format(group, key, value))
    emissive = properties.get("emissive")
    if emissive and emissive.get("unit") not in (None, "Ev100"):
        failures.add(name, "emissive.unit must be 'Ev100' in this engine version")


def _box_faces(lo, hi):
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    return [
        [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)],
        [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
        [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],
        [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)],
        [(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)],
        [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],
    ]


def selftest():
    """Prove the playable-space invariant still rejects what it is meant to.

    A validator that has quietly stopped detecting anything also reports PASS.
    These synthetic components pin each rule - and, critically, pin the failure
    case - so weakening the classifier to obtain a PASS breaks this first.
    """
    wall = WALL_INNER
    cases = [
        # Rejection cases are the real Block 26E C-1 offenders, at the AABBs the
        # pre-fix validator run actually reported. They are pinned here so the
        # defect cannot silently return through a future generator change.
        # (description, lo, hi, expected rule; None means "must be rejected")
        ("C-1 pylon: free-standing structural column",
         (-11.33, -6.43, 0.80), (-10.47, -5.57, 6.20), None),
        ("C-1 arch leg: hero-arch support on the deck",
         (-5.70, 7.88, 0.25), (-4.30, 9.12, 1.85), None),
        ("C-1 crate: free-standing prop on the open deck",
         (-6.80, -7.83, 0.00), (-5.80, -6.97, 0.84), None),
        ("C-1 barricade rail over the open deck",
         (-5.70, -5.28, 0.98), (-3.50, -5.12, 1.12), None),
        ("C-1 arch threshold kerb",
         (-5.93, 7.67, 0.25), (-4.07, 9.33, 0.35), None),
        ("knee-high kerb in open floor",
         (-1.0, 5.0, 0.00), (1.0, 6.0, 0.30), None),
        ("crate stood in the wall relief",
         (-7.9, wall + 0.01, 0.00), (-6.9, wall + 0.56, 0.84), "B"),
        ("roof truss chord",
         (-wall, -6.15, 6.20), (wall, -5.85, 7.00), "C"),
        ("deck plate", (-1.0, -1.0, -0.05), (0.94, 0.94, 0.00), "D"),
        ("painted marking plane", (-2.0, 3.0, 0.025), (0.0, 4.0, 0.025), "D"),
        ("armour plate on the left cover",
         (-2.95, -1.12, 0.29), (-1.55, -0.98, 0.81), "A"),
    ]
    problems = []
    for description, lo, hi, expected in cases:
        rule, detail = classify_component(lo, hi, _box_faces(lo, hi))
        if rule != expected:
            problems.append("{0}: expected {1}, got {2} ({3})".format(
                description, expected or "REJECT", rule or "REJECT", detail))
    return problems


def _plane_faces(lo, hi):
    """One horizontal quad - an open, zero-thickness component."""
    return [[(lo[0], lo[1], lo[2]), (hi[0], lo[1], lo[2]),
             (hi[0], hi[1], lo[2]), (lo[0], hi[1], lo[2])]]


def _tallest_interior_collider():
    if not WORLD["interior"]:
        raise AuthoritativeSourceError("no interior colliders to carry geometry")
    return max(WORLD["interior"], key=lambda c: c[2][2])


def reach_envelope_selftest():
    """Pin the Block 26E-R4 reach envelope, both directions.

    Block 26E-R1 treated floor_top + CapsuleHeight as the whole of player reach,
    so a solid at 1.95 m read as overhead when a plain jump from the deck already
    carries the capsule top past 3.4 m. These cases pin the replacement: what must
    still be rejected inside the envelope, what is legitimately clear of it, and -
    critically - that raising the ceiling did not simply exempt everything.

    Returns (problems, tokens) where tokens map the required outcome names to what
    the classifier actually did, so a regression shows up as a changed token.
    """
    name, clo, chi = _tallest_interior_collider()
    carried_lo = (clo[0] + 0.20, clo[1] + 0.20, chi[2] + 0.10)
    carried_hi = (chi[0] - 0.20, chi[1] - 0.20, chi[2] + 0.60)

    cases = (
        # (required token, lo, hi, face builder, required outcome)
        ("SOLID_INSIDE_STANDING_REACH",
         (-1.0, 5.0, 0.40), (0.0, 6.0, 1.40), _box_faces, "FAIL"),
        ("SOLID_ABOVE_STANDING_BUT_INSIDE_JUMP_OR_MANTLE_REACH",
         (-1.0, 5.0, REACH_Z + 0.40), (0.0, 6.0, REACH_Z + 1.20), _box_faces, "FAIL"),
        ("SOLID_ABOVE_CONSERVATIVE_REACH_ENVELOPE",
         (-1.0, 5.0, CONSERVATIVE_REACH_Z + 0.02),
         (0.0, 6.0, CONSERVATIVE_REACH_Z + 1.00), _box_faces, "PASS"),
        ("SOLID_SUPPORTED_BY_EXISTING_COLLIDER",
         carried_lo, carried_hi, _box_faces, "PASS"),
        ("OPEN_FLOOR_DETAIL_WITHIN_ALLOWED_RELIEF",
         (-2.0, 3.0, FLOOR_TOP + 0.025), (0.0, 4.0, FLOOR_TOP + 0.025),
         _plane_faces, "PASS"),
        ("OPEN_ELEVATED_DETAIL_INSIDE_REACH",
         (-2.0, 3.0, REACH_Z + 0.50), (0.0, 4.0, REACH_Z + 0.50),
         _plane_faces, "FAIL"),
    )

    problems = []
    tokens = {}
    for token, lo, hi, builder, required in cases:
        rule, detail = classify_component(lo, hi, builder(lo, hi))
        observed = "FAIL" if rule is None else "PASS"
        tokens[token] = observed
        if observed != required:
            problems.append("reach envelope regression: {0} required {1}, got {2} ({3})"
                            .format(token, required, observed, rule or detail))

    # The envelope has to be strictly above every individual reach it is built
    # from, or it is not an envelope.
    for label, value in (("jump", JUMP_REACH_Z), ("mantle", MANTLE_REACH_Z),
                         ("standing", REACH_Z)):
        if value >= CONSERVATIVE_REACH_Z:
            problems.append("reach envelope regression: {0} reach {1:.3f} is not "
                            "below the conservative envelope {2:.3f}"
                            .format(label, value, CONSERVATIVE_REACH_Z))
    if name is None:
        problems.append("reach envelope regression: no carrying collider")
    return problems, tokens


def _deck_stretched_tile_regression(failures):
    """Fail on a return to the Block 26D failure mode.

    Before Block 26E every quad got a full 0..1 UV tile regardless of size, so
    the 24 x 24 m deck received one stretched 512px tile. That reads as a smooth
    sheet rather than a floor. Guard it explicitly: the deck's UV extent has to
    span many tiles, not one.
    """
    deck = ASSET_DIR / "STW_ARENA_DECK_01.obj"
    if not deck.is_file():
        return
    positions, uvs, _, _, _ = parse_obj(deck)
    if not uvs or not positions:
        failures.add("STW_ARENA_DECK_01", "deck carries no UVs at all")
        return
    u_span = max(u for u, _ in uvs) - min(u for u, _ in uvs)
    v_span = max(v for _, v in uvs) - min(v for _, v in uvs)
    world = max(max(p[i] for p in positions) - min(p[i] for p in positions)
                for i in range(2))
    tiles = min(u_span, v_span)
    if tiles < world / 8.0:
        failures.add("STW_ARENA_DECK_01",
                     "deck UVs span only {0:.2f} tiles across {1:.1f} m - this is "
                     "the Block 26D single-stretched-tile regression"
                     .format(tiles, world))


def main():
    parser = argparse.ArgumentParser(description="Validate the STW arena kit.")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--assets", type=Path, default=None,
                        help="validate a kit generated outside the asset tree "
                             "(used to re-check pre-fix geometry; the collision "
                             "world and runtime table always come from source)")
    args = parser.parse_args()
    if args.assets is not None:
        global ASSET_DIR
        ASSET_DIR = args.assets
    failures = Failures()

    reach_problems, reach_tokens = reach_envelope_selftest()
    topology_problems, topology_tokens = topology_selftest()
    for problem in (selftest() + winding_selftest() + reach_problems
                    + topology_problems):
        failures.add("selftest", problem)

    try:
        import numpy
        if not numpy.__version__.startswith("1."):
            failures.add("toolchain",
                         "numpy {0} removes ndarray.ptp(), which "
                         "generate_stw_materials.py depends on".format(numpy.__version__))
    except ImportError:
        failures.add("toolchain", "numpy is not importable")

    for spawn_name, lo, hi in spawn_bounds():
        for collider_name, clo, chi in COLLIDERS:
            if capsule_intrudes(clo, chi, lo, hi):
                failures.add("spawn", spawn_name + " overlaps " + collider_name)

    pairs = parse_runtime_visual_assets(failures)

    total_v = total_f = total_t = total_b = 0
    total_tally = {"A": 0, "S": 0, "B": 0, "C": 0, "D": 0}
    total_offending = 0
    for visual_id, mesh_name, material_name in pairs:
        mesh_path = ASSET_DIR / "{0}.obj".format(mesh_name)
        material_path = ASSET_DIR / "{0}.material".format(material_name)
        if not mesh_path.is_file():
            failures.add(mesh_name, "missing mesh {0}".format(mesh_path.name))
            continue
        if not material_path.is_file():
            failures.add(mesh_name, "missing material {0}".format(material_path.name))
        if args.verbose:
            print("{0:<18} {1} -> {2}".format(visual_id, mesh_path.name, material_path.name))
        vertices, faces, triangles, size, tally, offending = validate_mesh(
            mesh_path, failures, args.verbose)
        total_v += vertices
        total_f += faces
        total_t += triangles
        total_b += size
        for key in total_tally:
            total_tally[key] += tally[key]
        total_offending += offending

    for material_name in sorted({m for _, _, m in pairs}):
        material_path = ASSET_DIR / "{0}.material".format(material_name)
        if material_path.is_file():
            validate_material(material_path, failures)

    # Superseded blockout assets must be gone, so OLD_BLOCKOUT_VISIBLE is
    # verifiable by absence rather than by inspection.
    for stale in ("STW_ARENA_01.obj", "STW_ARENA_COVER_01.obj",
                  "STW_ARENA_LANDMARK_01.obj", "STW_ARENA_TRIM_01.obj",
                  "STW_ARENA_01.mtl", "STW_ARENA_COVER_01.mtl",
                  "STW_ARENA_LANDMARK_01.mtl", "STW_ARENA_TRIM_01.mtl"):
        if (ASSET_DIR / stale).exists():
            failures.add("blockout", "superseded asset still present: {0}".format(stale))

    _deck_stretched_tile_regression(failures)

    print("COLLISION_WORLD_SOURCE={0} boxes={1} wall_inner={2:.2f} floor_top={3:.2f} "
          "detail_margin={4:.2f}".format(
              PHYSX_SOURCE.name, len(COLLIDERS), WALL_INNER, FLOOR_TOP, DETAIL_MARGIN))
    print("PLAYER_REACH standing_z={0:.3f} jump_apex={1:.3f} jump_reach_z={2:.3f} "
          "mantle_reach_z={3:.3f} conservative_reach_z={4:.3f} "
          "sources=PhysXPlayerRuntime.h:CapsuleHeight/CapsuleRadius,"
          "PhysXPlayerRuntime.cpp:MantleMaxHeight,PlayerSliceModel.h:JumpImpulseSpeed"
          .format(REACH_Z, WORLD["jump_apex"], JUMP_REACH_Z, MANTLE_REACH_Z,
                  CONSERVATIVE_REACH_Z))
    print("SELFTEST=PASS" if not (selftest() + winding_selftest() + reach_problems
                                 + topology_problems) else "SELFTEST=FAIL")
    for token in sorted(topology_tokens):
        print("{0}={1}".format(token, topology_tokens[token]))
    for token in sorted(reach_tokens):
        print("{0}={1}".format(token, reach_tokens[token]))
    print("RUNTIME_TABLE_ENTRIES={0}".format(len(pairs)))
    print("VALIDATED_MESHES={0}".format(len(pairs)))
    print("TOTAL_VERTICES={0}".format(total_v))
    print("TOTAL_FACES={0}".format(total_f))
    print("TOTAL_TRIANGLES={0}".format(total_t))
    print("TOTAL_OBJ_BYTES={0}".format(total_b))
    print("PLAYABLE_SPACE_COMPONENTS on_collider={0} carried_on_collider={1} "
          "outside_boundary={2} above_conservative_reach={3} non_blocking={4}".format(
              total_tally["A"], total_tally["S"], total_tally["B"],
              total_tally["C"], total_tally["D"]))
    print("REACHABLE_UNCOLLIDED_SOLID_COMPONENTS={0}".format(total_offending))
    if len(failures):
        print("VALIDATION=FAIL failures={0}".format(len(failures)))
        for item in failures.items:
            print("  {0}".format(item))
        return 1
    print("TOPOLOGY_OUTWARD_NORMALS_UV_COLLISION_SPAWN_MATERIALS=PASS")
    print("VALIDATION=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
