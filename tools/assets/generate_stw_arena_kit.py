#!/usr/bin/env python3
"""Generate the STW production modular arena kit (Block 26E, P3 + P4).

Authors the whole visible arena as nine deterministic OBJ meshes, one per PBR
material family, because the runtime binds exactly one material per mesh through
the mesh feature processor's default custom-material slot (see
ArenaPresentation.cpp: MeshHandleDescriptor + SetCustomMaterials). Splitting by
family is what lets the deck be re-coated as a dielectric surface independently
of the walls and structure.

Axis / import convention (do not "correct" this):
    Geometry is authored raw Z-up, +Y forward, in metres, unrotated. O3DE's
    scene importer rewrites OBJ vertices as (-x, z, y); every STW presentation
    applies the compensating rotation Matrix3x3::CreateFromColumns(-X, Z, Y) at
    bind time. That pairing is deliberate and is what commit d4ee1d4
    ("correct native arena import orientation") established.

UV convention:
    UVs are world-planar and scaled by metres-per-tile, NOT one 0..1 tile per
    face. The previous generator gave every quad a full 0..1 tile regardless of
    size, so the 24x24 m deck received a single stretched 512px tile - the main
    reason the deck read as one smooth sheet rather than a floor.

Emitting vt is mandatory: sceneassetimporter.setreg uses "LeaveSceneDataAsIs",
so O3DE generates no UVs, and with no UVs MikkT tangent generation is skipped
entirely.

No mtllib / usemtl is emitted; OBJ `g` groups are geometry groups only.

Usage:
    python3 tools/assets/generate_stw_arena_kit.py [--check]
"""

import argparse
import math
import random
from pathlib import Path

ASSET_DIR = Path(__file__).parents[2] / "stw-o3de/Project/Assets/Environment/STW_ARENA_01"

# Quantisation for emitted coordinates, so the files are byte-identical on every
# machine regardless of float printing.
DECIMALS = 6

# --------------------------------------------------------------------------- #
# Arena constants.
#
# These MIRROR the gameplay-authoritative PhysX static boxes in
# PhysXPlayerRuntime.cpp:54-61. They are duplicated here as visual targets only -
# this generator never defines collision, spawn or bounds authority. Keeping the
# visuals flush with the collision is what makes cover read truthfully.
# --------------------------------------------------------------------------- #
ARENA_HALF = 12.0           # collision floor is 24 x 24 centred on the origin
WALL_INNER = 11.75          # inner face of the collision walls - nothing visual crosses it
WALL_RELIEF = 0.20          # depth available for wall detail, all of it behind WALL_INNER
WALL_FACE = WALL_INNER + WALL_RELIEF    # backing-slab surface the modules stand proud of
WALL_OUTER = 12.35          # outermost visual extent
WALL_SLAB = WALL_OUTER - WALL_FACE      # backing-slab thickness
WALL_HEIGHT = 6.0           # visual only; collision walls remain 4 m
DECK_TOP = 0.0              # collision floor top plane
DECK_THICKNESS = 0.80
PLATE_HALF = 11.0           # deck plates cover +-11; the outer ring is framing
PLATE_SIZE = 2.0            # one deck plate == one UV tile
PLATE_GAP = 0.06            # recessed seam between plates
PLATE_RISE = 0.04           # plate top sits at DECK_TOP, base slab is recessed

TRUSS_Y = (-6.0, -2.0, 2.0, 6.0, 10.0)
TRUSS_BOTTOM = 6.2
TRUSS_TOP = 7.0

# Collision cover boxes: centre (+-2.25, 0, 1.25), size (1.5, 2.0, 2.5).
COVER_CENTRES = (-2.25, 2.25)
COVER_SIZE = (1.5, 2.0, 2.5)
# Collision "STW Step": centre (5, -2, 0.125), size (2, 2, 0.25).
STEP_CENTRE = (5.0, -2.0, 0.125)
STEP_SIZE = (2.0, 2.0, 0.25)

ARCH_Y = 8.5
ARCH_LEG_X = 5.0

# --------------------------------------------------------------------------- #
# Block 26E-R1 playable-space budget.
#
# PhysXPlayerRuntime builds exactly eight static boxes (floor, four perimeter
# walls, two covers, one step) and gameplay owns that list - this generator must
# never require a new one. So presentation geometry either sits on one of those
# colliders, stays behind the wall plane, clears the standing player, or is flush
# enough to walk over. tools/assets/validate_stw_obj.py enforces exactly that,
# per component and purely by position.
# --------------------------------------------------------------------------- #
# --------------------------------------------------------------------------- #
# Block 26E-R4 conservative reach envelope.
#
# Block 26E-R1 used the standing capsule height alone (1.80 m) as "reach", which
# ignored the vertical reach gameplay actually grants. These constants MIRROR the
# player-movement values gameplay owns - this generator never defines movement -
# and exist here only as visual clearance targets:
#
#   CAPSULE_HEIGHT      PhysXPlayerRuntime.h:18    CapsuleHeight      = 1.80f
#   CAPSULE_RADIUS      PhysXPlayerRuntime.h:20    CapsuleRadius      = 0.35f
#   STEP_HEIGHT         PhysXPlayerRuntime.h:21    StepHeight         = 0.30f
#   MANTLE_MAX_HEIGHT   PhysXPlayerRuntime.cpp:29  MantleMaxHeight    = 0.80f
#   JUMP_IMPULSE_SPEED  PlayerSliceModel.h:145     JumpImpulseSpeed   = 5.5f
#
# A jump is one tick of vertical takeoff velocity (PlayerSliceModel.cpp:487,
# GetDesiredVelocity -> velocity.SetZ(m_jumpImpulseThisTick)); PhysX then owns the
# arc (PhysXPlayerRuntime.cpp:314-320, Synchronize -> SetFallingVelocity) under
# CharacterGameplayConfiguration::m_gravityMultiplier = 1.0f
# (PhysXPlayerRuntime.cpp:82). No world-gravity constant is committed anywhere in
# this repository, so the apex cannot be proven exactly; G_CONSERVATIVE is
# deliberately below the O3DE PhysX default so the derived apex - and therefore
# the envelope - is an upper bound rather than an estimate.
# --------------------------------------------------------------------------- #
CAPSULE_HEIGHT = 1.80
CAPSULE_RADIUS = 0.35
STEP_HEIGHT = 0.30
MANTLE_MAX_HEIGHT = 0.80
JUMP_IMPULSE_SPEED = 5.5
G_CONSERVATIVE = 9.0
JUMP_APEX = JUMP_IMPULSE_SPEED ** 2 / (2.0 * G_CONSERVATIVE)

REACH_Z = DECK_TOP + CAPSULE_HEIGHT     # standing capsule top; reporting only
# Highest point a player collision capsule can occupy anywhere in the arena:
# traverse/mantle onto a ledge, jump from it, stand full height, plus the
# capsule's own rounded-cap radius. Presentation geometry that is neither carried
# by a collider nor behind the wall plane must begin above this.
CONSERVATIVE_REACH_Z = (DECK_TOP + MANTLE_MAX_HEIGHT + JUMP_APEX
                        + CAPSULE_HEIGHT + CAPSULE_RADIUS)
# Underside of every unsupported solid suspended over the deck, snapped up to a
# clean 5 cm so it is strictly above the envelope rather than exactly on it.
ARCH_CLEAR_Z = math.ceil(CONSERVATIVE_REACH_Z / 0.05) * 0.05
assert ARCH_CLEAR_Z >= CONSERVATIVE_REACH_Z

FLUSH_Z = 0.15              # floor relief the player walks over, never into
PROP_DEPTH = 0.55           # wall-hugging prop depth, inside the relief zone

# Metres per texture tile, per family.
TILE_DECK = 2.0
TILE_WALL = 1.5
TILE_STRUCT = 1.0
TILE_COVER = 1.0
TILE_TRIM = 1.0
TILE_LANDMARK = 1.0
TILE_PROP = 0.75
TILE_MARK = 1.0

MARK_OFFSET = 0.025         # lift markings off their host surface (z-fight guard)
# Wall-mounted dressing lives in the relief zone between the collision plane and
# the backing slab, so nothing hangs into playable space.
WALL_MOUNT = WALL_INNER + 0.10


# --------------------------------------------------------------------------- #
# OBJ builder
# --------------------------------------------------------------------------- #
class ObjKit:
    """Accumulates welded positions plus per-corner UVs and normals."""

    def __init__(self, name, tile):
        self.name = name
        self.tile = tile
        self.positions = []
        self._position_index = {}
        self.uvs = []
        self._uv_index = {}
        self.normals = []
        self._normal_index = {}
        self.groups = []

    def _add(self, store, index, value):
        key = tuple(round(c, DECIMALS) for c in value)
        if key not in index:
            store.append(key)
            index[key] = len(store)  # OBJ indices are 1-based
        return index[key]

    def begin_group(self, name):
        self.groups.append((name, []))

    def add_polygon(self, points, tile=None):
        """Add one convex polygon, wound counter-clockwise seen from outside.

        The normal is derived from the winding so the geometric normal and the
        emitted vn always agree - Atom's back-face culling depends on it.
        """
        normal = _face_normal(points)
        if normal is None:
            raise ValueError(
                "degenerate polygon in group '{0}' of {1}".format(
                    self.groups[-1][0] if self.groups else "?", self.name))
        tile = self.tile if tile is None else tile
        normal_index = self._add(self.normals, self._normal_index, normal)
        corner = []
        for point in points:
            position_index = self._add(self.positions, self._position_index, point)
            uv_index = self._add(self.uvs, self._uv_index, _planar_uv(point, normal, tile))
            corner.append((position_index, uv_index, normal_index))
        if not self.groups:
            self.begin_group("default")
        self.groups[-1][1].append(corner)

    def counts(self):
        faces = sum(len(faces) for _, faces in self.groups)
        triangles = sum(len(corner) - 2 for _, faces in self.groups for corner in faces)
        return len(self.positions), faces, triangles, len(self.groups)

    def bounds(self):
        xs = [p[0] for p in self.positions]
        ys = [p[1] for p in self.positions]
        zs = [p[2] for p in self.positions]
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def serialise(self):
        lines = [
            "# {0} - original STW production arena kit geometry".format(self.name),
            "# Generated deterministically by tools/assets/generate_stw_arena_kit.py",
            "# Axis convention: raw Z-up, +Y forward, metres, unrotated. O3DE imports OBJ",
            "# vertices as (-x, z, y) and the runtime applies the compensating rotation",
            "# Matrix3x3::CreateFromColumns(-X, Z, Y) at bind time.",
            "# UVs are world-planar, scaled to {0:.2f} m per tile.".format(self.tile),
            "# No mtllib / usemtl: the runtime binds one Atom StandardPBR material through",
            "# the mesh feature processor's default custom material slot.",
            "o {0}".format(self.name),
        ]
        for position in self.positions:
            lines.append("v {0:.6f} {1:.6f} {2:.6f}".format(*position))
        for uv in self.uvs:
            lines.append("vt {0:.6f} {1:.6f}".format(*uv))
        for normal in self.normals:
            lines.append("vn {0:.6f} {1:.6f} {2:.6f}".format(*normal))
        for name, faces in self.groups:
            lines.append("g {0}".format(name))
            for corner in faces:
                lines.append("f " + " ".join(
                    "{0}/{1}/{2}".format(p, t, n) for p, t, n in corner))
        return "\n".join(lines) + "\n"


def _normalise(vector):
    length = math.sqrt(sum(c * c for c in vector))
    if length < 1e-9:
        return None
    return tuple(c / length for c in vector)


def _face_normal(points):
    """Newell's method - stable for n-gons and near-degenerate triangles."""
    nx = ny = nz = 0.0
    count = len(points)
    for i in range(count):
        cx, cy, cz = points[i]
        nxt = points[(i + 1) % count]
        nx += (cy - nxt[1]) * (cz + nxt[2])
        ny += (cz - nxt[2]) * (cx + nxt[0])
        nz += (cx - nxt[0]) * (cy + nxt[1])
    return _normalise((nx, ny, nz))


def _planar_uv(point, normal, tile):
    """World-planar UV: project onto the two axes the face most faces away from."""
    ax, ay, az = abs(normal[0]), abs(normal[1]), abs(normal[2])
    if az >= ax and az >= ay:
        return (point[0] / tile, point[1] / tile)
    if ax >= ay:
        return (point[1] / tile, point[2] / tile)
    return (point[0] / tile, point[2] / tile)


# --------------------------------------------------------------------------- #
# primitives
# --------------------------------------------------------------------------- #
def add_box(kit, centre, size, tile=None):
    """Closed axis-aligned box. Faces wound counter-clockwise seen from outside."""
    hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
    x0, x1 = centre[0] - hx, centre[0] + hx
    y0, y1 = centre[1] - hy, centre[1] + hy
    z0, z1 = centre[2] - hz, centre[2] + hz
    kit.add_polygon([(x1, y1, z0), (x1, y0, z0), (x0, y0, z0), (x0, y1, z0)], tile)  # -Z
    kit.add_polygon([(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)], tile)  # +Z
    kit.add_polygon([(x1, y0, z0), (x1, y0, z1), (x0, y0, z1), (x0, y0, z0)], tile)  # -Y
    kit.add_polygon([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], tile)  # +Y
    kit.add_polygon([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], tile)  # -X
    kit.add_polygon([(x1, y1, z0), (x1, y1, z1), (x1, y0, z1), (x1, y0, z0)], tile)  # +X


def add_chamfered_box(kit, centre, size, chamfer, tile=None):
    """Box with its four vertical edges chamfered - an octagonal prism.

    This is the workhorse for anything that must not read as a rectangle:
    pylons, cover bodies, arch supports, crates.
    """
    hx, hy, hz = size[0] * 0.5, size[1] * 0.5, size[2] * 0.5
    chamfer = min(chamfer, hx * 0.9, hy * 0.9)
    x0, x1 = centre[0] - hx, centre[0] + hx
    y0, y1 = centre[1] - hy, centre[1] + hy
    z0, z1 = centre[2] - hz, centre[2] + hz
    # Counter-clockwise footprint seen from +Z.
    ring = [
        (x0 + chamfer, y0), (x1 - chamfer, y0),
        (x1, y0 + chamfer), (x1, y1 - chamfer),
        (x1 - chamfer, y1), (x0 + chamfer, y1),
        (x0, y1 - chamfer), (x0, y0 + chamfer),
    ]
    for i in range(len(ring)):
        px, py = ring[i]
        qx, qy = ring[(i + 1) % len(ring)]
        kit.add_polygon([(px, py, z0), (qx, qy, z0), (qx, qy, z1), (px, py, z1)], tile)
    kit.add_polygon([(px, py, z1) for px, py in ring], tile)              # +Z cap
    kit.add_polygon([(px, py, z0) for px, py in reversed(ring)], tile)    # -Z cap


def add_tapered_box(kit, centre, bottom_size, top_size, height, tile=None):
    """Frustum with a rectangular section - tapered structural supports."""
    bx, by = bottom_size[0] * 0.5, bottom_size[1] * 0.5
    tx, ty = top_size[0] * 0.5, top_size[1] * 0.5
    cx, cy = centre[0], centre[1]
    z0 = centre[2] - height * 0.5
    z1 = centre[2] + height * 0.5
    bottom = [(cx - bx, cy - by), (cx + bx, cy - by), (cx + bx, cy + by), (cx - bx, cy + by)]
    top = [(cx - tx, cy - ty), (cx + tx, cy - ty), (cx + tx, cy + ty), (cx - tx, cy + ty)]
    for i in range(4):
        bp, bq = bottom[i], bottom[(i + 1) % 4]
        tp, tq = top[i], top[(i + 1) % 4]
        kit.add_polygon([(bp[0], bp[1], z0), (bq[0], bq[1], z0),
                         (tq[0], tq[1], z1), (tp[0], tp[1], z1)], tile)
    kit.add_polygon([(p[0], p[1], z1) for p in top], tile)
    kit.add_polygon([(p[0], p[1], z0) for p in reversed(bottom)], tile)


def add_quad_z(kit, centre, size, z, tile=None):
    """Single upward-facing quad - deck markings."""
    hx, hy = size[0] * 0.5, size[1] * 0.5
    x0, x1 = centre[0] - hx, centre[0] + hx
    y0, y1 = centre[1] - hy, centre[1] + hy
    kit.add_polygon([(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], tile)


def add_quad_wall(kit, centre, size, axis, sign, tile=None):
    """Single wall-facing quad - wall signage. axis is 0 (X) or 1 (Y)."""
    hw, hh = size[0] * 0.5, size[1] * 0.5
    cx, cy, cz = centre
    z0, z1 = cz - hh, cz + hh
    if axis == 1:  # faces along -Y / +Y
        x0, x1 = cx - hw, cx + hw
        if sign < 0:
            kit.add_polygon([(x0, cy, z0), (x1, cy, z0), (x1, cy, z1), (x0, cy, z1)], tile)
        else:
            kit.add_polygon([(x1, cy, z0), (x0, cy, z0), (x0, cy, z1), (x1, cy, z1)], tile)
    else:          # faces along -X / +X
        y0, y1 = cy - hw, cy + hw
        if sign < 0:
            kit.add_polygon([(cx, y1, z0), (cx, y0, z0), (cx, y0, z1), (cx, y1, z1)], tile)
        else:
            kit.add_polygon([(cx, y0, z0), (cx, y1, z0), (cx, y1, z1), (cx, y0, z1)], tile)


def add_bolt(kit, centre, radius, height, tile=None):
    """Small hexagonal fastener cap - restrained, used only where it reads."""
    z0 = centre[2]
    z1 = centre[2] + height
    ring = []
    for i in range(6):
        a = math.pi * 2.0 * i / 6.0
        ring.append((centre[0] + radius * math.cos(a), centre[1] + radius * math.sin(a)))
    for i in range(6):
        px, py = ring[i]
        qx, qy = ring[(i + 1) % 6]
        kit.add_polygon([(px, py, z0), (qx, qy, z0), (qx, qy, z1), (px, py, z1)], tile)
    kit.add_polygon([(px, py, z1) for px, py in ring], tile)


def add_conduit(kit, start, end, radius, sides=6, tile=None):
    """Axis-aligned pipe run between two points, as a low-sided prism.

    Side quads must be wound so the geometric normal points radially outward for
    a run along any axis, in either direction. The ring is traced CCW in the
    (other[0], other[1]) plane; that winding is outward only when the basis
    (e_other0, e_other1, e_axis) is right-handed - i.e. (other0, other1, axis) is
    a cyclic permutation of (0, 1, 2). With other == [i != axis] that holds for
    the X and Z runs but NOT the Y run (other == [X, Z], and X x Z = -Y), so
    every +/-Y conduit used to be wound inward. The cross-section shape is left
    exactly as before; only the side-quad winding is flipped for the left-handed
    axis, so X and Z conduit geometry is byte-identical.
    """
    axis = max(range(3), key=lambda i: abs(end[i] - start[i]))
    other = [i for i in range(3) if i != axis]
    right_handed = ((other[0] + 1) % 3 == other[1]) and ((other[1] + 1) % 3 == axis)
    ring = []
    for i in range(sides):
        a = math.pi * 2.0 * i / sides
        ring.append((radius * math.cos(a), radius * math.sin(a)))
    def point(at, offset):
        p = list(at)
        p[other[0]] += offset[0]
        p[other[1]] += offset[1]
        return tuple(p)
    forward = end[axis] - start[axis]
    # For a right-handed axis the CCW ring is outward and only a reversed run
    # needs flipping; for a left-handed axis it is the other way round.
    reverse_winding = (forward < 0) != (not right_handed)
    for i in range(sides):
        a = ring[i]
        b = ring[(i + 1) % sides]
        quad = [point(start, a), point(start, b), point(end, b), point(end, a)]
        if reverse_winding:
            quad.reverse()
        kit.add_polygon(quad, tile)


# --------------------------------------------------------------------------- #
# DECK - segmented plates, seams, channels, hatches, perimeter framing
# --------------------------------------------------------------------------- #
def build_deck():
    kit = ObjKit("STW_ARENA_DECK_01", TILE_DECK)
    rng = random.Random(2601)

    # Recessed base slab. Plate tops land exactly on DECK_TOP so the player
    # stands on the same plane the collision floor defines.
    kit.begin_group("deck_substrate")
    slab_top = DECK_TOP - PLATE_RISE
    add_box(kit, (0.0, 0.0, slab_top - DECK_THICKNESS * 0.5),
            (ARENA_HALF * 2.0, ARENA_HALF * 2.0, DECK_THICKNESS))

    channels_x = (-7.0, 7.0)      # service runs along X
    channels_y = (-9.0, 9.0)      # service runs along Y
    hatches = {(-5.0, -5.0), (7.0, 3.0), (-7.0, 7.0), (3.0, -9.0)}

    def in_channel(cx, cy):
        for c in channels_x:
            if abs(cy - c) < PLATE_SIZE * 0.5:
                return True
        for c in channels_y:
            if abs(cx - c) < PLATE_SIZE * 0.5:
                return True
        return False

    kit.begin_group("deck_plates")
    steps = int(PLATE_HALF * 2.0 / PLATE_SIZE)
    plate_span = PLATE_SIZE - PLATE_GAP
    for iy in range(steps):
        for ix in range(steps):
            cx = -PLATE_HALF + PLATE_SIZE * (ix + 0.5)
            cy = -PLATE_HALF + PLATE_SIZE * (iy + 0.5)
            if in_channel(cx, cy):
                continue
            if (cx, cy) in hatches:
                continue
            # A minority of plates sit fractionally proud, so grazing light
            # catches the seams instead of reading as one flat sheet.
            rise = PLATE_RISE + (0.012 if rng.random() < 0.22 else 0.0)
            add_box(kit, (cx, cy, DECK_TOP - rise * 0.5), (plate_span, plate_span, rise))

    kit.begin_group("deck_service_channels")
    for cy in channels_x:
        add_box(kit, (0.0, cy, slab_top - 0.06), (PLATE_HALF * 2.0, 0.62, 0.12))
        for i in range(22):                      # grating bars
            bx = -PLATE_HALF + 0.5 + i
            add_box(kit, (bx, cy, DECK_TOP - 0.018), (0.34, 0.60, 0.036))
    for cx in channels_y:
        add_box(kit, (cx, 0.0, slab_top - 0.06), (0.62, PLATE_HALF * 2.0, 0.12))
        for i in range(22):
            by = -PLATE_HALF + 0.5 + i
            add_box(kit, (cx, by, DECK_TOP - 0.018), (0.60, 0.34, 0.036))

    kit.begin_group("deck_maintenance_hatches")
    for cx, cy in sorted(hatches):
        add_box(kit, (cx, cy, DECK_TOP - 0.05), (plate_span, plate_span, 0.10))
        add_box(kit, (cx, cy, DECK_TOP - 0.015), (plate_span - 0.28, plate_span - 0.28, 0.03))
        for sx in (-1, 1):
            for sy in (-1, 1):
                add_bolt(kit, (cx + sx * (plate_span * 0.5 - 0.16),
                               cy + sy * (plate_span * 0.5 - 0.16),
                               DECK_TOP), 0.055, 0.028)

    # Transition plates where traffic concentrates: the arch threshold and the step.
    # These overlay the plate field, so they stand proud rather than sharing the
    # DECK_TOP plane - coplanar top faces there would z-fight with the plates.
    kit.begin_group("deck_transition_plates")
    for centre, size in (((0.0, ARCH_Y), (11.2, 1.2)),
                         ((STEP_CENTRE[0], STEP_CENTRE[1] - 1.6), (2.4, 0.8)),
                         ((0.0, -9.6), (6.0, 1.0))):
        add_box(kit, (centre[0], centre[1], DECK_TOP - 0.004), (size[0], size[1], 0.032))

    # Perimeter framing ring: raised edge structure between the plates and walls.
    kit.begin_group("deck_perimeter_frame")
    frame_mid = (PLATE_HALF + WALL_INNER) * 0.5
    frame_width = WALL_INNER - PLATE_HALF
    # The +-Y runs span the full width; the +-X runs stop at PLATE_HALF so the
    # ring butts at the corners instead of overlapping with coplanar top faces.
    for sign in (-1.0, 1.0):
        add_box(kit, (0.0, sign * frame_mid, DECK_TOP - 0.04),
                (WALL_INNER * 2.0, frame_width, 0.08))
        add_box(kit, (sign * frame_mid, 0.0, DECK_TOP - 0.04),
                (frame_width, PLATE_HALF * 2.0, 0.08))
    for sign in (-1.0, 1.0):
        add_box(kit, (0.0, sign * (PLATE_HALF + 0.06), DECK_TOP - 0.06),
                (PLATE_HALF * 2.0, 0.12, 0.12))
        add_box(kit, (sign * (PLATE_HALF + 0.06), 0.0, DECK_TOP - 0.06),
                (0.12, PLATE_HALF * 2.0, 0.12))
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            add_bolt(kit, (sx * frame_mid, sy * frame_mid, DECK_TOP), 0.07, 0.035)
    return kit


# --------------------------------------------------------------------------- #
# WALLS - framed modules with recessed panels, ribs, rails, caps, bays, vents
# --------------------------------------------------------------------------- #
def _wall_module(kit, base, forward, right, width, variant):
    """One 2 m wall module standing proud of the backing slab.

    `base` sits on the slab face (WALL_FACE); `forward` points into the arena.
    `proud` is how far a piece's interior-facing surface stands off that slab
    face, and the piece occupies [proud - thick, proud]. Every proud value here
    is <= WALL_RELIEF, so no wall detail ever crosses the WALL_INNER collision
    plane into playable space. The exposed slab face between the proud frame
    members IS the recessed panel field - drawing a separate "recessed" box
    would bury it inside the slab and render nothing.
    """
    def slab(along, proud, thick, z, w, h):
        offset = proud - thick * 0.5
        centre = (base[0] + right[0] * along + forward[0] * offset,
                  base[1] + right[1] * along + forward[1] * offset,
                  z)
        size = (w, thick, h) if abs(right[0]) > 0.5 else (thick, w, h)
        add_box(kit, centre, size)

    half = width * 0.5
    # Outer structural frame: two stiles, a head and a sill.
    slab(-half + 0.10, 0.20, 0.20, WALL_HEIGHT * 0.5, 0.20, WALL_HEIGHT)
    slab(half - 0.10, 0.20, 0.20, WALL_HEIGHT * 0.5, 0.20, WALL_HEIGHT)
    slab(0.0, 0.20, 0.20, WALL_HEIGHT - 0.16, width, 0.32)
    slab(0.0, 0.20, 0.20, 0.20, width, 0.40)
    # Vertical ribs, standing shallower than the frame.
    for offset in (-0.55, 0.0, 0.55):
        slab(offset, 0.13, 0.13, WALL_HEIGHT * 0.5 + 0.10, 0.14, WALL_HEIGHT - 1.30)
    # Lower protective rail and upper cap.
    slab(0.0, 0.20, 0.14, 1.05, width, 0.26)
    slab(0.0, 0.20, 0.22, WALL_HEIGHT + 0.14, width + 0.08, 0.28)

    if variant == 1:
        # Service bay: a shallow backing pan with hardware mounted proud of it.
        slab(0.0, 0.12, 0.12, 2.30, width - 0.90, 1.50)
        slab(-0.30, 0.19, 0.14, 2.55, 0.42, 0.62)
        slab(0.34, 0.19, 0.12, 2.20, 0.30, 0.40)
    elif variant == 2:
        # Vent grille: louvre stack standing in front of its frame.
        slab(0.0, 0.10, 0.10, 3.10, width - 1.10, 1.30)
        for i in range(6):
            slab(0.0, 0.16, 0.05, 2.58 + i * 0.20, width - 1.30, 0.10)
    elif variant == 3:
        # Technical seam band plus a shallow panel split.
        slab(0.0, 0.08, 0.08, 3.40, width - 0.30, 0.10)
        slab(0.0, 0.05, 0.05, 4.30, width - 0.60, 1.10)


def build_walls():
    kit = ObjKit("STW_ARENA_WALL_01", TILE_WALL)
    module = 2.0
    count = int(WALL_INNER * 2.0 / module)

    sides = (
        ("wall_north", (0.0, -1.0), (1.0, 0.0), lambda a: (a, WALL_FACE)),
        ("wall_south", (0.0, 1.0), (-1.0, 0.0), lambda a: (-a, -WALL_FACE)),
        ("wall_east", (-1.0, 0.0), (0.0, -1.0), lambda a: (WALL_FACE, -a)),
        ("wall_west", (1.0, 0.0), (0.0, 1.0), lambda a: (-WALL_FACE, a)),
    )
    slab_mid = (WALL_FACE + WALL_OUTER) * 0.5

    for name, forward, right, place in sides:
        kit.begin_group(name)
        # Backing slab: the carcass the modules stand proud of. The X-axis slabs
        # stop at WALL_FACE so they butt against the Y-axis slabs rather than
        # overlapping them in the corners with coplanar faces.
        if abs(right[0]) > 0.5:
            add_box(kit, (0.0, place(0.0)[1] + (slab_mid - WALL_FACE) * (1.0 if place(0.0)[1] > 0 else -1.0),
                          WALL_HEIGHT * 0.5),
                    (WALL_OUTER * 2.0, WALL_SLAB, WALL_HEIGHT))
        else:
            add_box(kit, (place(0.0)[0] + (slab_mid - WALL_FACE) * (1.0 if place(0.0)[0] > 0 else -1.0),
                          0.0, WALL_HEIGHT * 0.5),
                    (WALL_SLAB, WALL_FACE * 2.0, WALL_HEIGHT))
        for i in range(count):
            along = -(count * module) * 0.5 + module * (i + 0.5)
            cx, cy = place(along)
            # Deterministic variation, coherent rather than random-looking.
            variant = (0, 1, 0, 2, 0, 3)[i % 6]
            _wall_module(kit, (cx, cy, 0.0), (forward[0], forward[1], 0.0),
                         (right[0], right[1], 0.0), module, variant)
    return kit


# --------------------------------------------------------------------------- #
# STRUCTURE - pylons, roof trusses, tie beams
# --------------------------------------------------------------------------- #
def _wall_box(kit, axis, sign, along, along_size, near, far, z, height,
              chamfer=None, tile=None):
    """Place a box hugging one perimeter wall.

    `near` and `far` are depths measured outward from WALL_INNER, so a caller
    physically cannot author something that reaches into playable space: every
    box this emits starts at or behind the collision plane. `axis` 1 mounts on a
    +-Y wall (the box runs along X), `axis` 0 mounts on a +-X wall.
    """
    depth = far - near
    centre_v = sign * (WALL_INNER + (near + far) * 0.5)
    if axis == 1:
        centre, size = (along, centre_v, z), (along_size, depth, height)
    else:
        centre, size = (centre_v, along, z), (depth, along_size, height)
    if chamfer is None:
        add_box(kit, centre, size, tile)
    else:
        add_chamfered_box(kit, centre, size, chamfer, tile)


def _wall_pylon(kit, axis, sign, along):
    """Full-height structural column engaged with the perimeter wall.

    Block 26E authored these as free-standing pylons at |coord| == 10.90, which
    put a 0.86 m square, 5.4 m tall solid squarely inside the traversable volume.
    PhysXPlayerRuntime defines no collider there and gameplay owns collision, so
    the player walked through them. Standing the same column in the wall relief
    keeps the vertical silhouette and the truss landing while moving every face
    to or behind the collision plane.
    """
    height = TRUSS_BOTTOM
    front = 0.01                       # 1 cm behind the collision plane
    back = WALL_OUTER - WALL_INNER      # 0.60 m of relief to work in

    # Foot / base plate.
    _wall_box(kit, axis, sign, along, 1.30, front, back, 0.09, 0.18, 0.16)
    _wall_box(kit, axis, sign, along, 1.05, front + 0.05, back, 0.30, 0.26, 0.12)
    # Outer structural shell, chamfered so it never reads as a rectangle.
    _wall_box(kit, axis, sign, along, 0.86, front + 0.05, back - 0.04,
              height * 0.5 + 0.40, height - 0.80, 0.19)
    # Recessed inner core, visible past the shell relief.
    _wall_box(kit, axis, sign, along, 0.62, front + 0.13, back - 0.10,
              height * 0.5 + 0.40, height - 0.30, 0.10)
    # Layered armour / protection panels on the lower body.
    for i, z in enumerate((1.15, 1.95, 2.75)):
        inset = 0.02 * i
        _wall_box(kit, axis, sign, along, 0.96 - inset, front, front + 0.30, z, 0.30)
        _wall_box(kit, axis, sign, along, 0.34, front, front + 0.44, z, 0.30)
    # Connection brackets to the truss chord above.
    for offset in (-0.50, 0.50):
        _wall_box(kit, axis, sign, along + offset, 0.22,
                  front + 0.08, front + 0.50, height - 0.55, 0.42)
    # Top connection block.
    _wall_box(kit, axis, sign, along, 1.10, front, back, height - 0.14, 0.28, 0.14)
    for offset in (-0.44, 0.44):
        depth = WALL_INNER + front + 0.22
        centre = ((along + offset, sign * depth, 0.18) if axis == 1
                  else (sign * depth, along + offset, 0.18))
        add_bolt(kit, centre, 0.06, 0.03)


def _truss(kit, y):
    span = WALL_INNER
    # Top and bottom chords.
    add_box(kit, (0.0, y, TRUSS_TOP - 0.11), (span * 2.0, 0.30, 0.22))
    add_box(kit, (0.0, y, TRUSS_BOTTOM + 0.11), (span * 2.0, 0.30, 0.22))
    # Web members: alternating diagonals approximated by short angled posts.
    steps = 11
    for i in range(steps):
        cx = -span + (i + 0.5) * (span * 2.0 / steps)
        add_box(kit, (cx, y, (TRUSS_TOP + TRUSS_BOTTOM) * 0.5), (0.13, 0.20, TRUSS_TOP - TRUSS_BOTTOM))
        if i % 2 == 0:
            add_box(kit, (cx, y, TRUSS_BOTTOM + 0.30), (0.9, 0.14, 0.10))
    # End brackets.
    for sx in (-1.0, 1.0):
        add_box(kit, (sx * (span - 0.20), y, (TRUSS_TOP + TRUSS_BOTTOM) * 0.5),
                (0.40, 0.36, TRUSS_TOP - TRUSS_BOTTOM + 0.24))


def build_struct():
    kit = ObjKit("STW_ARENA_STRUCT_01", TILE_STRUCT)
    kit.begin_group("support_pylons")
    for y in (-6.0, 0.0, 6.0):
        _wall_pylon(kit, 0, -1.0, y)
        _wall_pylon(kit, 0, 1.0, y)
    for x in (-6.0, 6.0):
        _wall_pylon(kit, 1, 1.0, x)

    kit.begin_group("roof_trusses")
    for y in TRUSS_Y:
        _truss(kit, y)

    kit.begin_group("tie_beams")
    for x in (-8.0, 8.0):
        add_box(kit, (x, 2.0, TRUSS_BOTTOM + 0.34), (0.24, WALL_INNER * 2.0 - 4.0, 0.24))
    add_box(kit, (0.0, 2.0, TRUSS_TOP - 0.06), (0.30, WALL_INNER * 2.0 - 4.0, 0.16))
    return kit


# --------------------------------------------------------------------------- #
# COVER - manufactured modules over the authoritative collision footprint
# --------------------------------------------------------------------------- #
def build_cover():
    kit = ObjKit("STW_ARENA_COVERMOD_01", TILE_COVER)
    sx, sy, sz = COVER_SIZE
    for index, cx in enumerate(COVER_CENTRES):
        kit.begin_group("cover_module_{0}".format(index))
        # Base plinth, slightly wider than the body.
        add_chamfered_box(kit, (cx, 0.0, 0.11), (sx + 0.14, sy + 0.14, 0.22), 0.07)
        # Main body: chamfered so the silhouette is not a cube.
        add_chamfered_box(kit, (cx, 0.0, 0.22 + (sz - 0.44) * 0.5), (sx, sy, sz - 0.44), 0.16)
        # Layered armour on the arena-facing side, three depths.
        for i, (inset, depth) in enumerate(((0.10, 0.06), (0.30, 0.10), (0.52, 0.14))):
            add_box(kit, (cx, -sy * 0.5 - depth * 0.5 + 0.02, 0.55 + i * 0.62),
                    (sx - inset, depth, 0.52))
        # Inset side panels.
        for side in (-1.0, 1.0):
            add_box(kit, (cx + side * (sx * 0.5 - 0.05), 0.0, 1.30), (0.10, sy - 0.42, 1.10))
        # Rear service face: recessed bay, hardware, conduit drop.
        add_box(kit, (cx, sy * 0.5 - 0.09, 1.35), (sx - 0.36, 0.18, 1.30))
        add_box(kit, (cx - 0.32, sy * 0.5 + 0.02, 1.62), (0.36, 0.16, 0.44))
        add_box(kit, (cx + 0.30, sy * 0.5 + 0.02, 1.20), (0.26, 0.14, 0.30))
        add_conduit(kit, (cx + 0.46, sy * 0.5 + 0.06, 0.30),
                    (cx + 0.46, sy * 0.5 + 0.06, 1.90), 0.045)
        # Top cap with a small overhang.
        add_chamfered_box(kit, (cx, 0.0, sz - 0.08), (sx + 0.10, sy + 0.10, 0.16), 0.08)
        for bx in (-1, 1):
            add_bolt(kit, (cx + bx * (sx * 0.5 - 0.16), -sy * 0.5 + 0.14, sz), 0.05, 0.026)

    # The "STW Step" collision box has never had a visual. Give it one so the
    # traversal affordance is honest.
    kit.begin_group("step_platform")
    add_chamfered_box(kit, (STEP_CENTRE[0], STEP_CENTRE[1], STEP_SIZE[2] * 0.5),
                      (STEP_SIZE[0], STEP_SIZE[1], STEP_SIZE[2]), 0.10)
    add_box(kit, (STEP_CENTRE[0], STEP_CENTRE[1] - STEP_SIZE[1] * 0.5 + 0.04,
                  STEP_SIZE[2] * 0.5), (STEP_SIZE[0] - 0.20, 0.08, STEP_SIZE[2] * 0.8))
    return kit


# --------------------------------------------------------------------------- #
# HERO ARCH - the strongest prototype cue in the R1 frame, fully re-authored
# --------------------------------------------------------------------------- #
ARCH_PORTAL_HEIGHT = 2.10   # header mass sits this far above ARCH_CLEAR_Z
ARCH_HANGER_TOP = TRUSS_TOP + 0.50


def build_arch():
    """Suspended hero portal.

    Block 26E stood this gate on two 1.4 x 1.24 m legs that ran to the deck at
    (+-5.0, 8.5) - open floor with no collider under it, so the player walked
    through the most prominent object in the frame. R1 lifted the feet to a
    1.95 m clearance, which the R4 audit showed is still inside real player reach:
    a jump from the deck alone puts the standing capsule near 3.5 m. Gameplay owns
    collision and this kit may not add a box, so the portal is carried from the
    roof structure and every solid part of it now begins at ARCH_CLEAR_Z, above
    the conservative mantle + jump + capsule envelope.

    Every Z here is expressed relative to `base`, so the portal moves as one piece
    if the envelope moves again. The composition is unchanged - terminated feet,
    tapered legs, layered shell over a recessed core, three-part header, recessed
    portal frame, cross-members, hangers, service routing, central housing - only
    the band it occupies is compressed into the headroom between the envelope and
    the roof.
    """
    kit = ObjKit("STW_ARENA_ARCH_01", TILE_STRUCT)
    base = ARCH_CLEAR_Z                     # lowest solid point of the portal
    top = base + ARCH_PORTAL_HEIGHT         # centre of the header mass
    for side in (-1.0, 1.0):
        x = side * ARCH_LEG_X
        kit.begin_group("arch_support_{0}".format("east" if side > 0 else "west"))
        # Terminated foot: the leg reads as deliberately suspended, not clipped.
        add_chamfered_box(kit, (x, ARCH_Y, base + 0.09), (1.30, 1.16, 0.18), 0.16)
        add_tapered_box(kit, (x, ARCH_Y, base + 0.44), (1.16, 1.04), (1.00, 0.94), 0.52)
        # Mid shaft: outer shell plus a recessed inner core.
        add_chamfered_box(kit, (x, ARCH_Y, base + 1.05), (0.98, 0.92, 0.92), 0.20)
        add_chamfered_box(kit, (x, ARCH_Y, base + 1.05), (0.70, 1.06, 0.80), 0.10)
        # Upper taper into the header.
        add_tapered_box(kit, (x, ARCH_Y, base + 1.68), (0.94, 0.88), (1.16, 1.00), 0.56)
        # Layered armour plates, stepped so the silhouette breaks up.
        for i, offset in enumerate((0.42, 0.86, 1.30, 1.72)):
            add_box(kit, (x - side * 0.52, ARCH_Y, base + offset),
                    (0.12, 1.00 - 0.06 * i, 0.34))
        # Joint brackets.
        for sy in (-1.0, 1.0):
            add_box(kit, (x, ARCH_Y + sy * 0.58, base + 1.62), (0.72, 0.20, 0.28))

    kit.begin_group("arch_header")
    # Layered header: main beam, lower fascia, upper cap.
    add_box(kit, (0.0, ARCH_Y, top - 0.28), (ARCH_LEG_X * 2.0 + 1.30, 0.94, 0.62))
    add_box(kit, (0.0, ARCH_Y - 0.06, top - 0.64), (ARCH_LEG_X * 2.0 + 0.60, 0.72, 0.20))
    add_chamfered_box(kit, (0.0, ARCH_Y, top + 0.12), (ARCH_LEG_X * 2.0 + 1.60, 1.06, 0.24), 0.13)
    # Inner recessed frame around the portal opening.
    add_box(kit, (0.0, ARCH_Y + 0.30, top - 0.76), (ARCH_LEG_X * 2.0 - 0.40, 0.24, 0.24))
    for side in (-1.0, 1.0):
        add_box(kit, (side * (ARCH_LEG_X - 0.62), ARCH_Y + 0.30, base + 1.15),
                (0.24, 0.24, 2.06))

    kit.begin_group("arch_cross_members")
    for offset in (0.62, 1.24):
        add_box(kit, (0.0, ARCH_Y - 0.34, base + offset),
                (ARCH_LEG_X * 2.0 - 1.10, 0.22, 0.18))
    for i in range(6):
        cx = -ARCH_LEG_X + 1.2 + i * 1.52
        add_box(kit, (cx, ARCH_Y, top - 0.70), (0.16, 0.60, 0.30))

    kit.begin_group("arch_hangers")
    # What holds the portal up: rods from the header cap into the roof structure,
    # past the truss top chord. Entirely above anything the player can reach.
    for i in range(4):
        cx = -ARCH_LEG_X + 1.35 + i * (ARCH_LEG_X * 2.0 - 2.70) / 3.0
        add_conduit(kit, (cx, ARCH_Y, top + 0.22), (cx, ARCH_Y, ARCH_HANGER_TOP), 0.065)
        add_box(kit, (cx, ARCH_Y, ARCH_HANGER_TOP + 0.06), (0.30, 0.30, 0.14))
    for side in (-1.0, 1.0):
        add_box(kit, (side * (ARCH_LEG_X + 0.55), ARCH_Y, top + 0.42), (0.34, 0.62, 0.36))

    kit.begin_group("arch_service_routing")
    # Cable / conduit runs along the header, with an asymmetric drop on one leg.
    for offset, radius in ((-0.30, 0.075), (-0.14, 0.055)):
        add_conduit(kit, (-ARCH_LEG_X - 0.6, ARCH_Y - 0.52, top - 0.16 + offset * 0.2),
                    (ARCH_LEG_X + 0.6, ARCH_Y - 0.52, top - 0.16 + offset * 0.2), radius)
    add_conduit(kit, (-ARCH_LEG_X + 0.55, ARCH_Y - 0.52, top - 0.34),
                (-ARCH_LEG_X + 0.55, ARCH_Y - 0.52, base + 0.24), 0.07)
    for offset in (0.34, 0.92, 1.50):
        add_box(kit, (-ARCH_LEG_X + 0.55, ARCH_Y - 0.52, base + offset), (0.20, 0.18, 0.12))

    kit.begin_group("arch_central_element")
    # Central technical housing - the focal detail on the sightline.
    add_chamfered_box(kit, (0.0, ARCH_Y - 0.20, top - 0.34), (1.50, 0.86, 0.78), 0.20)
    add_box(kit, (0.0, ARCH_Y - 0.66, top - 0.34), (0.96, 0.14, 0.46))
    for sx in (-1.0, 1.0):
        add_box(kit, (sx * 0.92, ARCH_Y - 0.30, top - 0.34), (0.26, 0.44, 0.54))
    return kit


# --------------------------------------------------------------------------- #
# LANDMARK - vertical silhouette plus the arena's emissive channels
# --------------------------------------------------------------------------- #
def build_landmark():
    """North-wall beacon: vertical silhouette plus the arena's emissive channels.

    The tower body cantilevers ~1.25 m off the wall over the deck. Block 26E-R1
    started it at 3.0 m, which cleared the standing capsule but sat squarely
    inside the R4 jump envelope - a player hugging the north wall would put their
    head through the beacon's underside with no collider to stop them. Gameplay
    owns collision and this kit may not add a box, so the whole suspended body is
    anchored at ARCH_CLEAR_Z instead and the stack is compressed to keep the
    sensor mast inside the arena visual envelope. The silhouette, layering, fins,
    service housings, maintenance ladder and emissive channels are unchanged.
    """
    kit = ObjKit("STW_ARENA_BEACON_01", TILE_LANDMARK)
    base_y = WALL_INNER - 0.55
    lb = ARCH_CLEAR_Z                       # lowest solid point of the beacon body

    kit.begin_group("landmark_tower")
    # Layered body, each section narrower than the one below it.
    add_chamfered_box(kit, (0.0, base_y, lb + 0.30), (5.60, 1.40, 0.60), 0.30)
    add_chamfered_box(kit, (0.0, base_y, lb + 1.55), (4.40, 1.20, 1.90), 0.34)
    add_chamfered_box(kit, (0.0, base_y, lb + 3.30), (3.10, 1.00, 1.70), 0.28)
    add_chamfered_box(kit, (0.0, base_y, lb + 4.70), (1.90, 0.86, 1.20), 0.22)
    add_tapered_box(kit, (0.0, base_y, lb + 5.70), (1.60, 0.80), (0.80, 0.50), 0.80)

    kit.begin_group("landmark_fins")
    for i in range(7):
        fx = -2.70 + i * 0.90
        add_box(kit, (fx, base_y - 0.62, lb + 1.90), (0.16, 0.42, 2.20))
    for sx in (-1.0, 1.0):
        add_box(kit, (sx * 2.10, base_y - 0.40, lb + 3.70), (0.20, 0.34, 1.50))

    kit.begin_group("landmark_service_housings")
    add_chamfered_box(kit, (-1.55, base_y - 0.72, lb + 0.92), (0.90, 0.60, 0.80), 0.12)
    add_chamfered_box(kit, (1.62, base_y - 0.68, lb + 0.78), (0.70, 0.54, 0.62), 0.10)
    add_box(kit, (0.0, base_y - 0.74, lb + 0.38), (1.30, 0.44, 0.44))

    kit.begin_group("landmark_sensors")
    for sx, height in ((-0.55, 1.15), (0.62, 0.85)):
        add_conduit(kit, (sx, base_y, lb + 5.90), (sx, base_y, lb + 5.90 + height),
                    0.055, sides=6)
        add_box(kit, (sx, base_y, lb + 5.90 + height), (0.24, 0.24, 0.14))
    add_chamfered_box(kit, (1.35, base_y - 0.30, lb + 5.05), (0.86, 0.30, 0.86), 0.30)

    kit.begin_group("landmark_maintenance_access")
    for i in range(9):
        add_box(kit, (2.35, base_y - 0.58, lb + 0.44 + i * 0.42), (0.42, 0.08, 0.06))
    add_box(kit, (2.35, base_y - 0.52, lb + 2.80), (0.62, 0.10, 0.70))

    # Controlled emissive channels. These share the landmark material, which is
    # the only family carrying an emissive map, so every glowing element in the
    # arena lives in this mesh.
    kit.begin_group("emissive_landmark_strips")
    for offset in (0.78, 2.42, 4.02):
        add_box(kit, (0.0, base_y - 0.66, lb + offset), (3.60, 0.10, 0.13))
    add_box(kit, (0.0, base_y - 0.66, lb + 5.30), (1.20, 0.10, 0.11))

    kit.begin_group("emissive_arch_channels")
    for side in (-1.0, 1.0):
        # Tracks the suspended portal's leg, not knee height.
        add_box(kit, (side * (ARCH_LEG_X - 0.62), ARCH_Y - 0.46, ARCH_CLEAR_Z + 1.15),
                (0.11, 0.09, 2.06))
    add_box(kit, (0.0, ARCH_Y - 0.60, ARCH_CLEAR_Z + 1.56),
            (ARCH_LEG_X * 2.0 - 1.40, 0.09, 0.11))
    add_box(kit, (0.0, ARCH_Y - 0.78, ARCH_CLEAR_Z + 2.16), (0.80, 0.09, 0.14))

    kit.begin_group("emissive_wall_datum")
    # A continuous low datum line reads the room's scale from the spawn.
    for sign in (-1.0, 1.0):
        add_box(kit, (0.0, sign * (WALL_INNER + 0.06), 1.22), (WALL_INNER * 2.0 - 2.2, 0.09, 0.10))
        add_box(kit, (sign * (WALL_INNER + 0.06), 0.0, 1.22), (0.09, WALL_INNER * 2.0 - 2.2, 0.10))

    kit.begin_group("emissive_truss_underside")
    for y in TRUSS_Y:
        add_box(kit, (0.0, y, TRUSS_BOTTOM + 0.02), (WALL_INNER * 1.7, 0.13, 0.08))
    return kit


# --------------------------------------------------------------------------- #
# TRIM - hazard bands, kick plates, edge rails
# --------------------------------------------------------------------------- #
def build_trim():
    kit = ObjKit("STW_ARENA_TRIMKIT_01", TILE_TRIM)
    # Wall-hugging trim sits in the relief zone behind the collision plane; the
    # X runs stop short so the four runs butt rather than overlap at the corners.
    kit.begin_group("wall_kick_plates")
    for sign in (-1.0, 1.0):
        add_box(kit, (0.0, sign * (WALL_INNER + 0.07), 0.16), (WALL_INNER * 2.0, 0.14, 0.32))
        add_box(kit, (sign * (WALL_INNER + 0.07), 0.0, 0.16), (0.14, WALL_INNER * 2.0 - 0.6, 0.32))

    kit.begin_group("wall_cap_band")
    for sign in (-1.0, 1.0):
        add_box(kit, (0.0, sign * (WALL_INNER + 0.06), WALL_HEIGHT - 0.42),
                (WALL_INNER * 2.0, 0.12, 0.16))
        add_box(kit, (sign * (WALL_INNER + 0.06), 0.0, WALL_HEIGHT - 0.42),
                (0.12, WALL_INNER * 2.0 - 0.6, 0.16))

    kit.begin_group("cover_hazard_caps")
    for cx in COVER_CENTRES:
        add_box(kit, (cx, 0.0, COVER_SIZE[2] + 0.03),
                (COVER_SIZE[0] + 0.16, COVER_SIZE[1] + 0.16, 0.06))
        add_box(kit, (cx, -COVER_SIZE[1] * 0.5 - 0.16, 0.30), (COVER_SIZE[0] - 0.20, 0.08, 0.12))

    kit.begin_group("step_edge_rails")
    add_box(kit, (STEP_CENTRE[0], STEP_CENTRE[1], STEP_SIZE[2] + 0.02),
            (STEP_SIZE[0] + 0.10, STEP_SIZE[1] + 0.10, 0.04))

    kit.begin_group("arch_threshold_trim")
    # Flush landing pads under the suspended portal legs. Block 26E raised these
    # to 0.30 m, which put a 1.86 x 1.66 m kerb in open floor with no collider.
    for side in (-1.0, 1.0):
        add_box(kit, (side * ARCH_LEG_X, ARCH_Y, 0.04), (1.86, 1.66, 0.08))
        add_box(kit, (side * ARCH_LEG_X, ARCH_Y, 0.085), (1.42, 1.26, 0.03))
    add_box(kit, (0.0, ARCH_Y - 0.66, 0.03), (10.4, 0.24, 0.06))

    kit.begin_group("channel_edge_trim")
    for cy in (-7.0, 7.0):
        for sy in (-1.0, 1.0):
            add_box(kit, (0.0, cy + sy * 0.34, DECK_TOP - 0.02), (PLATE_HALF * 2.0, 0.08, 0.05))
    for cx in (-9.0, 9.0):
        for sx in (-1.0, 1.0):
            add_box(kit, (cx + sx * 0.34, 0.0, DECK_TOP - 0.02), (0.08, PLATE_HALF * 2.0, 0.05))
    return kit


# --------------------------------------------------------------------------- #
# PROPS - composed set dressing, not uniform spam
# --------------------------------------------------------------------------- #
def _crate(kit, centre, size):
    add_chamfered_box(kit, centre, size, min(size[0], size[1]) * 0.10)
    add_box(kit, (centre[0], centre[1], centre[2] + size[2] * 0.5 - 0.03),
            (size[0] * 0.86, size[1] * 0.86, 0.06))
    for sx in (-1, 1):
        add_box(kit, (centre[0] + sx * size[0] * 0.38, centre[1] - size[1] * 0.5 - 0.02,
                      centre[2]), (0.10, 0.06, size[2] * 0.7))


def _barricade(kit, centre, length, facing):
    width, height = 0.16, 1.05
    size = (length, width, 0.14) if facing == 0 else (width, length, 0.14)
    add_box(kit, (centre[0], centre[1], centre[2] + height), size)
    add_box(kit, (centre[0], centre[1], centre[2] + height - 0.34),
            (length * 0.94, width * 0.7, 0.10) if facing == 0
            else (width * 0.7, length * 0.94, 0.10))
    for offset in (-length * 0.4, length * 0.4):
        lx = centre[0] + (offset if facing == 0 else 0.0)
        ly = centre[1] + (0.0 if facing == 0 else offset)
        add_box(kit, (lx, ly, centre[2] + height * 0.5), (0.10, 0.10, height))
        add_box(kit, (lx, ly, centre[2] + 0.04), (0.40, 0.40, 0.08))


def _wall_crate(kit, axis, sign, along, along_size, height, stack=0.0):
    """Crate / case stood against a perimeter wall, inside the relief zone.

    Block 26E scattered these as free-standing crates across the open deck, where
    the collision world has nothing but floor: they were walk-through. Lining the
    perimeter keeps the same silhouette vocabulary - chamfered crate, lid plate,
    feet - while every face stays at or behind the collision plane.
    """
    front, back = 0.01, 0.01 + PROP_DEPTH
    base = stack
    _wall_box(kit, axis, sign, along, along_size, front, back,
              base + height * 0.5, height, min(along_size, PROP_DEPTH) * 0.10)
    _wall_box(kit, axis, sign, along, along_size * 0.86, front + 0.04, back - 0.04,
              base + height - 0.03, 0.06)
    for offset in (-along_size * 0.38, along_size * 0.38):
        _wall_box(kit, axis, sign, along + offset, 0.10, front, front + 0.06,
                  base + height * 0.5, height * 0.7)


def _wall_case(kit, axis, sign, along, along_size, height, z):
    """Flatter equipment case, mounted clear of the floor on a short bracket."""
    front, back = 0.02, 0.02 + PROP_DEPTH * 0.82
    _wall_box(kit, axis, sign, along, along_size, front, back, z, height, 0.07)
    _wall_box(kit, axis, sign, along, along_size * 0.88, front + 0.03, back - 0.05,
              z + height * 0.5 + 0.03, 0.05)


def build_props():
    kit = ObjKit("STW_ARENA_PROP_01", TILE_PROP)

    # Perimeter equipment lines. The deck itself carries no free-standing props:
    # every collider in the arena is accounted for in PhysXPlayerRuntime, and
    # none of them is a crate, so a crate on the open deck can only be walked
    # through. The dressing reads from the same sightlines, stacked along the
    # walls where the collision plane already stops the player.
    kit.begin_group("props_foreground")
    for along, size, height in ((-7.40, 1.00, 0.84), (-4.20, 0.74, 0.62),
                                (3.60, 0.92, 0.78), (7.90, 0.62, 0.54)):
        _wall_crate(kit, 1, -1.0, along, size, height)
    _wall_crate(kit, 1, -1.0, -7.40, 0.62, 0.46, stack=0.84)
    _wall_case(kit, 1, -1.0, 0.90, 1.15, 0.44, 1.30)

    kit.begin_group("props_midfield")
    for along, size, height in ((-8.60, 1.05, 0.88), (-3.20, 0.70, 0.60),
                                (3.40, 0.96, 0.82), (9.00, 0.66, 0.56)):
        _wall_crate(kit, 0, -1.0, along, size, height)
    for along, size, height in ((-8.90, 0.98, 0.80), (-3.00, 0.72, 0.64),
                                (3.10, 1.02, 0.86), (8.80, 0.60, 0.52)):
        _wall_crate(kit, 0, 1.0, along, size, height)
    _wall_crate(kit, 0, 1.0, -8.90, 0.66, 0.44, stack=0.80)
    for sign, along in ((-1.0, 0.60), (1.0, -0.40)):
        _wall_case(kit, 0, sign, along, 1.15, 0.44, 1.24)

    # Focal area: dressing that frames the hero arch without standing under it.
    kit.begin_group("props_arch_focus")
    for along, size, height in ((-9.20, 0.86, 0.72), (9.00, 0.78, 0.66)):
        _wall_crate(kit, 1, 1.0, along, size, height)
    _wall_case(kit, 1, 1.0, -9.20, 0.90, 0.40, 1.22)
    # Two service columns flanking the portal sightline, carried on the cover
    # modules - the only mid-arena colliders that exist - so they sit above the
    # standing player rather than in front of them.
    for cx in COVER_CENTRES:
        add_chamfered_box(kit, (cx, 0.0, COVER_SIZE[2] + 0.36), (0.66, 0.58, 0.52), 0.10)
        add_box(kit, (cx, -0.30, COVER_SIZE[2] + 0.66), (0.44, 0.10, 0.24))
    # Flush service pads where the crates used to stand, so the deck keeps its
    # composition anchors without anything to walk through.
    for cx, cy in ((-6.30, -7.40), (7.10, -8.05), (-8.40, 1.20),
                   (8.55, 0.40), (-3.90, 4.40), (4.30, 5.10)):
        add_box(kit, (cx, cy, DECK_TOP - 0.020), (1.10, 0.94, 0.040))
        add_box(kit, (cx, cy, DECK_TOP - 0.004), (0.86, 0.70, 0.020))

    kit.begin_group("props_conduit_runs")
    for sign in (-1.0, 1.0):
        y = sign * WALL_MOUNT
        add_conduit(kit, (-WALL_INNER + 1.0, y, 2.62), (WALL_INNER - 1.0, y, 2.62), 0.075)
        add_conduit(kit, (-WALL_INNER + 1.0, y, 2.42), (WALL_INNER - 1.0, y, 2.42), 0.055)
        for i in range(7):
            cx = -WALL_INNER + 1.8 + i * 3.0
            add_box(kit, (cx, sign * (WALL_INNER + 0.13), 2.52), (0.16, 0.22, 0.42))
        x = sign * WALL_MOUNT
        add_conduit(kit, (x, -WALL_INNER + 1.0, 2.62), (x, WALL_INNER - 1.0, 2.62), 0.075)
        for i in range(7):
            cy = -WALL_INNER + 1.8 + i * 3.0
            add_box(kit, (sign * (WALL_INNER + 0.13), cy, 2.52), (0.22, 0.16, 0.42))

    kit.begin_group("props_wall_boxes")
    # (axis, sign, along, z, size); axis 1 mounts on a +-Y wall, 0 on a +-X wall.
    # The mount is derived from each box's own depth so its inner face lands just
    # outside WALL_INNER rather than hanging into playable space.
    wall_boxes = (
        (1, 1.0, -8.0, 1.95, (0.62, 0.34, 0.78)),
        (1, 1.0, -2.6, 2.20, (0.48, 0.30, 0.56)),
        (1, 1.0, 3.8, 1.85, (0.70, 0.36, 0.64)),
        (1, 1.0, 9.1, 2.05, (0.44, 0.28, 0.52)),
        (0, -1.0, -3.4, 2.00, (0.34, 0.66, 0.72)),
        (0, -1.0, 5.2, 1.90, (0.30, 0.50, 0.58)),
        (0, 1.0, -1.8, 2.10, (0.34, 0.60, 0.68)),
        (0, 1.0, 6.6, 1.88, (0.30, 0.46, 0.54)),
    )
    for axis, sign, along, cz, size in wall_boxes:
        depth = size[1] if axis == 1 else size[0]
        mount = sign * (WALL_INNER + depth * 0.5 + 0.02)
        centre = (along, mount, cz) if axis == 1 else (mount, along, cz)
        add_chamfered_box(kit, centre, size, 0.06)
        add_box(kit, (centre[0], centre[1], cz - size[2] * 0.5 - 0.05),
                (size[0] * 0.5, size[1] * 0.6, 0.10))

    kit.begin_group("props_antennae")
    for cx, cy in ((-10.2, WALL_MOUNT), (10.4, WALL_MOUNT)):
        add_box(kit, (cx, cy, 6.25), (0.30, 0.30, 0.36))
        add_conduit(kit, (cx, cy, 6.40), (cx, cy, 7.85), 0.05, sides=6)
        for z in (7.05, 7.45):
            add_box(kit, (cx, cy, z), (0.52, 0.06, 0.05))

    kit.begin_group("props_floor_service_panels")
    for cx, cy in ((-3.0, -3.0), (4.0, 1.0), (-9.0, -5.0), (8.0, 6.0)):
        add_box(kit, (cx, cy, DECK_TOP - 0.012), (0.86, 0.86, 0.024))
        add_box(kit, (cx, cy, DECK_TOP - 0.004), (0.62, 0.62, 0.012))

    kit.begin_group("props_light_housings")
    # Seated so the housing body and its lens both stop at the collision plane.
    # WALL_MOUNT alone put a 0.34 m deep body 0.07 m proud of WALL_INNER, which
    # is inside the R4 reach envelope with only the top of the 4 m wall collider
    # under it - the wall stops the player at the plane, not 0.07 m past it.
    housing_mount = WALL_INNER + 0.19
    for cx in (-7.5, 0.0, 7.5):
        add_chamfered_box(kit, (cx, housing_mount, 4.42), (0.88, 0.34, 0.26), 0.08)
        add_box(kit, (cx, housing_mount - 0.14, 4.30), (0.70, 0.10, 0.10))
    for y in (-4.0, 4.0):
        for sx in (-1.0, 1.0):
            add_chamfered_box(kit, (sx * 9.4, y, TRUSS_BOTTOM - 0.22), (0.62, 0.62, 0.30), 0.10)
            add_box(kit, (sx * 9.4, y, TRUSS_BOTTOM - 0.42), (0.44, 0.44, 0.12))
    return kit


# --------------------------------------------------------------------------- #
# MARKINGS - thin planes offset off their host surface
# --------------------------------------------------------------------------- #
def build_marks():
    kit = ObjKit("STW_ARENA_MARK_01", TILE_MARK)
    z = DECK_TOP + MARK_OFFSET

    kit.begin_group("deck_hazard_striping")
    # Threshold striping under the arch and in front of each cover module.
    for i in range(9):
        add_quad_z(kit, (-4.8 + i * 1.2, ARCH_Y - 1.15, 0.0), (0.72, 0.62), z)
    for cx in COVER_CENTRES:
        for i in range(3):
            add_quad_z(kit, (cx - 0.5 + i * 0.5, -COVER_SIZE[1] * 0.5 - 0.55, 0.0),
                       (0.34, 0.72), z)
    for i in range(4):
        add_quad_z(kit, (STEP_CENTRE[0] - 0.75 + i * 0.5, STEP_CENTRE[1] - 1.32, 0.0),
                   (0.32, 0.44), z)

    kit.begin_group("deck_directional_chevrons")
    for i in range(6):
        add_quad_z(kit, (0.0, -8.2 + i * 2.6, 0.0), (1.35, 0.60), z)
    for sx in (-1.0, 1.0):
        for i in range(3):
            add_quad_z(kit, (sx * 8.6, -4.0 + i * 3.4, 0.0), (0.90, 0.48), z)

    kit.begin_group("deck_service_numbering")
    for cx, cy in ((-5.0, -5.0), (7.0, 3.0), (-7.0, 7.0), (3.0, -9.0)):
        add_quad_z(kit, (cx, cy - 0.72, 0.0), (0.90, 0.30), z + 0.04)
    for cx, cy in ((-3.0, -3.0), (4.0, 1.0), (-9.0, -5.0), (8.0, 6.0)):
        add_quad_z(kit, (cx, cy - 0.62, 0.0), (0.60, 0.22), z)

    kit.begin_group("wall_signage")
    # Signage sits on the recessed panel field (the slab face), not floating in
    # front of the frame members that stand proud of it.
    wall_y = WALL_FACE - MARK_OFFSET
    for cx, cz, size in ((-9.4, 2.65, (1.30, 0.62)), (-4.2, 2.90, (1.05, 0.50)),
                         (1.9, 2.60, (1.45, 0.68)), (6.9, 2.85, (1.10, 0.52)),
                         (10.3, 2.55, (0.85, 0.42))):
        add_quad_wall(kit, (cx, wall_y, cz), size, 1, -1)
    wall_x = WALL_FACE - MARK_OFFSET
    for cy, cz, size in ((-6.2, 2.70, (1.20, 0.58)), (0.9, 2.95, (1.35, 0.62)),
                         (7.4, 2.60, (0.95, 0.46))):
        add_quad_wall(kit, (wall_x, cy, cz), size, 0, 1)
        add_quad_wall(kit, (-wall_x, cy, cz), size, 0, -1)

    kit.begin_group("wall_caution_bands")
    for cx in (-11.0, -6.6, -2.2, 2.2, 6.6, 11.0):
        add_quad_wall(kit, (cx, wall_y, 0.62), (1.60, 0.26), 1, -1)

    kit.begin_group("cover_industrial_symbols")
    for cx in COVER_CENTRES:
        add_quad_wall(kit, (cx, -COVER_SIZE[1] * 0.5 - 0.14 - MARK_OFFSET, 1.62),
                      (0.70, 0.70), 1, -1)
        add_quad_wall(kit, (cx, -COVER_SIZE[1] * 0.5 - 0.14 - MARK_OFFSET, 0.86),
                      (0.52, 0.26), 1, -1)

    kit.begin_group("arch_maintenance_numbering")
    # On the mid-shaft face of the suspended portal legs, so the numbering rides
    # with the leg instead of floating at chest height where the leg used to be.
    for side in (-1.0, 1.0):
        add_quad_wall(kit, (side * ARCH_LEG_X, ARCH_Y - 0.46 - MARK_OFFSET,
                            ARCH_CLEAR_Z + 1.05), (0.62, 0.44), 1, -1)
    return kit


# --------------------------------------------------------------------------- #
# M1 regression: conduit winding is geometrically outward on every axis
# --------------------------------------------------------------------------- #
def _conduit_faces(start, end, radius=0.1, sides=6):
    """Emit one conduit through add_conduit and return (points, stored_vn) per face."""
    kit = ObjKit("conduit_probe", 1.0)
    kit.begin_group("probe")
    add_conduit(kit, start, end, radius, sides=sides)
    out = []
    for _, faces in kit.groups:
        for corner in faces:
            points = [kit.positions[pi - 1] for pi, _, _ in corner]
            stored = kit.normals[corner[0][2] - 1]
            out.append((points, stored))
    return out


def _closest_point_on_segment(point, start, end):
    axis_vec = tuple(end[i] - start[i] for i in range(3))
    length_sq = sum(c * c for c in axis_vec)
    if length_sq < 1e-12:
        return start
    t = sum((point[i] - start[i]) * axis_vec[i] for i in range(3)) / length_sq
    return tuple(start[i] + t * axis_vec[i] for i in range(3))


def conduit_winding_selftest():
    """Derive each conduit side-face normal from VERTEX ORDER ONLY and prove it
    points radially outward, for a run along +/-X, +/-Y and +/-Z. Also prove the
    stored vn agrees with that vertex-order normal. Independent of the generator's
    own normal bookkeeping - _face_normal is re-run here on the emitted positions.
    """
    directions = {
        "X": ((-1.0, 0.3, 0.2), (1.0, 0.3, 0.2)),
        "Y": ((0.3, -1.0, 0.2), (0.3, 1.0, 0.2)),
        "Z": ((0.3, 0.2, -1.0), (0.3, 0.2, 1.0)),
        "NEG_X": ((1.0, 0.3, 0.2), (-1.0, 0.3, 0.2)),
        "NEG_Y": ((0.3, 1.0, 0.2), (0.3, -1.0, 0.2)),
        "NEG_Z": ((0.3, 0.2, 1.0), (0.3, 0.2, -1.0)),
    }
    results = {}
    for label, (start, end) in directions.items():
        outward_ok = True
        agree_ok = True
        for points, stored in _conduit_faces(start, end):
            geometric = _face_normal(points)
            if geometric is None:
                outward_ok = False
                continue
            centroid = tuple(sum(p[i] for p in points) / len(points) for i in range(3))
            centre = _closest_point_on_segment(centroid, start, end)
            radial = tuple(centroid[i] - centre[i] for i in range(3))
            if sum(geometric[i] * radial[i] for i in range(3)) <= 1e-9:
                outward_ok = False
            if sum(geometric[i] * stored[i] for i in range(3)) <= 0.0:
                agree_ok = False
        results[label] = outward_ok and agree_ok
    return results


# --------------------------------------------------------------------------- #
# entry point
# --------------------------------------------------------------------------- #
BUILDERS = (
    ("STW_ARENA_DECK_01", build_deck),
    ("STW_ARENA_WALL_01", build_walls),
    ("STW_ARENA_STRUCT_01", build_struct),
    ("STW_ARENA_COVERMOD_01", build_cover),
    ("STW_ARENA_ARCH_01", build_arch),
    ("STW_ARENA_BEACON_01", build_landmark),
    ("STW_ARENA_TRIMKIT_01", build_trim),
    ("STW_ARENA_PROP_01", build_props),
    ("STW_ARENA_MARK_01", build_marks),
)


def _print_conduit_regressions():
    results = conduit_winding_selftest()
    order = ("X", "Y", "Z", "NEG_X", "NEG_Y", "NEG_Z")
    for label in order:
        print("{0}_CONDUIT_OUTWARD={1}".format(label, "PASS" if results[label] else "FAIL"))
    return all(results[label] for label in order)


def main():
    parser = argparse.ArgumentParser(description="Generate the STW modular arena kit.")
    parser.add_argument("--check", action="store_true",
                        help="build and report without writing any file")
    parser.add_argument("--selftest", action="store_true",
                        help="run only the conduit winding regression and exit")
    args = parser.parse_args()

    if args.selftest:
        return 0 if _print_conduit_regressions() else 1

    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    total_vertices = total_faces = total_triangles = total_bytes = 0
    for name, builder in BUILDERS:
        kit = builder()
        payload = kit.serialise()
        vertices, faces, triangles, groups = kit.counts()
        (lo, hi) = kit.bounds()
        path = ASSET_DIR / "{0}.obj".format(name)
        if not args.check:
            path.write_text(payload, encoding="utf-8")
        size = len(payload.encode("utf-8"))
        total_vertices += vertices
        total_faces += faces
        total_triangles += triangles
        total_bytes += size
        print("mesh={0:24s} v={1:6d} f={2:6d} tris={3:6d} groups={4:3d} bytes={5:8d} "
              "bounds=({6:.2f},{7:.2f},{8:.2f})..({9:.2f},{10:.2f},{11:.2f})".format(
                  name, vertices, faces, triangles, groups, size,
                  lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]))
    print("STW_KIT_MESHES={0}".format(len(BUILDERS)))
    print("STW_KIT_VERTICES={0}".format(total_vertices))
    print("STW_KIT_FACES={0}".format(total_faces))
    print("STW_KIT_TRIANGLES={0}".format(total_triangles))
    print("STW_KIT_BYTES={0}".format(total_bytes))
    print("STW_KIT_WRITTEN={0}".format("NO" if args.check else "YES"))
    print("DIR={0}".format(ASSET_DIR))
    _print_conduit_regressions()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
