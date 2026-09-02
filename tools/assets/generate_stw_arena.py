#!/usr/bin/env python3
"""Generate the original deterministic STW_ARENA_01 static Atom source mesh."""
from pathlib import Path

OUT = Path(__file__).parents[2] / "stw-o3de/Project/Assets/Environment/STW_ARENA_01/STW_ARENA_01.obj"
boxes = [
    ("floor", (0, 0, -0.45), (24, 24, 0.8)),
    ("north_wall", (0, 11.75, 1.5), (24, .5, 3)),
    ("south_wall", (0, -11.75, 1.5), (24, .5, 3)),
    ("east_wall", (11.75, 0, 1.5), (.5, 24, 3)),
    ("west_wall", (-11.75, 0, 1.5), (.5, 24, 3)),
    ("left_cover", (-3.0, 1.5, 1.25), (1.5, 2, 2.5)),
    ("right_cover", (3.0, 1.5, 1.25), (1.5, 2, 2.5)),
    ("east_platform", (7.0, 3.5, .55), (5, 4, 1.1)),
    ("west_ramp_mass", (-7.0, -2.5, .8), (4, 5, 1.6)),
    ("lane_pylon_left", (-5.0, 5.0, 1.6), (1, 1, 3.2)),
    ("lane_pylon_right", (5.0, 5.0, 1.6), (1, 1, 3.2)),
    ("combat_arch_left", (-5.0, 8.5, 2.2), (0.8, 0.8, 4.4)),
    ("combat_arch_right", (5.0, 8.5, 2.2), (0.8, 0.8, 4.4)),
    ("combat_arch_header", (0.0, 8.5, 4.2), (10.8, 0.8, 0.7)),
]
verts, faces, lines = [], [], [
    "# STW_ARENA_01 original deterministic source",
    "mtllib STW_ARENA_01.mtl",
    "o STW_ARENA_01",
    "usemtl STW_ARENA_01",
]
corners = [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]
quads = [(1,2,3,4),(5,8,7,6),(1,5,6,2),(2,6,7,3),(3,7,8,4),(5,1,4,8)]
for name, center, size in boxes:
    base = len(verts)
    lines.append(f"# {name}")
    for c in corners:
        v = tuple(center[i] + c[i] * size[i] * .5 for i in range(3))
        # Assimp imports OBJ's Y-up coordinates into O3DE as (-X, Z, Y).
        # Author the inverse transform so the resulting Atom model uses the
        # intended O3DE Z-up arena coordinates.
        obj_v = (-v[0], v[2], v[1])
        verts.append(obj_v); lines.append("v %.4f %.4f %.4f" % obj_v)
    for q in quads:
        face = tuple(base + i for i in q); faces.append(face)
        lines.append("f %d %d %d %d" % face)
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"generated={OUT} vertices={len(verts)} faces={len(faces)}")
