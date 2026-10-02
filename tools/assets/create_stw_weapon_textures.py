#!/usr/bin/env python3
"""Deterministic original PBR maps for the ten STW weapon profiles.

Writes 2048-square baseColor, metallic, roughness, normal and AO PNGs beside
each weapon material and points the existing StandardPBR propertyValues at them.
Nothing is downloaded. The maps assign no production classification.

The filename suffix is the ImageBuilder preset key: _basecolor, _metallic,
_roughness, _normal, _ao.

Usage:
    python3 tools/assets/create_stw_weapon_textures.py [--check]
"""

import json
import struct
import sys
import zlib
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[2]
WEAPON_ROOT = REPO / "stw-o3de" / "Project" / "Assets" / "Weapons"
EDGE = 2048
MAPS = ("basecolor", "metallic", "roughness", "normal", "ao")

PROFILES = {
    "STW_SMG_01": {"seed": 1101, "rows": 8, "cols": 3, "accent": (0.20, 0.22, 0.24)},
    "STW_RIFLE_02": {"seed": 1202, "rows": 10, "cols": 2, "accent": (0.02, 0.45, 0.48)},
    "STW_RIFLE_03": {"seed": 1303, "rows": 12, "cols": 2, "accent": (0.10, 0.40, 0.46)},
    "STW_LMG_04": {"seed": 1404, "rows": 6, "cols": 4, "accent": (0.28, 0.32, 0.34)},
    "STW_SIDEARM_01": {"seed": 1505, "rows": 4, "cols": 2, "accent": (0.48, 0.16, 0.28)},
    "STW_LAUNCHER_01": {"seed": 1606, "rows": 5, "cols": 3, "accent": (0.52, 0.30, 0.08)},
    "STW_TACTICAL_FLASH_01": {"seed": 1707, "rows": 3, "cols": 3, "accent": (0.78, 0.68, 0.16)},
    "STW_TACTICAL_SMOKE_01": {"seed": 1808, "rows": 4, "cols": 3, "accent": (0.20, 0.46, 0.40)},
    "STW_LETHAL_FRAG_01": {"seed": 1909, "rows": 5, "cols": 5, "accent": (0.55, 0.14, 0.10)},
    "STW_MELEE_01": {"seed": 2010, "rows": 2, "cols": 8, "accent": (0.16, 0.48, 0.72)},
}


def _rng(seed):
    return np.random.default_rng(seed)


def _value_noise(seed, cells):
    rng = _rng(seed)
    acc = np.zeros((EDGE, EDGE), dtype=np.float32)
    amp_total = 0.0
    amp = 1.0
    freq = cells
    for _octave in range(4):
        grid = rng.random((freq, freq)).astype(np.float32)
        ys = (np.arange(EDGE) / EDGE * freq) % freq
        xs = (np.arange(EDGE) / EDGE * freq) % freq
        y0 = np.floor(ys).astype(np.int32) % freq
        x0 = np.floor(xs).astype(np.int32) % freq
        y1 = (y0 + 1) % freq
        x1 = (x0 + 1) % freq
        fy = (ys - np.floor(ys)).astype(np.float32)[:, None]
        fx = (xs - np.floor(xs)).astype(np.float32)[None, :]
        top = grid[np.ix_(y0, x0)] * (1.0 - fx) + grid[np.ix_(y0, x1)] * fx
        bot = grid[np.ix_(y1, x0)] * (1.0 - fx) + grid[np.ix_(y1, x1)] * fx
        acc += (top * (1.0 - fy) + bot * fy) * amp
        amp_total += amp
        amp *= 0.5
        freq *= 2
    return acc / amp_total


def _panel_mask(rows, cols, seam):
    y = np.linspace(0.0, 1.0, EDGE, endpoint=False, dtype=np.float32)
    x = np.linspace(0.0, 1.0, EDGE, endpoint=False, dtype=np.float32)
    gy = np.minimum((y * rows) % 1.0, 1.0 - (y * rows) % 1.0)
    gx = np.minimum((x * cols) % 1.0, 1.0 - (x * cols) % 1.0)
    gap = np.minimum(gy[:, None], gx[None, :])
    return np.clip(gap / seam, 0.0, 1.0).astype(np.float32)


def _normal_from_height(height, strength):
    height = height.astype(np.float32)
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * (0.5 * strength)
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * (0.5 * strength)
    length = np.sqrt(dx * dx + dy * dy + 1.0)
    normal = np.stack([-dx / length, -dy / length, 1.0 / length], axis=-1)
    return ((normal * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)


def _u8(field):
    return (np.clip(field, 0.0, 1.0) * 255.0).astype(np.uint8)


def _write_png(path, array):
    if array.ndim == 2:
        color_type = 0
        planes = array[:, :, None]
    else:
        color_type = 2
        planes = array
    height, width = planes.shape[0], planes.shape[1]
    raw = bytearray()
    for row in range(height):
        raw.append(0)
        raw.extend(planes[row].tobytes())

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    path.write_bytes(png)


def _tint(profile):
    material = json.loads((WEAPON_ROOT / profile / f"{profile}.material").read_text(encoding="utf-8"))
    color = material["propertyValues"]["baseColor.color"]
    return tuple(float(channel) for channel in color[:3])


def build_maps(profile, spec):
    tint = np.array(_tint(profile), dtype=np.float32)
    accent = np.array(spec["accent"], dtype=np.float32)
    panels = _panel_mask(spec["rows"], spec["cols"], 0.08)
    grain = _value_noise(spec["seed"], 4)
    wear = _value_noise(spec["seed"] + 17, 2)
    seam = 1.0 - panels
    seam_rgb = seam[:, :, None]
    grain_rgb = grain[:, :, None]
    base = tint * (0.82 + 0.18 * grain_rgb) * (1.0 - 0.35 * seam_rgb) + accent * seam_rgb * 0.65
    base = np.clip(base, 0.02, 0.92)
    metallic = np.clip(0.72 * panels + 0.08 * seam + 0.05 * wear, 0.0, 1.0)
    roughness = np.clip(0.28 + 0.45 * seam + 0.12 * grain, 0.08, 0.92)
    height = panels * (0.65 + 0.35 * grain)
    ambient = np.clip(0.55 + 0.45 * panels - 0.15 * seam, 0.2, 1.0)
    return {
        "basecolor": np.stack([_u8(base[:, :, 0]), _u8(base[:, :, 1]), _u8(base[:, :, 2])], axis=-1),
        "metallic": _u8(metallic),
        "roughness": _u8(roughness),
        "normal": _normal_from_height(height, 4.0),
        "ao": _u8(ambient),
    }


def _png_edge(path):
    header = path.read_bytes()[:24]
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise SystemExit(f"{path} is not a PNG")
    width, height = struct.unpack(">II", header[16:24])
    return width, height


def write_materials():
    keys = {
        "basecolor": ("baseColor.textureMap", "baseColor.useTexture"),
        "metallic": ("metallic.textureMap", "metallic.useTexture"),
        "roughness": ("roughness.textureMap", "roughness.useTexture"),
        "normal": ("normal.textureMap", "normal.useTexture"),
        "ao": ("occlusion.diffuseTextureMap", "occlusion.diffuseUseTexture"),
    }
    for profile in PROFILES:
        path = WEAPON_ROOT / profile / f"{profile}.material"
        data = json.loads(path.read_text(encoding="utf-8"))
        values = data.setdefault("propertyValues", {})
        for suffix, (texture_key, use_key) in keys.items():
            values[texture_key] = f"Textures/{profile}_{suffix}.png"
            values[use_key] = True
        path.write_text(json.dumps(data, indent=4) + "\n", encoding="utf-8")


def validate_written():
    problems = []
    expected = {
        "baseColor.textureMap": "basecolor",
        "metallic.textureMap": "metallic",
        "roughness.textureMap": "roughness",
        "normal.textureMap": "normal",
        "occlusion.diffuseTextureMap": "ao",
    }
    for profile in PROFILES:
        directory = WEAPON_ROOT / profile / "Textures"
        material = json.loads((WEAPON_ROOT / profile / f"{profile}.material").read_text(encoding="utf-8"))
        values = material["propertyValues"]
        for suffix in MAPS:
            path = directory / f"{profile}_{suffix}.png"
            if not path.is_file():
                problems.append(f"missing {path.name}")
                continue
            width, height = _png_edge(path)
            if (width, height) != (EDGE, EDGE):
                problems.append(f"{path.name} is {width}x{height}")
        for key, suffix in expected.items():
            relative = f"Textures/{profile}_{suffix}.png"
            if values.get(key) != relative or not (WEAPON_ROOT / profile / relative).is_file():
                problems.append(f"{profile} {key} does not resolve")
    if problems:
        raise SystemExit("weapon textures failed: " + "; ".join(problems))


def main(argv=None):
    check_only = "--check" in (argv if argv is not None else sys.argv[1:])
    if not check_only:
        for profile, spec in PROFILES.items():
            directory = WEAPON_ROOT / profile / "Textures"
            directory.mkdir(parents=True, exist_ok=True)
            for suffix, image in build_maps(profile, spec).items():
                destination = directory / f"{profile}_{suffix}.png"
                _write_png(destination, image)
                print(f"STW_WEAPON_TEXTURE profile={profile} map={suffix} bytes={destination.stat().st_size}")
        write_materials()
    validate_written()
    print(f"STW_WEAPON_TEXTURES edge={EDGE} profiles={len(PROFILES)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
