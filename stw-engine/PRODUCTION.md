# STW production architecture

CANONICAL BRANCH: `brauny/stw-game-production`

CANONICAL GAME: native STW `GameRuntime`

CANONICAL PUBLIC TEST: `/stw-hq/`

WEB: transport/input only

BRIDGE: transport/process/security only

GAMEPLAY: native C++ only

ALL FUTURE GAME DEVELOPMENT: directly on `brauny/stw-game-production`

NO new milestone/prototype/visual-fix/green/combat branches.

## Ownership boundary

| Layer | Canonical responsibility | Must not own |
|---|---|---|
| Native `GameRuntime` and `src/game/` | menu, world, player, weapons, combat, bots, health, animation state, update order | browser/session policy |
| Native renderer | OpenGL rendering, materials, effects, skinning, frame capture | browser rendering or gameplay decisions |
| `playtest/web/` | touch/keyboard/mouse collection, fixed requests, native frame display, read-only HUD | gameplay simulation, weapon state, damage, bots, native animation |
| `playtest/bridge/` | fixed-schema validation, owned process lifecycle, frame/status transport, debug authorization | gameplay rules, arbitrary commands, browser-provided paths or arguments |

The executable started by the bridge is always:

```text
stw --playtest game
```

`--playtest game` calls the same native `RunGameRuntime` used by the game. HQ
and standard are presentation settings inside that runtime, not separate games.

## Current production state

The canonical native path includes the HQ arena and first-person weapon
presentation, five weapons with independent ammo/reload/switching, recoil,
muzzle/tracer/hit feedback, mobile multi-touch look, player health and
respawn, three deterministic native combat bots, LOS/perception/movement/fire,
player-to-bot damage, bot death/respawn, and per-instance imported animation
through `RuntimeModelInstance`, `Animator`, `SkinningPalette`, and
`DrawWithSkinning`.

The browser displays those native results. HUD values are read-only mirrors of
native status; changing HTML cannot change authoritative gameplay.

## Removed production alternatives

The procedural ribbon/bending `skinning` and imported-ribbon `animation`
render loops were early diagnostics. They are no longer compiled or registered
as alternate runtimes. Their source fixtures are not production characters.
Auditable glTF data still used by C++ regression tests remains in the repository.

The old browser debug/token-entry page and scene-switching path were removed.
Optional debug API operations remain server-side, bearer-protected, fixed, and
disabled when no debug token is configured. The public `/stw-hq/` page requires
no browser token.

## Forward-development rule

Historical branches remain immutable history. Do not branch forward from them
and do not create new `codex/` or milestone-named `brauny/stw-*` development
branches. Continue game work only on:

```text
brauny/stw-game-production
```

Deployment is a separate, explicit VPS operation. A repository commit or green
test suite does not imply deployment or mobile visual acceptance.
