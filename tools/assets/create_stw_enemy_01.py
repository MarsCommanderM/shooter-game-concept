#!/usr/bin/env python3
"""Generate the original fictional STW_ENEMY_01 robotic combat-unit OBJ."""

import argparse
import os
import sys

from create_stw_smg_01 import ObjBuilder, add_block, add_offset_block


def build():
    mesh = ObjBuilder()
    parts = [
        ("armored_core", -0.30, 0.30, (0.48, 0.52), (0.42, 0.47), 1.25, 1.25, 0.0),
        ("sensor_head", -0.18, 0.18, (0.28, 0.22), (0.24, 0.20), 1.92, 1.92, 0.0),
        ("crown_fin", -0.08, 0.08, (0.08, 0.23), (0.05, 0.20), 2.27, 2.27, 0.0),
        ("left_shoulder", -0.20, 0.22, (0.24, 0.22), (0.20, 0.18), 1.52, 1.52, -0.56),
        ("right_shoulder", -0.20, 0.22, (0.24, 0.22), (0.20, 0.18), 1.52, 1.52, 0.56),
        ("left_arm", -0.13, 0.15, (0.14, 0.40), (0.12, 0.34), 1.05, 1.05, -0.64),
        ("right_arm", -0.13, 0.15, (0.14, 0.40), (0.12, 0.34), 1.05, 1.05, 0.64),
        ("left_leg", -0.18, 0.20, (0.20, 0.50), (0.16, 0.43), 0.46, 0.46, -0.27),
        ("right_leg", -0.18, 0.20, (0.20, 0.50), (0.16, 0.43), 0.46, 0.46, 0.27),
        ("left_foot", -0.32, 0.34, (0.22, 0.13), (0.19, 0.11), 0.10, 0.10, -0.27),
        ("right_foot", -0.32, 0.34, (0.22, 0.13), (0.19, 0.11), 0.10, 0.10, 0.27),
        ("chest_reactor", 0.30, 0.43, (0.20, 0.22), (0.13, 0.16), 1.34, 1.34, 0.0),
    ]
    for name, y0, y1, back, front, z0, z1, x in parts:
        mesh.begin_group(name)
        if x:
            add_offset_block(mesh, y0, y1, back, front, x, z0, z1)
        else:
            add_block(mesh, y0, y1, back, front, z0, z1)
    return mesh


def serialise(mesh):
    text = mesh.serialise()
    return text.replace(
        "# STW_SMG_01 - original fictional STW first-person weapon presentation asset\n"
        "# Generated deterministically by tools/assets/create_stw_smg_01.py\n",
        "# STW_ENEMY_01 - original fictional robotic alien combat unit\n"
        "# Generated deterministically by tools/assets/create_stw_enemy_01.py\n",
    ).replace("o STW_SMG_01", "o STW_ENEMY_01")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="stw-o3de/Project/Assets/Enemies/STW_ENEMY_01/STW_ENEMY_01.obj")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = serialise(build())
    if args.check:
        try:
            with open(args.output, encoding="utf-8") as stream:
                current = stream.read()
        except FileNotFoundError:
            return 1
        return 0 if current == data else 1
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
