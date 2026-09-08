#!/usr/bin/env python3
"""Generate STW's repository-owned two-joint training mannequin fixture.

The output is a self-contained text glTF: no downloaded model, texture, sound,
or proprietary source asset is involved. Geometry and animation values below
are deliberately small and auditable.
"""

import argparse
import base64
import json
import math
import struct
from pathlib import Path


positions = []
normals = []
texcoords = []
joints = []
weights = []
indices = []


def influence(y, mode):
    if mode == "root":
        upper = 0.0
    elif mode == "upper":
        upper = 1.0
    else:
        upper = max(0.0, min(1.0, (y - 0.88) / 0.42))
    return (0, 1, 0, 0), (1.0 - upper, upper, 0.0, 0.0)


def add_box(center, size, mode):
    cx, cy, cz = center
    sx, sy, sz = (component * 0.5 for component in size)
    corners = [
        (-sx, -sy, -sz), (sx, -sy, -sz),
        (sx, sy, -sz), (-sx, sy, -sz),
        (-sx, -sy, sz), (sx, -sy, sz),
        (sx, sy, sz), (-sx, sy, sz),
    ]
    faces = [
        ((0, 1, 2, 3), (0.0, 0.0, -1.0)),
        ((5, 4, 7, 6), (0.0, 0.0, 1.0)),
        ((4, 0, 3, 7), (-1.0, 0.0, 0.0)),
        ((1, 5, 6, 2), (1.0, 0.0, 0.0)),
        ((3, 2, 6, 7), (0.0, 1.0, 0.0)),
        ((4, 5, 1, 0), (0.0, -1.0, 0.0)),
    ]
    uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    for face, normal in faces:
        first = len(positions)
        for corner_index, uv in zip(face, uvs):
            x, y, z = corners[corner_index]
            position = (cx + x, cy + y, cz + z)
            joint, weight = influence(position[1], mode)
            positions.append(position)
            normals.append(normal)
            texcoords.append(uv)
            joints.append(joint)
            weights.append(weight)
        indices.extend((first, first + 1, first + 2,
                        first, first + 2, first + 3))


# Lower body, blended pelvis/torso, arms, head, and a small chest plate.
add_box((-0.24, 0.43, 0.0), (0.32, 0.86, 0.34), "root")
add_box((0.24, 0.43, 0.0), (0.32, 0.86, 0.34), "root")
add_box((0.0, 0.91, 0.0), (0.78, 0.30, 0.42), "blend")
add_box((0.0, 1.38, 0.0), (0.92, 0.74, 0.44), "blend")
add_box((-0.60, 1.38, 0.0), (0.24, 0.82, 0.26), "upper")
add_box((0.60, 1.38, 0.0), (0.24, 0.82, 0.26), "upper")
add_box((0.0, 1.97, 0.0), (0.46, 0.46, 0.46), "upper")
add_box((0.0, 1.42, 0.25), (0.54, 0.38, 0.08), "upper")


def quaternion(axis, degrees):
    half = math.radians(degrees) * 0.5
    sine = math.sin(half)
    return (axis[0] * sine, axis[1] * sine, axis[2] * sine, math.cos(half))


def multiply(left, right):
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return (
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    )


idle_times = (0.0, 1.0, 2.0)
idle_rotations = tuple(quaternion((0.0, 0.0, 1.0), angle)
                       for angle in (-3.0, 3.0, -3.0))
move_times = (0.0, 0.5, 1.0)
move_rotations = tuple(quaternion((0.0, 0.0, 1.0), angle)
                       for angle in (-18.0, 20.0, -18.0))
fire_times = (0.0, 0.10, 0.32)
fire_rotations = (
    quaternion((1.0, 0.0, 0.0), 0.0),
    multiply(quaternion((0.0, 0.0, 1.0), -12.0),
             quaternion((1.0, 0.0, 0.0), -32.0)),
    quaternion((1.0, 0.0, 0.0), 0.0),
)


binary = bytearray()
buffer_views = []
accessors = []


def append_view(payload, target=None):
    while len(binary) % 4:
        binary.append(0)
    offset = len(binary)
    binary.extend(payload)
    view = {"buffer": 0, "byteOffset": offset, "byteLength": len(payload)}
    if target is not None:
        view["target"] = target
    buffer_views.append(view)
    return len(buffer_views) - 1


def floats(values):
    flattened = [component for value in values for component in value]
    return struct.pack("<" + "f" * len(flattened), *flattened)


def add_accessor(view, component_type, count, shape, minimum=None, maximum=None):
    accessor = {
        "bufferView": view,
        "componentType": component_type,
        "count": count,
        "type": shape,
    }
    if minimum is not None:
        accessor["min"] = list(minimum)
    if maximum is not None:
        accessor["max"] = list(maximum)
    accessors.append(accessor)
    return len(accessors) - 1


mins = tuple(min(value[axis] for value in positions) for axis in range(3))
maxs = tuple(max(value[axis] for value in positions) for axis in range(3))
position_accessor = add_accessor(
    append_view(floats(positions), 34962), 5126, len(positions), "VEC3", mins, maxs)
normal_accessor = add_accessor(
    append_view(floats(normals), 34962), 5126, len(normals), "VEC3")
texcoord_accessor = add_accessor(
    append_view(floats(texcoords), 34962), 5126, len(texcoords), "VEC2")
joint_payload = struct.pack(
    "<" + "H" * (len(joints) * 4),
    *(component for value in joints for component in value))
joint_accessor = add_accessor(
    append_view(joint_payload, 34962), 5123, len(joints), "VEC4")
weight_accessor = add_accessor(
    append_view(floats(weights), 34962), 5126, len(weights), "VEC4")
index_payload = struct.pack("<" + "H" * len(indices), *indices)
index_accessor = add_accessor(
    append_view(index_payload, 34963), 5123, len(indices), "SCALAR")

inverse_bind = (
    (1.0, 0.0, 0.0, 0.0,
     0.0, 1.0, 0.0, 0.0,
     0.0, 0.0, 1.0, 0.0,
     0.0, 0.0, 0.0, 1.0),
    (1.0, 0.0, 0.0, 0.0,
     0.0, 1.0, 0.0, 0.0,
     0.0, 0.0, 1.0, 0.0,
     0.0, -1.05, 0.0, 1.0),
)
inverse_bind_accessor = add_accessor(
    append_view(floats(inverse_bind)), 5126, 2, "MAT4")


def add_scalar(values):
    return add_accessor(append_view(floats(tuple((value,) for value in values))),
                        5126, len(values), "SCALAR")


def add_quaternions(values):
    return add_accessor(append_view(floats(values)), 5126, len(values), "VEC4")


idle_time_accessor = add_scalar(idle_times)
idle_rotation_accessor = add_quaternions(idle_rotations)
move_time_accessor = add_scalar(move_times)
move_rotation_accessor = add_quaternions(move_rotations)
fire_time_accessor = add_scalar(fire_times)
fire_rotation_accessor = add_quaternions(fire_rotations)


def animation(name, time_accessor, rotation_accessor):
    return {
        "name": name,
        "samplers": [{
            "input": time_accessor,
            "output": rotation_accessor,
            "interpolation": "LINEAR",
        }],
        "channels": [{
            "sampler": 0,
            "target": {"node": 1, "path": "rotation"},
        }],
    }


document = {
    "asset": {
        "version": "2.0",
        "generator": "STW repository-owned gameplay mannequin generator",
        "extras": {
            "purpose": "Production gameplay presentation and T4-B state acceptance",
            "license": "Repository-owned procedural geometry and animation",
            "policy": "Idle/Move loop; Fire is a non-looping gameplay override",
        },
    },
    "scene": 0,
    "scenes": [{"name": "Training", "nodes": [0, 2]}],
    "nodes": [
        {"name": "RootJoint", "children": [1]},
        {"name": "UpperBodyJoint", "translation": [0.0, 1.05, 0.0]},
        {"name": "TrainingMannequin", "mesh": 0, "skin": 0},
    ],
    "skins": [{
        "name": "TrainingMannequinSkin",
        "inverseBindMatrices": inverse_bind_accessor,
        "skeleton": 0,
        "joints": [0, 1],
    }],
    "meshes": [{
        "name": "ProceduralTrainingMannequin",
        "primitives": [{
            "attributes": {
                "POSITION": position_accessor,
                "NORMAL": normal_accessor,
                "TEXCOORD_0": texcoord_accessor,
                "JOINTS_0": joint_accessor,
                "WEIGHTS_0": weight_accessor,
            },
            "indices": index_accessor,
            "material": 0,
        }],
    }],
    "materials": [{
        "name": "TrainingOrange",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.92, 0.24, 0.055, 1.0],
            "metallicFactor": 0.18,
            "roughnessFactor": 0.42,
        },
    }],
    "animations": [
        animation("Idle", idle_time_accessor, idle_rotation_accessor),
        animation("Move", move_time_accessor, move_rotation_accessor),
        animation("Fire", fire_time_accessor, fire_rotation_accessor),
    ],
    "buffers": [{
        "byteLength": len(binary),
        "uri": "data:application/octet-stream;base64," +
               base64.b64encode(binary).decode("ascii"),
    }],
    "bufferViews": buffer_views,
    "accessors": accessors,
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
