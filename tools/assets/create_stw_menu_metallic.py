#!/usr/bin/env python3
"""Generate STW_Menu_Metallic_UIL.tif + .sprite - a real, tinted LyShine button
texture giving the main menu its "metallischer Look" (the user's original
main-menu request), replacing the current flat-color-only button fill.

Deterministic, no external assets: a brushed-metal streak pattern with a
lighter top bevel highlight and a darker bottom bevel shadow, authored in
near-neutral tones so it composites correctly with UiImageBus::SetColor's
existing multiplicative steel tint (MainMenuPresentation.cpp's
SteelLightR/G/B) rather than fighting it - the texture supplies the
metallic SHAPE (brushed streaks + bevel), the existing tint still supplies
the final hue.

Filename ends in "_UIL", not "_Metallic": AssetProcessor's
ImageProcessingAtom builder assigns a preset purely from the LAST
underscore-delimited suffix of the filename (BuilderSettingManager::
GetFileMask/GetSuggestedPreset, used whenever no .assetinfo sidecar exists -
verified directly from Code/Source/BuilderSettings/BuilderSettingManager.cpp,
not assumed), matched against the table in
Gems/Atom/Asset/ImageProcessingAtom/Assets/Config/ImageBuilder.settings.
"_metallic" is registered there (line ~196) to the "Reflectance" preset -
BC4, single-channel - meant for real PBR metalness maps (this project's own
STW_ARENA_01_metallic.png etc. correctly rely on exactly this). The prior
attempt at this same asset was named "STW_Menu_Metallic.tif" and silently
got compressed to single-channel BC4 as a result - confirmed by a real
AssetProcessorBatch run showing "[BC4_UNORM] converted with preset
[Reflectance]" in the job log for that filename. "_uil" is ALSO a
registered suffix (same settings file, ~line 218) mapping to
"UserInterface_Lossless" - R8G8B8A8, sRGB, uncompressed - the engine's own
intended, correct preset for exactly this kind of small UI sprite texture.

Paired implicitly by filename stem with its .sprite sidecar (LyShine's own
convention - confirmed against Gems/UiBasics/Assets/Textures/Basic/*.sprite
in the engine, not assumed): STW_Menu_Metallic_UIL.tif +
STW_Menu_Metallic_UIL.sprite next to each other. The .sprite's 9-slice
margins preserve the top/bottom bevel edges while letting the uniform-per-
row brushed streaks stretch freely to any button width/height without a
visible seam.
"""

from pathlib import Path

import numpy as np
from PIL import Image

ASSET_DIR = Path(__file__).parents[2] / "stw-o3de/Project/Assets/UI/STW_Menu_Metallic_UIL"
WIDTH = 128
HEIGHT = 48
BEVEL_PX = 6
SEED = 20260924


def _brushed_metal() -> np.ndarray:
    rng = np.random.default_rng(SEED)

    # Base near-white value so the existing multiplicative SetColor tint
    # (SteelLightR/G/B ~= 0.16/0.20/0.24) still controls the final hue - the
    # texture only supplies luminance variation, not color.
    base = np.full((HEIGHT, WIDTH), 0.82, dtype=np.float64)

    # Brushed streaks: independent per-row brightness noise, replicated
    # across every column (a real horizontal brushed-metal look, not
    # per-pixel static), smoothed slightly so it reads as directional
    # brushing rather than random speckle.
    row_noise = rng.normal(0.0, 0.035, size=HEIGHT)
    kernel = np.array([0.25, 0.5, 0.25])
    row_noise = np.convolve(row_noise, kernel, mode="same")
    streaks = np.tile(row_noise[:, None], (1, WIDTH))

    # Soft embossed dome across the stretchable middle band: lighter just
    # under the top bevel, gently darkening toward the bottom bevel.
    middle = np.linspace(0.06, -0.06, HEIGHT)[:, None]
    middle = np.tile(middle, (1, WIDTH))

    value = base + streaks + middle

    # Top bevel highlight (light catching a raised metal edge) and bottom
    # bevel shadow (the same edge's underside) - real, not decorative:
    # this is what makes the button read as beveled metal rather than a
    # flat-painted rectangle once the sprite's 9-slice keeps these rows
    # fixed regardless of button width.
    for row in range(BEVEL_PX):
        highlight = 0.22 * (1.0 - row / BEVEL_PX)
        value[row, :] += highlight
    for row in range(BEVEL_PX):
        shadow = 0.30 * (1.0 - row / BEVEL_PX)
        value[HEIGHT - 1 - row, :] -= shadow

    return np.clip(value, 0.0, 1.0)


def _write_tif(value: np.ndarray, path: Path) -> None:
    rgb = np.stack([value, value, value], axis=-1)
    rgba = np.concatenate([rgb, np.ones((*value.shape, 1))], axis=-1)
    pixels = (rgba * 255.0 + 0.5).astype(np.uint8)
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(pixels, mode="RGBA").save(path, format="TIFF")


def _write_sprite(path: Path) -> None:
    left = 0.0
    right = 1.0
    top = BEVEL_PX / HEIGHT
    bottom = (HEIGHT - BEVEL_PX) / HEIGHT
    path.write_text(
        '<Sprite versionNumber="1">\n'
        f' <Sprite m_left="{left:.4f}" m_right="{right:.4f}" '
        f'm_top="{top:.4f}" m_bottom="{bottom:.4f}"/>\n'
        "</Sprite>\n"
    )


def main() -> None:
    value = _brushed_metal()
    _write_tif(value, ASSET_DIR / "STW_Menu_Metallic_UIL.tif")
    _write_sprite(ASSET_DIR / "STW_Menu_Metallic_UIL.sprite")
    print(f"Wrote {ASSET_DIR / 'STW_Menu_Metallic_UIL.tif'}")
    print(f"Wrote {ASSET_DIR / 'STW_Menu_Metallic_UIL.sprite'}")


if __name__ == "__main__":
    main()
