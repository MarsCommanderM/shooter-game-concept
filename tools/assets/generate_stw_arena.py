#!/usr/bin/env python3
"""Generate the original deterministic STW arena meshes with render-ready streams."""

from math import sqrt
from pathlib import Path


ASSET_DIR = Path(__file__).parents[2] / "stw-o3de/Project/Assets/Environment/STW_ARENA_01"
BASE_OUT = ASSET_DIR / "STW_ARENA_01.obj"

BASE_BOXES = [
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

BOX_CORNERS = [
    (-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
    (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1),
]
# Outward-facing winding for the six box faces. The corner numbering is
# shared by every generated box, so keeping winding here in agreement with the
# geometric normal makes Atom's default back-face culling correct.
BOX_QUADS = [(1, 4, 3, 2), (5, 6, 7, 8), (1, 2, 6, 5),
             (2, 3, 7, 6), (3, 4, 8, 7), (1, 5, 8, 4)]
QUAD_UVS = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))


def _normal(a, b, c):
    # The face winding is authored outward, so the geometric cross product is
    # also the outward render normal used by the generated product.
    ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
    ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
    cross = (
        ab[1] * ac[2] - ab[2] * ac[1],
        ab[2] * ac[0] - ab[0] * ac[2],
        ab[0] * ac[1] - ab[1] * ac[0],
    )
    length = sqrt(sum(component * component for component in cross))
    return tuple(component / length for component in cross)


def _write_streamed_mesh(path, object_name, boxes):
    """Write box geometry with per-face normals and non-degenerate UV0 coordinates.

    UV1 is intentionally mapped by Atom's material UV fallback to UV0 when the
    material asks for its second set. SceneProcessing derives TANGENT0 with
    MikkTSpace from these UVs and the supplied normals.
    """
    lines = [
        f"# {object_name} original deterministic STW arena source",
        f"o {object_name}",
    ]
    streamed = []
    for name, center, size in boxes:
        lines.append(f"g {name}")
        corners = [
            tuple(center[i] + corner[i] * size[i] * 0.5 for i in range(3))
            for corner in BOX_CORNERS
        ]
        for quad in BOX_QUADS:
            quad_positions = [corners[index - 1] for index in quad]
            normal = _normal(quad_positions[0], quad_positions[1], quad_positions[2])
            streamed.extend((position, uv, normal) for position, uv in zip(quad_positions, QUAD_UVS))

    for position, _, _ in streamed:
        lines.append("v %.6f %.6f %.6f" % position)
    for _, uv, _ in streamed:
        lines.append("vt %.6f %.6f" % uv)
    for _, _, normal in streamed:
        lines.append("vn %.6f %.6f %.6f" % normal)

    vertex_index = 1
    for _ in boxes:
        for _ in BOX_QUADS:
            face = [f"{vertex_index + offset}/{vertex_index + offset}/{vertex_index + offset}" for offset in range(4)]
            lines.append("f " + " ".join(face))
            vertex_index += 4

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"generated={path} vertices={len(streamed)} faces={len(streamed) // 4}")


def _read_box_faces(path):
    """Read existing visual-only box faces so their authored layout is preserved."""
    vertices = []
    faces = []
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            vertices.append(tuple(float(value) for value in parts[1:4]))
        elif parts[0] == "f":
            faces.append(tuple(int(value.split("/")[0]) for value in parts[1:5]))
    return vertices, faces


def _rewrite_existing_visual_mesh(path):
    vertices, faces = _read_box_faces(path)
    if not vertices or not faces:
        raise ValueError(f"Arena visual mesh is empty: {path}")

    object_name = path.stem
    lines = [
        f"# {object_name} original deterministic visual-only source with render streams",
        f"o {object_name}",
    ]
    streamed = []
    for face in faces:
        quad_positions = [vertices[index - 1] for index in face]
        normal = _normal(quad_positions[0], quad_positions[1], quad_positions[2])
        streamed.extend((position, uv, normal) for position, uv in zip(quad_positions, QUAD_UVS))

    for position, _, _ in streamed:
        lines.append("v %.6f %.6f %.6f" % position)
    for _, uv, _ in streamed:
        lines.append("vt %.6f %.6f" % uv)
    for _, _, normal in streamed:
        lines.append("vn %.6f %.6f %.6f" % normal)

    for index in range(0, len(streamed), 4):
        face = [f"{index + 1 + offset}/{index + 1 + offset}/{index + 1 + offset}" for offset in range(4)]
        lines.append("f " + " ".join(face))

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"streamed={path} vertices={len(streamed)} faces={len(streamed) // 4}")


ASSET_DIR.mkdir(parents=True, exist_ok=True)
_write_streamed_mesh(BASE_OUT, "STW_ARENA_01", BASE_BOXES)
for visual_name in ("STW_ARENA_TRIM_01", "STW_ARENA_LANDMARK_01", "STW_ARENA_COVER_01"):
    _rewrite_existing_visual_mesh(ASSET_DIR / f"{visual_name}.obj")
