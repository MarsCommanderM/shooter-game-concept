#!/usr/bin/env python3
"""Generate the STW arena's production PBR material families.

Authors, deterministically and with zero external assets, a set of tiling
StandardPBR texture maps (baseColor / normal / roughness / metallic / AO, plus
emissive and height where the family calls for it) and the matching `.material`
definitions. One family per material the arena kit binds; every `.material` file
in the arena directory is rewritten in place, and because every field is seeded
only the families whose parameters actually changed produce a diff.

UV note: the kit meshes now carry world-planar UVs scaled in metres per tile
(deck 2 m, walls 1.5 m, structure 1 m), so these maps tile across the surface
instead of being stretched once across a whole 24 m face. Panel rows/cols are
tuned for that tile size.

Requires only numpy - PNG writing is a dependency-free zlib/struct writer below.
"""

import json
import struct
import zlib
from pathlib import Path

import numpy as np

ASSET_DIR = Path(__file__).parents[2] / "stw-o3de/Project/Assets/Environment/STW_ARENA_01"
TEX_DIR = ASSET_DIR / "Textures"
RES = 512
MATERIAL_TYPE = "Materials/Types/StandardPBR.materialtype"
MATERIAL_TYPE_VERSION = 5


# --------------------------------------------------------------------------- #
# deterministic procedural fields
# --------------------------------------------------------------------------- #
def _rng(seed):
    return np.random.default_rng(seed)


def _value_noise(seed, cells, octaves=4):
    """Tiling fractal value noise in [0, 1]."""
    rng = _rng(seed)
    acc = np.zeros((RES, RES), dtype=np.float64)
    amp_total = 0.0
    amp = 1.0
    freq = cells
    for _ in range(octaves):
        grid = rng.random((freq, freq))
        # bilinear upsample with wrap
        ys = (np.arange(RES) / RES * freq) % freq
        xs = (np.arange(RES) / RES * freq) % freq
        y0 = np.floor(ys).astype(int) % freq
        x0 = np.floor(xs).astype(int) % freq
        y1 = (y0 + 1) % freq
        x1 = (x0 + 1) % freq
        fy = (ys - np.floor(ys))[:, None]
        fx = (xs - np.floor(xs))[None, :]
        top = grid[np.ix_(y0, x0)] * (1 - fx) + grid[np.ix_(y0, x1)] * fx
        bot = grid[np.ix_(y1, x0)] * (1 - fx) + grid[np.ix_(y1, x1)] * fx
        acc += (top * (1 - fy) + bot * fy) * amp
        amp_total += amp
        amp *= 0.5
        freq *= 2
    return acc / amp_total


def _panel_mask(rows, cols, seam):
    """Height field: recessed seams between raised panels (tiling)."""
    y = np.linspace(0, 1, RES, endpoint=False)
    x = np.linspace(0, 1, RES, endpoint=False)
    gy = np.minimum((y * rows) % 1.0, 1.0 - (y * rows) % 1.0)
    gx = np.minimum((x * cols) % 1.0, 1.0 - (x * cols) % 1.0)
    g = np.minimum(gy[:, None], gx[None, :])
    return np.clip(g / seam, 0.0, 1.0)


def _bolts(seed, rows, cols, radius):
    rng = _rng(seed)
    field = np.zeros((RES, RES), dtype=np.float64)
    yy, xx = np.mgrid[0:RES, 0:RES]
    for r in range(rows):
        for c in range(cols):
            cy = int((r + 0.5) / rows * RES)
            cx = int((c + 0.5) / cols * RES)
            jitter = rng.integers(-3, 4, size=2)
            d = np.sqrt((yy - cy - jitter[0]) ** 2 + (xx - cx - jitter[1]) ** 2)
            field = np.maximum(field, np.clip(1.0 - d / radius, 0.0, 1.0))
    return field


def _normal_from_height(height, strength):
    h = height.astype(np.float64)
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5 * strength
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5 * strength
    nz = np.ones_like(h)
    length = np.sqrt(dx * dx + dy * dy + nz * nz)
    n = np.stack([-dx / length, -dy / length, nz / length], axis=-1)
    return ((n * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)


# --------------------------------------------------------------------------- #
# minimal dependency-free PNG writer (8-bit, L or RGB)
# --------------------------------------------------------------------------- #
def _write_png(path, array):
    if array.ndim == 2:
        color_type = 0
        planes = array[:, :, None]
    else:
        color_type = 2
        planes = array
    h, w = planes.shape[0], planes.shape[1]
    raw = bytearray()
    for row in range(h):
        raw.append(0)
        raw.extend(planes[row].tobytes())
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, color_type, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    path.write_bytes(png)


def _u8(field):
    return (np.clip(field, 0.0, 1.0) * 255.0).astype(np.uint8)


def _rgb(r, g, b):
    return np.stack([_u8(r), _u8(g), _u8(b)], axis=-1)


# --------------------------------------------------------------------------- #
# family authoring
# --------------------------------------------------------------------------- #
# metal_coat is the coated/painted value that covers most of the surface;
# metal_bare is the value reached only where fasteners and worn seams expose raw
# metal. Block 26D shipped these inverted - the deck read 0.69 mean metallic at
# 0.42 roughness, which under the HDRI plus a 25000 lux key is a mirror, and it
# clipped to white across the largest surface in the frame.
FAMILIES = [
    # R1: parallax/POM removed from every family. Per-pixel POM on the base deck+wall mesh
    # (largest screen coverage) was a primary cost in the Block 26D fps collapse. normal +
    # roughness + metallic + AO carry the surface detail.
    dict(key="STW_ARENA_01", name="coated structural deck", seed=101,
         tint=(0.21, 0.23, 0.26), accent=(0.34, 0.31, 0.25),
         rows=2, cols=2, seam=0.04, metal_bare=0.35, metal_coat=0.02,
         rough_lo=0.46, rough_hi=0.84, emissive=None, height=False),
    dict(key="STW_ARENA_COVER_01", name="cover composite", seed=202,
         tint=(0.14, 0.15, 0.17), accent=(0.45, 0.13, 0.05),
         rows=2, cols=3, seam=0.09, metal_bare=0.45, metal_coat=0.05,
         rough_lo=0.42, rough_hi=0.78, emissive=None, height=False),
    dict(key="STW_ARENA_LANDMARK_01", name="emissive beacon", seed=303,
         tint=(0.09, 0.10, 0.12), accent=(0.05, 0.55, 0.62),
         rows=8, cols=2, seam=0.06, metal_bare=0.70, metal_coat=0.12,
         rough_lo=0.28, rough_hi=0.55,
         emissive=(0.05, 0.85, 0.95), height=False),
    dict(key="STW_ARENA_TRIM_01", name="hazard trim", seed=404,
         tint=(0.62, 0.50, 0.06), accent=(0.05, 0.05, 0.05),
         rows=1, cols=6, seam=0.12, metal_bare=0.55, metal_coat=0.06,
         rough_lo=0.34, rough_hi=0.70, emissive=None, height=False),
    dict(key="STW_ARENA_WALLPANEL_01", name="wall panel", seed=505,
         tint=(0.24, 0.26, 0.30), accent=(0.30, 0.33, 0.38),
         rows=3, cols=2, seam=0.05, metal_bare=0.55, metal_coat=0.08,
         rough_lo=0.38, rough_hi=0.72, emissive=None, height=False),
    dict(key="STW_ARENA_STRUCT_01", name="structural metal", seed=606,
         tint=(0.16, 0.17, 0.19), accent=(0.22, 0.20, 0.18),
         rows=2, cols=2, seam=0.07, metal_bare=0.85, metal_coat=0.18,
         rough_lo=0.30, rough_hi=0.62, emissive=None, height=False),
    dict(key="STW_ARENA_PROP_01", name="equipment and set dressing", seed=707,
         tint=(0.30, 0.28, 0.24), accent=(0.44, 0.31, 0.10),
         rows=2, cols=2, seam=0.08, metal_bare=0.55, metal_coat=0.10,
         rough_lo=0.40, rough_hi=0.78, emissive=None, height=False),
    dict(key="STW_ARENA_MARK_01", name="painted markings", seed=808,
         tint=(0.72, 0.62, 0.10), accent=(0.86, 0.86, 0.83),
         rows=1, cols=1, seam=0.20, metal_bare=0.10, metal_coat=0.00,
         rough_lo=0.55, rough_hi=0.80, emissive=None, height=False),
]


def author_family(fam):
    seed = fam["seed"]
    panels = _panel_mask(fam["rows"], fam["cols"], fam["seam"])
    grime = _value_noise(seed + 1, 3, octaves=5)
    fine = _value_noise(seed + 2, 12, octaves=4)
    bolts = _bolts(seed + 3, fam["rows"] + 1, fam["cols"] + 1, RES * 0.012)
    edges = 1.0 - panels  # seam channel

    # ---- height / normal ----
    height = (panels * 0.6 + bolts * 0.9 + (fine - 0.5) * 0.12
              + (grime - 0.5) * 0.05)
    height = (height - height.min()) / (height.ptp() + 1e-6)
    normal = _normal_from_height(height, strength=6.0)

    # ---- baseColor ----
    tint = np.array(fam["tint"])
    accent = np.array(fam["accent"])
    blend = (0.35 + 0.65 * panels)[:, :, None]
    grime_rgb = (0.55 + 0.45 * grime)[:, :, None]
    base = tint[None, None, :] * blend + accent[None, None, :] * (1.0 - blend) * 0.5
    base = base * (0.75 + 0.45 * grime_rgb)
    base = base * (1.0 - 0.35 * edges[:, :, None])          # darker seams
    base_rgb = _rgb(base[:, :, 0], base[:, :, 1], base[:, :, 2])

    # ---- roughness (spatially varying) ----
    rough = (fam["rough_lo"] + (fam["rough_hi"] - fam["rough_lo"])
             * (0.35 * (1.0 - panels) + 0.45 * grime + 0.20 * fine))
    rough = np.clip(rough + 0.10 * edges, 0.05, 0.98)
    rough_map = _u8(rough)

    # ---- metallic (coated surface, bare metal only where it is believable) ----
    # Driven by fasteners and worn panel edges rather than by a binary grime
    # threshold: a painted structural surface is dielectric almost everywhere,
    # and reads as metal only where the coating has actually been broken.
    wear = np.clip((grime - 0.58) / 0.32, 0.0, 1.0)
    exposed = np.clip(bolts * 1.10 + edges * 0.85 * wear + 0.25 * wear * fine, 0.0, 1.0)
    metal = fam["metal_coat"] + (fam["metal_bare"] - fam["metal_coat"]) * exposed
    metal = np.clip(metal, 0.0, 1.0)
    metal_map = _u8(metal)

    # ---- AO ----
    ao = np.clip(1.0 - 0.55 * edges - 0.25 * (1.0 - bolts) * (bolts > 0.05)
                 - 0.10 * (1.0 - grime), 0.25, 1.0)
    ao_map = _u8(ao)

    written = {}
    _write_png(TEX_DIR / f"{fam['key']}_basecolor.png", base_rgb); written["baseColor"] = 1
    _write_png(TEX_DIR / f"{fam['key']}_normal.png", normal); written["normal"] = 1
    _write_png(TEX_DIR / f"{fam['key']}_roughness.png", rough_map); written["roughness"] = 1
    _write_png(TEX_DIR / f"{fam['key']}_metallic.png", metal_map); written["metallic"] = 1
    _write_png(TEX_DIR / f"{fam['key']}_ao.png", ao_map); written["ao"] = 1

    if fam["emissive"]:
        strips = ((np.linspace(0, 1, RES, endpoint=False)[:, None] * fam["rows"]) % 1.0)
        glow = (np.clip(1.0 - np.abs(strips - 0.5) / 0.10, 0.0, 1.0)
                * (0.6 + 0.4 * fine))
        ec = np.array(fam["emissive"])
        emis = _rgb(glow * ec[0], glow * ec[1], glow * ec[2])
        _write_png(TEX_DIR / f"{fam['key']}_emissive.png", emis)
        written["emissive"] = 1

    if fam["height"]:
        _write_png(TEX_DIR / f"{fam['key']}_height.png", _u8(height))
        written["height"] = 1

    # ---- .material ----
    props = {
        "baseColor": {"textureMap": f"Textures/{fam['key']}_basecolor.png"},
        "metallic": {"textureMap": f"Textures/{fam['key']}_metallic.png"},
        "roughness": {"textureMap": f"Textures/{fam['key']}_roughness.png"},
        "normal": {"textureMap": f"Textures/{fam['key']}_normal.png", "factor": 1.0},
        "occlusion": {"diffuseTextureMap": f"Textures/{fam['key']}_ao.png"},
        "specularF0": {"factor": 0.5},
    }
    if fam["emissive"]:
        props["emissive"] = {
            "enable": True,
            "unit": "Ev100",
            "intensity": 4.0,
            "color": [fam["emissive"][0], fam["emissive"][1], fam["emissive"][2], 1.0],
            "textureMap": f"Textures/{fam['key']}_emissive.png",
            "useTexture": True,
        }
    if fam["height"]:
        props["parallax"] = {
            "textureMap": f"Textures/{fam['key']}_height.png",
            "useTexture": True,
            "factor": 0.02,
            "pdo": True,
            "quality": "High",
        }
    material = {
        "description": f"STW arena production PBR - {fam['name']} family (procedural, original).",
        "materialType": MATERIAL_TYPE,
        "materialTypeVersion": MATERIAL_TYPE_VERSION,
        "properties": props,
    }
    (ASSET_DIR / f"{fam['key']}.material").write_text(
        json.dumps(material, indent=4) + "\n", encoding="utf-8")
    return fam["key"], sorted(written)


def main():
    TEX_DIR.mkdir(parents=True, exist_ok=True)
    total_maps = 0
    for fam in FAMILIES:
        key, maps = author_family(fam)
        total_maps += len(maps)
        print(f"family={key:26s} maps={maps}")
    print(f"FAMILIES={len(FAMILIES)} TEXTURE_MAPS={total_maps} DIR={TEX_DIR}")


if __name__ == "__main__":
    main()
