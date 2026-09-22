#!/usr/bin/env python3
"""Check that the visual look contract matches the eight arena colliders."""

import argparse
import re
import sys
from pathlib import Path


EXPECTED_BOXES = {
    "STW Floor": ((0.0, 0.0, -0.5), (24.0, 24.0, 1.0)),
    "STW North Wall": ((0.0, 12.0, 2.0), (24.0, 0.5, 4.0)),
    "STW South Wall": ((0.0, -12.0, 2.0), (24.0, 0.5, 4.0)),
    "STW East Wall": ((12.0, 0.0, 2.0), (0.5, 24.0, 4.0)),
    "STW West Wall": ((-12.0, 0.0, 2.0), (0.5, 24.0, 4.0)),
    "STW Left Cover": ((-2.25, 0.0, 1.25), (1.5, 2.0, 2.5)),
    "STW Right Cover": ((2.25, 0.0, 1.25), (1.5, 2.0, 2.5)),
    "STW Step": ((5.0, -2.0, 0.125), (2.0, 2.0, 0.25)),
}

BOX_PATTERN = re.compile(
    r'\{\s*"(?P<name>[^"]+)",\s*'
    r'AZ::Vector3\((?P<center>[^)]+)\),\s*'
    r'AZ::Vector3\((?P<size>[^)]+)\)\s*\}'
)
VECTOR_PATTERN = re.compile(
    r"^\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+))f?\s*,\s*"
    r"([-+]?(?:\d+(?:\.\d*)?|\.\d+))f?\s*,\s*"
    r"([-+]?(?:\d+(?:\.\d*)?|\.\d+))f?\s*$"
)


def parse_vector(value: str) -> tuple[float, float, float]:
    match = VECTOR_PATTERN.fullmatch(value)
    if not match:
        raise ValueError(f"unsupported vector syntax: {value!r}")
    return tuple(map(float, match.groups()))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repo", type=Path, help="root of the shooter-game-concept checkout")
    args = parser.parse_args()
    source = args.repo / "stw-o3de/Gems/STWGameplay/Code/Source/Clients/PhysXArenaRuntime.cpp"
    if not source.is_file():
        print(f"ERROR: PhysXArenaRuntime missing: {source}", file=sys.stderr)
        return 2

    actual = {}
    for match in BOX_PATTERN.finditer(source.read_text(encoding="utf-8")):
        name = match.group("name")
        if name in actual:
            print(f"ERROR: duplicate PhysX body {name!r}", file=sys.stderr)
            return 1
        try:
            actual[name] = (parse_vector(match.group("center")), parse_vector(match.group("size")))
        except ValueError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 1

    if actual != EXPECTED_BOXES:
        print("ERROR: PhysX collision bodies differ from the look template.", file=sys.stderr)
        for name in sorted(set(EXPECTED_BOXES) | set(actual)):
            if actual.get(name) != EXPECTED_BOXES.get(name):
                print(f"  {name}: expected={EXPECTED_BOXES.get(name)} found={actual.get(name)}", file=sys.stderr)
        return 1
    print("STW_LOOK_COLLISION_CONTRACT_OK: exactly eight known PhysX boxes unchanged")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
