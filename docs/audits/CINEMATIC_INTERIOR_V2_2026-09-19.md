# Cinematic interior v2 — sun shadows and skylights, 2026-09-19

Branch `codex/stw-cinematic-interior-v2` (base `f33f583`). Same method as v1: every number is a gate frame
(`stw-o3de-gate/player-slice-*/stw-player-slice.png`, 1920x1080, HUD rows excluded, Rec.709 luma 0..255), one variable per
experiment, gate against the exact commit, `GITHUB_REF` set to the literal the script requires (the `SOURCE_COMMIT` in each
`report.log` is the truth). Still `BLOCKOUT` / `TECHNICAL_VALIDATION`; no claim of AAA quality.

## Root cause: the sun had no shadows

Atom's directional light defaults to `m_shadowmapSize = 1` and `ShadowFilterMethod::None`; STW never set either. Evidence:
the floor followed the sun's lux even with a closed roof (25 -> 10 lux: floor 184 -> 150, walls 92 -> 76, run
`20260919T132751Z`), and no cover or character cast a shadow in any frame. A 4 lux sun is rejected by the unit test
`AccentLightsNeverOverpowerTheKeyLight` (accents reach 8.7 lux) before rendering, as designed.

## Series

| Step | Change | Run | Verdict |
|---|---|---|---|
| 7a | sun shadow map 2048, EsmPcf, cascade blending | 20260919T133457Z | exit 1 (launcher gone), not reproduced later |
| 7b | same, plain PCF, no blending | 20260919T134534Z fail, then 20260919T135641Z PASS | **kept** |
| 7c | manual exposure -1.75 -> -0.75 EV | 20260919T140111Z | no effect (mean 61.30 vs 61.44); reverted |
| 7d | HDRI ambient -1 -> 0 EV | 20260919T140624Z | mean 77, RMS 41, but flat; reverted |
| 8 | three 1.6 m skylight slots in the roof (y = -4, 2, 8) | 20260919T141205Z | **kept** |

Manual exposure compensation does not drive the image (a full stop changed nothing); the HDRI exposure trim does.

| metric | v1 final (`20260919T091117Z`) | sun shadows (`...135641Z`) | + skylights (`...141205Z`) | v2 tip (`...141736Z`) |
|---|---|---|---|---|
| mean luma | 106.4 | 61.4 | 68.1 | **68.1** |
| floor third | 183.8 | 96.1 | 97.8 | **97.8** |
| wall/mid third | 91.9 | 54.3 | 59.4 | **59.4** |
| RMS contrast | 77.9 | 39.9 | 49.5 | **49.4** |
| clipped >= 250 | 0.00 % | 0.00 % | 0.00 % | **0.00 %** |
| crushed <= 8 | 0.74 % | 0.74 % | 0.82 % | **0.81 %** |
| average fps | 29.4 | 29.3 | 29.6 | **29.0** |

Final gate `player-slice-20260919T141736Z`, `SOURCE_COMMIT=d1eb9e93e90c5f98066df02ac68615856bb990ef`, `RESULT=PASS`,
unit tests 100 %. It reproduced the skylight run to within 0.1 luma, so the render is stable.

## What improved

- Real sun shadows: pillars, the character and the roof structure now shade the deck; the flat, sun-flooded look is gone.
- The skylight slots put hard sun bars on the deck and a streak on a wall, which is the first real light shaping in the scene.
- No frame-rate cost (29 fps average, unchanged).

## Still open

1. Global contrast is below the baseline (RMS 49 vs 78): the interior is ambient-lit and dim (mean 68). Needs deliberate
   interior key/practical lighting (accent rig, emissive strips) rather than more sun.
2. The sky is still reflected in the deck at grazing angles; reflection probes and a baked ambient (Diffuse Probe Grid gem
   is enabled but unused) are the documented Atom answer.
3. An unexplained intermittent failure: two of the first three runs with shadows ended with the launcher gone. Not reproduced
   in six later runs; no core file; `gdb` is now installed and core dumps enabled for the next occurrence.
4. Characters, weapons and surface detail remain `BLOCKOUT`; lighting alone cannot close the distance to the product target.

## Step 9 (branch `codex/stw-cinematic-interior-v3`): accent lights as practical lights

The closed, shadowed hall was ambient-lit (mean 68, RMS 49). The three accents were raised from 85/85/50 cd to
210/210/120 cd; their floor illuminance (16.2 / 16.2 / 20.8 lux) stays below the sun's 25 lux, which the unit test
`AccentLightsNeverOverpowerTheKeyLight` enforces. Gate `player-slice-20260919T142242Z`, `SOURCE_COMMIT=3ec4cf982e8cbaa1cba27b075f221b0ef798bc95`,
`RESULT=PASS`, average fps 30.5.

| metric | v2 tip | + brighter accents |
|---|---|---|
| mean luma | 68.1 | 83.0 |
| floor third | 97.8 | 122.0 |
| RMS contrast | 49.4 | 55.4 |
| p5 luma | 14.7 | 17.3 |
| clipped >= 250 / crushed <= 8 | 0.00 % / 0.81 % | 0.00 % / 0.45 % |
| R/B overall | 0.97 | 0.95 (cooler fill on walls and roof) |

Kept. Contrast is still below the very first baseline (78) because the scene no longer has a sun-flooded deck; the next
levers are point-light shadows for the practicals, emissive strips and reflection probes.
