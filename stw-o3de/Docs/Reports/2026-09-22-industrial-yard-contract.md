# 2026-09-22 — Industrial Yard Contract

Status: TECHNICAL_VALIDATION / integrated as arena variant / T4 runtime acceptance pending

## Before change

Local working branch: `codex/stw-rin-all-enemies`
Local HEAD: `53809640292638081f19f4129a73c35a91e5000d`
Fresh production remote: `bfdc27f4adb237c55777dcc24de564f3f68010a3`
Engine pin: O3DE 26.05.0 / `3db6943249d8bd7960b9ed7e9aee310b7668586e`

Visual target: 16:9 Industrial-Yard vertical slice using the existing eight
PhysX arena colliders, with wet concrete, facades, cover cladding, overhead
structure, signage and puddle material intents.

Affected paths:

- `stw-o3de/Gems/STWGameplay/Code/Include/STWGameplay/IndustrialYardLookTemplate.h`
- `stw-o3de/Gems/STWGameplay/Code/Source/IndustrialYardLookTemplate.cpp`
- `stw-o3de/Gems/STWGameplay/Code/Tests/Clients/IndustrialYardLookTemplateTests.cpp`
- `tools/check_stw_collision_contract.py`
- the three STWGameplay CMake file lists
- this report

Expected performance impact: none from the offline contract; no runtime assets,
lighting settings, mesh IDs or render path were changed. Any GPU, CPU, frame,
draw-call and memory impact remains UNMEASURED.

## Result

The C++ template describes 14 visual pieces and four StandardPBR material
starting points. It validates that visual skins remain within the existing
colliders, that overhead geometry remains above the arena, and that exterior
props remain beyond the west wall. The checker reads the actual collider owner,
`PhysXArenaRuntime`, rather than assuming player-runtime ownership.

Superseded by the integration on the same day: the `STW_INDUSTRIAL_YARD_01`
source/runtime set (nine FBX groups, six StandardPBR `.material` sources, thirty
PBR maps, `.blend` sources, provenance and target-bounds report) now lives under
`stw-o3de/Project/Assets/IndustrialYard/`. Its generators are versioned at
`tools/blender/` and are pinned to Blender 4.5.14 LTS (see `PROVENANCE.md`).
`ArenaPresentation` selects the yard as a complete nine-group variant with the
current `STW_ARENA_01` set as the only fallback, keeps partially loaded groups
hidden, re-binds materials once models exist and reports the rendered world
bounds of every group.

## Verification

The native T4 gate (`.github/lightning-t4/task.sh`) is the authority. It now
requires `STW_ARENA_VARIANT=IndustrialYard`, a capture taken while the yard is
active (`ARENA_VARIANT_AT_CAPTURE=IndustrialYard`) and
`INDUSTRIAL_YARD_BOUNDS=PASS`, which compares the rendered group bounds with
`STW_INDUSTRIAL_YARD_01.report.json`. The run ID, SHA and frame of the passing
run are recorded in the commit/PR that promotes this work; nothing here is
VERIFIED without that exact-revision run.

Visual Forge gap: the yard keeps the legacy `STW_` naming and has no LODs or
Visual Forge asset manifest, so `forge.py changes` would reject it. It stays
classified `TECHNICAL_VALIDATION` until that migration is done.

## Open and rollback

Open: author and license `STW_INDUSTRIAL_YARD_01` source/runtime assets;
connect a complete nine-group asset path set as an explicit ArenaPresentation
variant with the existing arena fallback; run Asset Processor and Vulkan;
capture movement and measure performance. Do not claim production readiness
from this offline contract.

Before commit, remove only the files listed in this report and reverse only
the corresponding CMake hunks. No branch reset or history rewrite is allowed.
