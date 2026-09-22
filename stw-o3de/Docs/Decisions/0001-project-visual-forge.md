# ADR 0001 — Project Visual Forge

Date: 2026-09-19. Status: accepted pipeline foundation; runtime rollout pending.

## Baseline and scope before implementation

Repository: MarsCommanderM/shooter-game-concept. Existing clean branch:
`codex/stw-rin-all-enemies`. Local baseline:
`53809640292638081f19f4129a73c35a91e5000d`. Fresh remote production:
`bfdc27f4adb237c55777dcc24de564f3f68010a3`. Repository guard passed;
production is an ancestor of this branch. Preserve the intervening work.

Implement standards, versioned targets, a dependency-free validation CLI,
negative tests and a CI contract check. Retain the canonical `stw-o3de/Project`
and `stw-o3de/Gems/STWGameplay` roots. No bulk asset rename, level rewrite,
engine upgrade, new branch or gameplay authority change is part of this task.

Affected paths: `stw-o3de/Config/VisualForge`, `stw-o3de/Tools/visual_forge`,
`stw-o3de/Docs`, `stw-o3de/Source`, `stw-o3de/README.md`, `.gitignore`,
`.gitattributes`, `.github/workflows/stw-visual-forge.yml`,
`docs/STW_PRODUCT_VISION.md` (navigation only), `AGENTS.md` (future visual-work rules).

Performance impact: offline validation only; no runtime settings changed.
GPU/CPU/frame/draw-call/memory impact is unmeasured, not reported as zero.
Budgets are provisional acceptance targets, not measured capabilities.

## Decisions

- The existing product vision remains authoritative: cinematic, hyper-realistic
  sci-fi multiplayer FPS with bodycam presentation and authoritative physics.
- Keep DCC originals in `stw-o3de/Source/` outside the project's asset scan root.
  This implements the requested `_Source` separation without importing originals.
- Use JSON contracts (Python standard library; no YAML dependency).
- Keep current asset paths intact. Audit legacy debt explicitly; do not silently
  exempt it from production approval or equate a passing tooling CI with asset approval.
- Store generated evidence in ignored `stw-o3de/Build/VisualForge/`; commit concise
  authored reports in `stw-o3de/Docs/Reports/`. Hash evidence and exported files.
- New source binaries and newly prefixed runtime binaries use scoped LFS rules.
  Existing binary history requires its own reviewed migration.
- Profiles and lighting recipes are declarative targets until a native adapter
  applies them and an exact-revision capture verifies effective settings.
- Native VFX consumes presentation events. No renderer may own damage, hit
  validation or authoritative smoke/visibility. Keep the backend replaceable.

## Engine facts checked

O3DE is pinned to 26.05.0 / `3db6943249d8bd7960b9ed7e9aee310b7668586e`.
The [official release notes](https://www.docs.o3de.org/docs/release-notes/2605-0-release-notes/)
confirm AgX and Khronos PBR Neutral, explicitly opt-in missing-LOD generation,
and an experimental Open Particle System discouraged for production.
These capabilities are not evidence that STW has enabled or validated them.

## Rollback

Before commit, remove only this task's added paths and reverse only its hunks
in shared files after checking the diff. After commit, revert the specific
Visual Forge commit. Never reset the branch to the baseline: it contains other work.
