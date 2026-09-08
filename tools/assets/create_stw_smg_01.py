#!/usr/bin/env python3
"""Deterministic generator for the original STW first-person weapon asset STW_SMG_01.

STW_SMG_01 is an ORIGINAL, FICTIONAL sci-fi game prop authored entirely by this script.
It is a first-person presentation silhouette only: it replicates no real firearm, exposes
no real-world construction, mechanism, or functional dimensions, and contains no moving or
functional parts. Nothing is downloaded and no third-party asset is embedded.

Geometry is built from convex blocks (optionally tapered along the barrel axis) that are
composed into named OBJ groups. Output is byte-for-byte reproducible: no randomness, no
timestamps, no locale-dependent formatting.

Engine-local axis convention, matching the runtime viewmodel basis built from
(right, aim, up) as the columns of the mesh transform:

    +X = right      +Y = aim / forward (muzzle)      +Z = up

The asset is authored directly at first-person presentation proportions (engine-local
visual proportions), so the runtime applies a scale of 1.0 on every axis.

Usage:
    python3 tools/assets/create_stw_smg_01.py [--output <path/to/STW_SMG_01.obj>] [--check]
"""

import argparse
import os
import sys

ASSET_NAME = "STW_SMG_01"

# Quantisation for emitted coordinates. Values are rounded to this many decimals so the
# file is identical on every machine regardless of floating point printing differences.
DECIMALS = 6

# Minimum quality floor this generator refuses to fall below. The asset must never
# degenerate into a single box or a stretched placeholder. Vertices are counted AFTER
# welding, so these thresholds are far above a cube (8 vertices, 6 faces, 1 group).
MIN_GROUPS = 10
MIN_VERTICES = 120
MIN_FACES = 100


class ObjBuilder:
    """Accumulates positions, normals and grouped faces, then serialises an OBJ."""

    def __init__(self):
        self.positions = []
        self._position_index = {}
        self.normals = []
        self._normal_index = {}
        self.groups = []  # list of (name, [ (pos_idx, nrm_idx) x N ])

    def _add_position(self, point):
        # Positions are shared between the faces that meet at them, so the emitted mesh has
        # welded corners rather than one loose vertex per face corner.
        key = tuple(round(c, DECIMALS) for c in point)
        if key not in self._position_index:
            self.positions.append(key)
            self._position_index[key] = len(self.positions)  # OBJ indices are 1-based
        return self._position_index[key]

    def _add_normal(self, normal):
        key = tuple(round(c, DECIMALS) for c in normal)
        if key not in self._normal_index:
            self.normals.append(key)
            self._normal_index[key] = len(self.normals)
        return self._normal_index[key]

    def begin_group(self, name):
        self.groups.append((name, []))

    def add_polygon(self, points, normal):
        """Add one convex polygon. points are ordered counter-clockwise seen from outside."""
        normal_index = self._add_normal(normal)
        corner = [(self._add_position(p), normal_index) for p in points]
        self.groups[-1][1].append(corner)

    def counts(self):
        vertices = len(self.positions)
        faces = sum(len(faces) for _, faces in self.groups)
        return vertices, faces, len(self.groups)

    def bounds(self):
        xs = [p[0] for p in self.positions]
        ys = [p[1] for p in self.positions]
        zs = [p[2] for p in self.positions]
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def serialise(self):
        lines = [
            "# {0} - original fictional STW first-person weapon presentation asset".format(ASSET_NAME),
            "# Generated deterministically by tools/assets/create_stw_smg_01.py",
            "# Axis convention: +X right, +Y aim/forward, +Z up (engine-local visual proportions)",
            "# No mtllib / usemtl is emitted: the runtime binds one Atom StandardPBR material",
            "# through the mesh feature processor's default custom material slot.",
            "o {0}".format(ASSET_NAME),
        ]
        for position in self.positions:
            lines.append("v {0:.6f} {1:.6f} {2:.6f}".format(*position))
        for normal in self.normals:
            lines.append("vn {0:.6f} {1:.6f} {2:.6f}".format(*normal))
        for name, faces in self.groups:
            lines.append("g {0}".format(name))
            for corner in faces:
                lines.append("f " + " ".join("{0}//{1}".format(p, n) for p, n in corner))
        return "\n".join(lines) + "\n"


def _normalise(vector):
    length = sum(component * component for component in vector) ** 0.5
    if length == 0.0:
        return (0.0, 0.0, 1.0)
    return tuple(component / length for component in vector)


def _face_normal(points):
    ax, ay, az = points[0]
    bx, by, bz = points[1]
    cx, cy, cz = points[2]
    u = (bx - ax, by - ay, bz - az)
    v = (cx - ax, cy - ay, cz - az)
    return _normalise((u[1] * v[2] - u[2] * v[1],
                       u[2] * v[0] - u[0] * v[2],
                       u[0] * v[1] - u[1] * v[0]))


def add_block(builder, y_back, y_front, back, front, lift_back=0.0, lift_front=0.0):
    """Add a closed six-sided block spanning y_back..y_front along the aim axis.

    back / front are (half_width, half_height) pairs, so the block may taper along its
    length. lift_* offsets the section centre vertically, which produces wedges and
    angled bodies rather than plain axis-aligned boxes.
    """
    bw, bh = back
    fw, fh = front
    # Corner order per section: lower-left, lower-right, upper-right, upper-left (seen from +Y)
    b = [(-bw, y_back, lift_back - bh), (bw, y_back, lift_back - bh),
         (bw, y_back, lift_back + bh), (-bw, y_back, lift_back + bh)]
    f = [(-fw, y_front, lift_front - fh), (fw, y_front, lift_front - fh),
         (fw, y_front, lift_front + fh), (-fw, y_front, lift_front + fh)]

    quads = [
        [f[0], f[1], f[2], f[3]],   # front cap (+Y)
        [b[1], b[0], b[3], b[2]],   # back cap (-Y)
        [b[0], b[1], f[1], f[0]],   # bottom (-Z)
        [b[3], f[3], f[2], b[2]],   # top (+Z)
        [b[1], b[2], f[2], f[1]],   # right (+X)
        [b[0], f[0], f[3], b[3]],   # left (-X)
    ]
    for quad in quads:
        builder.add_polygon(quad, _face_normal(quad))


def add_offset_block(builder, y_back, y_front, back, front, x_offset,
                     lift_back=0.0, lift_front=0.0):
    """Same as add_block but shifted along the right axis, for asymmetric detail.

    The offset is baked into the emitted corner positions rather than patched afterwards,
    because positions are shared between faces once welded.
    """
    original = builder.add_polygon

    def shifted(points, normal):
        original([(x + x_offset, y, z) for (x, y, z) in points], normal)

    builder.add_polygon = shifted
    try:
        add_block(builder, y_back, y_front, back, front, lift_back, lift_front)
    finally:
        builder.add_polygon = original


def build():
    """Compose the STW_SMG_01 silhouette from named components."""
    builder = ObjBuilder()

    # 1. Main receiver body - the central mass the rest of the weapon hangs off.
    builder.begin_group("receiver_body")
    add_block(builder, -0.170, 0.090, (0.036, 0.052), (0.034, 0.050))

    # 2. Upper rail - flat raised deck along the top of the receiver.
    builder.begin_group("upper_rail")
    add_block(builder, -0.150, 0.120, (0.022, 0.010), (0.020, 0.009), 0.058, 0.056)

    # 3. Optic housing - fictional sight block, tapered towards the front.
    builder.begin_group("optic_housing")
    add_block(builder, -0.090, 0.010, (0.026, 0.026), (0.022, 0.021), 0.090, 0.086)

    # 4. Optic lens hood - small angled shade in front of the housing.
    builder.begin_group("optic_lens_hood")
    add_block(builder, 0.010, 0.046, (0.021, 0.020), (0.018, 0.016), 0.086, 0.079)

    # 5. Forward shroud - tapering barrel shroud, the dominant forward silhouette.
    builder.begin_group("forward_shroud")
    add_block(builder, 0.090, 0.268, (0.031, 0.036), (0.021, 0.024), 0.004, 0.006)

    # 6. Shroud vents - three raised ribs breaking up the shroud surface.
    builder.begin_group("shroud_vents")
    for index, y in enumerate((0.120, 0.166, 0.212)):
        shrink = 0.002 * index
        add_block(builder, y, y + 0.020,
                  (0.030 - shrink, 0.007), (0.029 - shrink, 0.007), 0.036, 0.034)

    # 7. Muzzle device - fictional compensator block at the very front.
    builder.begin_group("muzzle_device")
    add_block(builder, 0.268, 0.318, (0.023, 0.026), (0.019, 0.021), 0.006, 0.006)

    # 8. Energy cell housing - angled block under the forward receiver.
    builder.begin_group("cell_housing")
    add_block(builder, 0.030, 0.150, (0.028, 0.022), (0.024, 0.016), -0.066, -0.058)

    # 9. Magazine well - canted magazine below the receiver.
    builder.begin_group("magazine_well")
    add_block(builder, -0.070, 0.020, (0.026, 0.062), (0.024, 0.058), -0.110, -0.100)

    # 10. Grip - rear grip, canted backwards.
    builder.begin_group("grip")
    add_block(builder, -0.168, -0.086, (0.024, 0.060), (0.022, 0.056), -0.108, -0.092)

    # 11. Trigger guard - three thin members forming an open loop.
    builder.begin_group("trigger_guard")
    add_block(builder, -0.086, -0.074, (0.020, 0.026), (0.020, 0.026), -0.078, -0.078)  # rear post
    add_block(builder, -0.074, -0.022, (0.020, 0.006), (0.020, 0.006), -0.100, -0.100)  # lower bar
    add_block(builder, -0.030, -0.018, (0.020, 0.022), (0.020, 0.022), -0.074, -0.074)  # front post

    # 12. Rear brace - tapered tail closing the silhouette behind the receiver.
    builder.begin_group("rear_brace")
    add_block(builder, -0.246, -0.170, (0.020, 0.034), (0.030, 0.046), 0.010, 0.006)

    # 13. Side plate - asymmetric panel on the right flank only.
    builder.begin_group("side_plate")
    add_offset_block(builder, -0.140, -0.010, (0.006, 0.030), (0.006, 0.026), 0.038, 0.006, 0.004)

    # 14. Ejection block - small asymmetric detail opposite the side plate.
    builder.begin_group("ejection_block")
    add_offset_block(builder, -0.056, 0.006, (0.005, 0.016), (0.005, 0.014), -0.038, 0.022, 0.020)

    return builder


def validate(builder):
    """Enforce the geometry quality floor. Returns a dict of reported facts."""
    vertices, faces, groups = builder.counts()
    minimum, maximum = builder.bounds()
    extents = tuple(maximum[i] - minimum[i] for i in range(3))

    problems = []
    if groups < MIN_GROUPS:
        problems.append("group count {0} < {1}".format(groups, MIN_GROUPS))
    if vertices < MIN_VERTICES:
        problems.append("vertex count {0} < {1}".format(vertices, MIN_VERTICES))
    if faces < MIN_FACES:
        problems.append("face count {0} < {1}".format(faces, MIN_FACES))
    for value in list(minimum) + list(maximum):
        if value != value or value in (float("inf"), float("-inf")):
            problems.append("non-finite coordinate {0}".format(value))
    for axis, extent in zip("xyz", extents):
        if extent <= 0.0:
            problems.append("zero extent on {0}".format(axis))
    if problems:
        raise SystemExit("{0} geometry validation failed: {1}".format(ASSET_NAME, "; ".join(problems)))

    return {
        "vertices": vertices,
        "faces": faces,
        "groups": groups,
        "min": minimum,
        "max": maximum,
        "extents": extents,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Generate the original STW_SMG_01 weapon asset.")
    default_output = os.path.join("stw-o3de", "Project", "Assets", "Weapons", ASSET_NAME,
                                  "{0}.obj".format(ASSET_NAME))
    parser.add_argument("--output", default=default_output, help="path of the OBJ to write")
    parser.add_argument("--check", action="store_true",
                        help="validate and report only; do not write the file")
    args = parser.parse_args(argv)

    builder = build()
    facts = validate(builder)
    text = builder.serialise()

    print("STW_ASSET_NAME={0}".format(ASSET_NAME))
    print("STW_ASSET_VERTEX_COUNT={0}".format(facts["vertices"]))
    print("STW_ASSET_FACE_COUNT={0}".format(facts["faces"]))
    print("STW_ASSET_GROUP_COUNT={0}".format(facts["groups"]))
    print("STW_ASSET_BOUNDS_MIN={0:.6f},{1:.6f},{2:.6f}".format(*facts["min"]))
    print("STW_ASSET_BOUNDS_MAX={0:.6f},{1:.6f},{2:.6f}".format(*facts["max"]))
    print("STW_ASSET_EXTENTS={0:.6f},{1:.6f},{2:.6f}".format(*facts["extents"]))
    print("STW_ASSET_BYTES={0}".format(len(text.encode("utf-8"))))

    if args.check:
        return 0

    directory = os.path.dirname(os.path.abspath(args.output))
    if directory:
        os.makedirs(directory, exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    print("STW_ASSET_WRITTEN={0}".format(args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
