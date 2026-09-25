# STW native O3DE production

> ## PRODUCTION QUALITY BAR: REALISTIC CINEMATIC. NOT BLOCKOUT. NOT A PROTOTYPE.
>
> Everything under this directory targets a hyper-realistic, cinematic AAA
> presentation. Over 100 prototypes of this game already exist — this is
> not another one. Blockout geometry, placeholder materials, and debug
> visuals are development tools only; never treat them, describe them, or
> ship them as a finished result. See
> [`../docs/STW_PRODUCT_VISION.md`](../docs/STW_PRODUCT_VISION.md) and
> [Project Visual Forge](Docs/README.md) before any asset, level, or
> rendering decision.

`stw-o3de/` is the authoritative game root for current STW development.

## Pinned engine

O3DE 26.05.0 is pinned to
`3db6943249d8bd7960b9ed7e9aee310b7668586e`. See
[`O3DE_VERSION.md`](O3DE_VERSION.md).

## Tracked ownership

- `Gems/STWGameplay/`: native gameplay code and deterministic tests
- `Project/Assets/`: repository-owned O3DE source assets
- `Scripts/`: host verification utilities

The Lightning workflow mirrors those tracked inputs into the persistent,
pinned O3DE worktree, builds the affected native targets, runs deterministic
tests, launches the real game, validates Atom/Vulkan/PhysX/runtime markers, and
records evidence.

## Recovery boundary

The remote baseline before cleanup includes the bodycam foundation but does not
contain the reported weekend multiplayer/server/ragdoll package. Issue #5 must
recover that work before it can be reviewed or integrated.

NOVA, the browser prototype, the legacy Node relay, the custom OpenGL engine,
and Unity starter are not production dependencies of `STWGameplay`.

The superseded initial migration gate is retained at
[`../docs/history/O3DE_MIGRATION_GATE_2026-08-22.md`](../docs/history/O3DE_MIGRATION_GATE_2026-08-22.md).

## Project Visual Forge

Visual production standards, reproducible targets and evidence gates:
[Project Visual Forge](Docs/README.md). Start there before changing visual assets,
lighting, materials or quality settings.
