#!/usr/bin/env python3
"""Add deterministic UV0-compatible and normal streams to simple STW OBJ assets.

The first-person and enemy source meshes are intentionally authored as simple convex
parts, but Atom's StandardPBR contract still requires usable UV and tangent inputs.
This tool keeps the source asset deterministic while deriving face-local normals and
planar UVs from the authored geometry.  UV1 is resolved by Atom to the first UV set.

No asset is downloaded or inferred from an external source.  The rewrite is idempotent:
an OBJ that already contains complete ``v/t/n`` faces is rejected rather than rewritten.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path


DECIMALS = 6


def _parse_index(token: str, count: int) -> int:
    value = int(token)
    if value < 0:
        return count + value
    return value


def _normal(a: tuple[float, float, float], b: tuple[float, float, float], c: tuple[float, float, float]) -> tuple[float, float, float]:
    ux, uy, uz = (b[i] - a[i] for i in range(3))
    vx, vy, vz = (c[i] - a[i] for i in range(3))
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = math.sqrt(nx * nx + ny * ny + nz * nz)
    if length <= 1e-12:
        raise ValueError("degenerate face cannot receive a normal")
    return (nx / length, ny / length, nz / length)


def _uv(point: tuple[float, float, float], normal: tuple[float, float, float]) -> tuple[float, float]:
    """Project each face onto its dominant plane so every convex box face has UV area."""
    x, y, z = point
    ax, ay, az = (abs(component) for component in normal)
    if az >= ax and az >= ay:
        return (x, y)
    if ay >= ax:
        return (x, z)
    return (y, z)


def rewrite(path: Path) -> tuple[str, int, int]:
    lines = path.read_text(encoding="utf-8").splitlines()
    existing_uv_count = sum(1 for line in lines if line.startswith("vt "))
    existing_normal_count = sum(1 for line in lines if line.startswith("vn "))
    if existing_uv_count or existing_normal_count:
        if not existing_uv_count or not existing_normal_count:
            raise ValueError(f"{path}: source contains only one of vt/vn streams")
        for line in lines:
            fields = line.split()
            if fields and fields[0] == "f":
                if any(len(corner.split("/")) != 3 for corner in fields[1:]):
                    raise ValueError(f"{path}: existing face does not use position/uv/normal indices")
        return "\n".join(lines) + "\n", existing_uv_count, existing_normal_count

    positions: list[tuple[float, float, float]] = []
    first_face_line = None
    for line_index, line in enumerate(lines):
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "v":
            positions.append(tuple(float(value) for value in fields[1:4]))
        elif fields[0] == "f":
            if first_face_line is None:
                first_face_line = line_index
            if len(fields) < 4:
                raise ValueError(f"{path}: face at line {line_index + 1} is not a polygon")
            points = []
            for corner in fields[1:]:
                position_token = corner.split("/", 1)[0]
                position_index = _parse_index(position_token, len(positions))
                points.append(positions[position_index - 1])
            face_normal = _normal(points[0], points[1], points[2])
    if first_face_line is None:
        raise ValueError(f"{path}: no faces found")

    # Re-read face geometry now that every position index is known.  This also preserves
    # the original face winding while assigning a face-local normal and UV per corner.
    uv_streams: list[tuple[float, float]] = []
    normal_streams: list[tuple[float, float, float]] = []
    generated_faces: dict[int, str] = {}
    for line_index, line in enumerate(lines):
        fields = line.split()
        if not fields or fields[0] != "f":
            continue
        position_indices = [
            _parse_index(corner.split("/", 1)[0], len(positions)) for corner in fields[1:]
        ]
        points = [positions[index - 1] for index in position_indices]
        face_normal = _normal(points[0], points[1], points[2])
        normal_streams.append(face_normal)
        normal_index = len(normal_streams)
        corner_refs = []
        for position_index, point in zip(position_indices, points):
            uv_streams.append(_uv(point, face_normal))
            corner_refs.append(f"{position_index}/{len(uv_streams)}/{normal_index}")
        generated_faces[line_index] = "f " + " ".join(corner_refs)

    output: list[str] = []
    streams_inserted = False
    for line_index, line in enumerate(lines):
        fields = line.split()
        if fields and fields[0] in {"vt", "vn"}:
            continue
        if line_index == first_face_line and not streams_inserted:
            output.extend(f"vt {u:.{DECIMALS}f} {v:.{DECIMALS}f}" for u, v in uv_streams)
            output.extend(
                f"vn {x:.{DECIMALS}f} {y:.{DECIMALS}f} {z:.{DECIMALS}f}"
                for x, y, z in normal_streams
            )
            streams_inserted = True
        if fields and fields[0] == "f":
            output.append(generated_faces[line_index])
        else:
            output.append(line)
    return "\n".join(output) + "\n", len(uv_streams), len(normal_streams)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--check", action="store_true", help="verify the deterministic rewrite without writing")
    args = parser.parse_args(argv)

    result = 0
    for path in args.paths:
        expected, uv_count, normal_count = rewrite(path)
        current = path.read_text(encoding="utf-8")
        if args.check:
            if current != expected:
                print(f"STW_OBJ_STREAMS=FAIL path={path} uv={uv_count} normals={normal_count}")
                result = 1
            else:
                print(f"STW_OBJ_STREAMS=PASS path={path} uv={uv_count} normals={normal_count}")
        else:
            path.write_text(expected, encoding="utf-8", newline="\n")
            print(f"STW_OBJ_STREAMS=WRITTEN path={path} uv={uv_count} normals={normal_count}")
    return result


if __name__ == "__main__":
    sys.exit(main())
