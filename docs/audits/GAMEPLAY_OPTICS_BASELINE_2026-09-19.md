# Gameplay optics baseline (first-person sequence), 2026-09-19

Tool: `tools/evidence/capture_gameplay_sequence.sh` (opt-in runtime evidence mode `STW_NATIVE_CAPTURE_INTERVAL` /
`_MAX_FRAMES`, automated acceptance play, same Xvfb / process-local Vulkan ICD setup as the gate; it only writes under
`stw-o3de-gate/gameplay-sequence-<timestamp>/`). It needs a finished gate run first. Run:
`stw-o3de-gate/gameplay-sequence-20260919T143048Z` (48 frames at 0.5 s), build of integration `db1c66c` (gate run
`player-slice-20260919T142242Z`).

| metric (HUD excluded) | value |
|---|---|
| frames | 48 |
| mean luma (min / max over frames) | 73.4 (59.9 / 85.2) |
| RMS contrast | 51.8 |
| clipped >= 250 / crushed <= 8 | 0.09 % / 0.49 % |

Contact sheet: `stw-o3de-gate/gameplay-sequence-20260919T143048Z/contact-sheet.png`.

## What the sequence shows

- Environment: enclosed hall, real sun shadows, skylight bars, cool fill; stable across the clip.
- **Gameplay visuals are still blockout, and this is the largest remaining gap:**
  the first-person rifle is a flat, untextured teal block with no arms or hands; enemies are cube humanoids
  (`STW_CHARACTER_01`) that fall apart as blocks; no visible muzzle flash or impact feedback in the captured frames.

## What would close it (not started)

1. Animated enemies from the engine's Rin character (currently only a static model in `STW_ENEMY_01_RIN`, no actor): an
   `ActorGroup` import for Rin, the Mocap clips of the MotionMatching gem (Walk / Jog / Run / Crouch; **no** death clip),
   a presentation switch, scale and collision alignment, then a real ragdoll for death.
2. A textured first-person weapon with arms (asset decision needed: own models or licensed / CC0 sources with recorded
   provenance).
3. Reflection probes / Diffuse Probe Grid (gem enabled, unused) for the bright grazing-angle deck reflection.
