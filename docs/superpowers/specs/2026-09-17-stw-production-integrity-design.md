# STW Production Integrity Phase 0 Design

## Objective

Bring the canonical STW O3DE production branch to a state where the controlled
production gate proves the current runtime contract from the real built game,
not from source inspection or a browser substitute. The gate must continue to
reject missing gameplay, presentation, physics, asset, and runtime evidence.

## Current evidence baseline

- Repository: `MarsCommanderM/shooter-game-concept`
- Production branch: `brauny/stw-game-production`
- Isolated implementation branch: `codex/stw-phase0-production`
- Candidate base: `394eb1c8a2c47f0a56123429eb19f0b58d3ce91b`
- Fresh remote production head: `bfdc27f4adb237c55777dcc24de564f3f68010a3`
- Repository guard: passed for the isolated branch and canonical repository.
- Build, registered test discovery, 26 suites/357 cases, AssetProcessorBatch,
  Vulkan/Tesla-T4 launch, PhysX, arena, traversal, combat, AI, audio,
  presentation, and weapon-switch markers were observed in the latest run.
- The initial gate failed because the required marker
  `LOADOUT_ACCEPTANCE result=PASS` was not emitted; the controlled diagnostic
  proved a held-switch edge was not released before the acceptance probe.
- The initial runtime also exposed O3DE `Matrix3x4` zero-determinant warnings
  from transient Atom mesh scales. The trace proved the exact presentation
  path; the production path now keeps hidden handles nonsingular and uses
  visibility rather than a near-zero scale.
- A later asset-quality pass removed missing `TANGENT0`, `UV0`, and `UV1`
  streams from the rendered OBJ/GLB sources. The gate now treats both mesh
  stream and zero-determinant warnings as hard production failures.

## Design constraints

1. `stw-o3de/` remains the authoritative game implementation.
2. Acceptance diagnostics may expose state, but the official PASS marker may be
   emitted only when every real predicate passes.
3. Fixes must preserve authority separation: gameplay state remains authoritative
   and presentation remains read-only.
4. Host-network execution is required for the O3DE Asset Processor server; this
   is an execution-environment requirement, not a reason to weaken the build.
5. Every phase ends with a fresh full gate run and archived evidence.

## Phase-0 exit criteria

- The loadout acceptance failure has a source-level root cause and a regression
  test or deterministic runtime proof.
- The skeletal presentation warning source is corrected or the run is blocked
  by a concrete external engine defect with evidence.
- The complete controlled gate passes on the Tesla T4 with all required markers,
  asset processing, tests, and real runtime evidence.
- The final gate report and fresh runtime log contain zero mesh-input-stream
  warnings and zero zero-determinant warnings.
- The isolated branch is cleanly reviewable with exact commits and no generated
  cache/user files included.
