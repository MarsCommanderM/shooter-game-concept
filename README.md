# SAVE THE WORLD (STW)

> ## PRODUCTION QUALITY BAR: REALISTIC CINEMATIC. NOT BLOCKOUT. NOT A PROTOTYPE.
>
> Over 100 prototypes of this game already exist. This repository is the
> production line, not another one. Blockout-tier, placeholder-tier, or
> "tech demo"-tier quality is never the target, never final, and never
> shipped — anywhere in this repository, on any branch, in any session.

This repository contains the native STW game and historical prototypes.

STW is a high-end native 3D multiplayer FPS with a hyper-realistic cinematic
sci-fi target. Blockouts, debug geometry and placeholder content are development
tools, never the quality bar. Read
[`docs/STW_PRODUCT_VISION.md`](docs/STW_PRODUCT_VISION.md) before making any
architecture, asset or cleanup decision.

## Canonical production line

- Repository: `MarsCommanderM/shooter-game-concept`
- Production branch: `brauny/stw-game-production`
- Native engine: O3DE 26.05.0, pinned in
  [`stw-o3de/O3DE_VERSION.md`](stw-o3de/O3DE_VERSION.md)
- Gameplay owner: [`stw-o3de/Gems/STWGameplay/`](stw-o3de/Gems/STWGameplay/)
- Project source assets: [`stw-o3de/Project/Assets/`](stw-o3de/Project/Assets/)

Run `bash tools/stw-repo-guard.sh start` before changing source. The command
rejects the wrong repository, stale `main`, an unrelated branch, a stale
production checkout, or a dirty task start.

## Recovery status

The remote baseline captured before repository cleanup is
`6992c8040cd7f427b7f306e08ef0e18841f23c53`. It contains the native O3DE
player/combat/presentation foundation, including the bodycam foundation. The
separate weekend multiplayer/server/ragdoll package is not present in that
remote tree and must be recovered under
[Issue #5](https://github.com/MarsCommanderM/shooter-game-concept/issues/5)
before integration claims are made.

## Legacy boundary

NOVA has been removed from the active tree and preserved in
`archive/stw-pre-cleanup-6992c804`.

The browser/Next.js prototype, legacy Node relay, `stw-engine/`, and
`unity-starter/` were archived on 2026-09-18 after a dependency audit showed no
production reference. They are preserved at tag
`archive/legacy-web-prototype-20260918` and are not valid targets for new work.

See [`ENGINE_STACK.md`](ENGINE_STACK.md),
[`DEPLOY.md`](DEPLOY.md), and
[`docs/REPOSITORY_CONTRACT.md`](docs/REPOSITORY_CONTRACT.md).
