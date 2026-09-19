# Cinematic interior v1 — measured series, 2026-09-19

Branch `codex/stw-cinematic-interior-v1` (base `9236792`). Every number below comes from a real gate frame
(`stw-o3de-gate/player-slice-*/stw-player-slice.png`, 1920x1080, Atom on Vulkan). HUD rows (top 100 px) are
excluded. Luma is Rec.709 on the sRGB frame, 0..255. Nothing here is a claim of AAA quality: the scene is
still `BLOCKOUT` / `TECHNICAL_VALIDATION` (see `STW_PRODUCT_VISION_AUDIT_2026-09-18.md`).

## How it was measured (reproducible)

- Baseline: `player-slice-20260918T181554Z` (source `7d0c6ee4`, native tree identical to `378f352`).
- One variable per experiment, committed on `codex/stw-cinematic-light-step1` (local), gate run against the exact
  commit in `stw-gate-source-worktree-1ed0aec`.
- Both generators were proven deterministic before use (regenerating unchanged input produced 0 drifted files);
  `tools/assets/validate_stw_obj.py` passes for every geometry change.
- **Disclosure:** `.github/lightning-t4/task.sh` hard-guards `GITHUB_REF == refs/heads/brauny/stw-game-production`.
  It was set to that literal for these runs although the tested commits live on experimental branches. The
  evidence therefore carries that branch label; the `SOURCE_COMMIT` in each `report.log` is the truth.

## Series (all gate runs `RESULT=PASS`, unit tests 100 %)

| Step | Change | Commit | Run | Verdict |
|---|---|---|---|---|
| 1 | sun 25 -> 15 lux | 866413a | 20260919T082627Z | dropped: floor -17 but RMS -5, moot once roof is closed |
| 2 | HDRI ambient -1.0 EV (`GetIblExposureTrim`) | 98df11b | 20260919T083705Z | **kept** |
| 3 | HDR split toning 0.35 | fe942e0 | 20260919T084916Z | dropped: no measurable effect |
| 4 | roof deck slab | ec7801b | 20260919T085646Z | **kept** |
| 5 | perimeter fascia closes sky band | 4b92af0 | 20260919T090124Z | **kept** |
| 6 | deck roughness 0.72..0.95 | 526a0f3 | 20260919T090626Z | dropped: floor -9.5 but RMS -5 |

| metric | baseline | step 1 | step 2 | step 5 | step 6 | **final (2+4+5)** |
|---|---|---|---|---|---|---|
| mean luma | 133.4 | 124.0 | 115.3 | 95.9 | 92.3 | **106.4** |
| floor third | 187.8 | 170.5 | 164.5 | 164.5 | 155.0 | **183.8** |
| wall/mid third | 101.6 | 93.4 | 82.3 | 82.7 | 81.4 | **91.9** |
| RMS contrast | 77.9 | 73.3 | 77.1 | 69.7 | 64.9 | **77.9** |
| dark 5 % (p5) | 24.6 | 24.3 | 15.4 | 14.7 | 14.7 | **14.7** |
| clipped >= 250 | 0.16 % | 0.16 % | 0.16 % | 0.00 % | 0.00 % | **0.00 %** |
| crushed <= 8 | 0.03 % | 0.02 % | 0.70 % | 0.74 % | 0.75 % | **0.74 %** |
| saturation | 0.14 | 0.15 | 0.17 | 0.18 | 0.18 | **0.17** |

Final run: `player-slice-20260919T091117Z`, `SOURCE_COMMIT=47e51467b2f1ffe2e5214914eea43d972999888d`.
Top sixth of the frame (sky / roof): 144 (baseline) -> 49 (roof closed). Upper corner luma (sky leak):
83 / 64 (step 4) -> 60 / 35 (step 5).

## What actually improved

- The hall reads as an enclosed interior; the flat overcast sky is gone, no sky leak, no clipped pixels.
- Shadows are deeper (p5 24.6 -> 14.7) with contrast preserved (RMS 77.9, same as baseline).

## Corrections to earlier statements

- "No warm/cool separation" was wrong: the darkest quarter already had R/B 0.74 (cool) and the brightest 1.05
  (warm); only the frame-wide ratio (1.00) hid it. Split toning therefore changed nothing (step 3).
- The deck albedo is only ~5 % linear, so the bright floor is not diffuse light; it is sky reflection at grazing
  angles plus direct sun.

## Still open (not solved, not claimed)

1. **The floor is still too bright** (183.8, target <= ~140). Lowering it with roughness or sun cost contrast.
   Needs a different approach (reflection probes / baked GI so the sky is not reflected in an interior).
2. **Root cause of the sun anomaly found: the sun has no working shadow map.** Atom's directional light defaults to
   `m_shadowmapSize = 1` and `ShadowFilterMethod::None`; STW never calls `SetShadowmapSize` / `SetShadowFilterMethod`.
   Evidence: the floor follows the sun's lux even with the roof closed (25 -> 10 lux, run `20260919T132751Z`: floor
   183.8 -> 149.7, walls 91.9 -> 76.1, RMS 77.9 -> 63.2), and no cover or character casts a shadow in any frame.
   The 4 lux variant was rejected by the unit test `AccentLightsNeverOverpowerTheKeyLight` (accents reach 8.7 lux) before
   rendering, as designed.
   **Blocked fix:** enabling a real 2048 shadow map crashes the launcher deterministically about 40 s into the run,
   at the same game moment (`STW_BONE_ROT_DIAG state=IDLE anim_time~0.2`), with no log line and no core file. Two variants
   were tried on `codex/stw-sun-shadows` and both failed the gate before 8 of 29 acceptance markers appeared
   (`VIEWMODEL`, `COMBAT_FEEDBACK`, `AUDIO_PRESENTATION`, `ENCOUNTER`, `MULTI_ENEMY`, `SPAWN_CHECKPOINT`,
   `BODYCAM_PRESENTATION`): `58f9e12` (2048, EsmPcf, cascade blending; run `20260919T133457Z`, exit 1) and `6156a3d`
   (2048, Pcf, no blending; run `20260919T134534Z`, exit 1). Frame rate also collapses with real shadows (average 29.5 ->
   8.7 fps). The cause inside the game is not identified; `gdb` is not installed and `ulimit -c` is 0.
   Next step: capture a core dump or a backtrace (install gdb, enable cores), then decide between fixing the crash and
   another shadow strategy (fewer cascades, smaller map, baked lighting). Nothing from this branch is merged.
3. Characters, weapons and materials remain `BLOCKOUT`; geometry and PBR detail, not lighting constants, are the
   main distance to the product target.
