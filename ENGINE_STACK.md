# STW canonical engine stack

This document records the engine stack. The product target it serves is defined
in [`docs/STW_PRODUCT_VISION.md`](docs/STW_PRODUCT_VISION.md): a high-end native
3D multiplayer FPS with a hyper-realistic cinematic sci-fi target.

## Authority

| Layer | Canonical implementation |
|---|---|
| Engine | O3DE 26.05.0 |
| Upstream pin | `3db6943249d8bd7960b9ed7e9aee310b7668586e` |
| Gameplay | `stw-o3de/Gems/STWGameplay/` |
| Physics | O3DE PhysX integration |
| Rendering | O3DE Atom with hardware Vulkan acceptance |
| Game assets | `stw-o3de/Project/Assets/` |
| Controlled build host | Lightning self-hosted GitHub Actions runner |

The `STWGameplay` Gem declares itself the authoritative native gameplay
module. Current tracked code contains player and enemy PhysX runtime adapters,
combat and encounter models, loadout/weapon behavior, presentation systems,
native skeletal character presentation, audio feedback, arena/environment
presentation, and a bodycam presentation foundation with deterministic tests.

## Pending recovery boundary

The GitHub baseline captured at
`6992c8040cd7f427b7f306e08ef0e18841f23c53` does not contain the reported
weekend multiplayer, native server, and ragdoll implementation. Those features
must be recovered from Lightning under Issue #5 and verified against their
exact recovered SHA before this document claims them as integrated.

## Retired and archived implementations

| Path | State | Rule |
|---|---|---|
| `nova/` | RETIRED; removed from active tree | Never restore as production |
| `app/`, `components/`, `server.mjs`, `stw-engine/`, `unity-starter/` | ARCHIVED 2026-09-18 at tag `archive/legacy-web-prototype-20260918` | Never restore into the active tree |

The historical document that described NOVA as production is preserved at
[`docs/history/ENGINE_STACK_NOVA_2026-08-11.md`](docs/history/ENGINE_STACK_NOVA_2026-08-11.md)
for provenance only.
