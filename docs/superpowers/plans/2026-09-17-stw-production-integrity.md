# STW Production Integrity Phase 0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox notation and must be completed with fresh verification evidence.

## Scope

This plan executes the Phase-0 production-integrity work in the isolated
`codex/stw-phase0-production` worktree. It does not convert a single-process
acceptance run into a multiplayer proof; the existing `NOT_VERIFIED` authority
markers remain honest and are a later Phase-1 exit criterion.

## Tasks

- [x] Establish the canonical isolated worktree and pass `tools/stw-repo-guard.sh start`.
- [x] Run the controlled gate with host networking and archive its report.
- [x] Prove the immediate gate failure is the missing `LOADOUT_ACCEPTANCE` marker,
      not build, test registration, asset processing, GPU initialization, or
      launcher startup.
- [x] Add a one-shot diagnostic for every loadout acceptance predicate and run
      the real gate again to identify the first false predicate.
- [x] Correct the proven loadout runtime defect without weakening acceptance.
- [x] Add or strengthen a regression test for the corrected behavior.
- [x] Trace the zero-determinant presentation warnings to the exact
      transform input and correct the production presentation path.
- [x] Repair the rendered OBJ/GLB vertex-stream contract with deterministic
      UV/normal authoring and explicit skinned-mesh tangent generation.
- [x] Promote zero-determinant and missing-mesh-stream counts to hard gate
      failures, then prove both counts are zero in a fresh runtime log.
- [x] Rebuild, run the complete registered test suite, process assets, launch on
      Vulkan/Tesla T4, and require every gate marker to pass.
- [x] Review the final diff, generated-file status, repository guard, and exact
      evidence paths; the implementation worktree contains only source, tests,
      and documentation changes (generated cache/user files remain outside Git).

## Verification commands

```bash
bash tools/stw-repo-guard.sh start
cmake --build /teamspace/studios/this_studio/stw-o3de-build/linux --target STW.GameLauncher STWGameplay.Tests
ctest --test-dir /teamspace/studios/this_studio/stw-o3de-build/linux --output-on-failure
GITHUB_REPOSITORY=MarsCommanderM/shooter-game-concept \
GITHUB_REF=refs/heads/brauny/stw-game-production \
GITHUB_SHA=<verified-local-head> \
GITHUB_WORKSPACE=/teamspace/studios/this_studio/stw-phase0-production \
bash .github/lightning-t4/task.sh
```

The final gate report, launcher log, Game.log, and frame evidence are the
authoritative runtime evidence. No completion claim is valid from source
inspection alone.
