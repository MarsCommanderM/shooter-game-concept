# STW HQ native game acceptance

The only browser-facing manual acceptance experience is `/stw-hq/`. It shows
PNG readbacks from the real native STW OpenGL backbuffer and forwards a fixed,
typed input schema to the same process. The browser has no renderer, weapon
simulation, damage rules, bot AI, map logic, or animation state machine.

Both normal native startup and `--playtest game` enter `RunGameRuntime`. The
runtime owns the menu, HQ training arena, player controller, five-weapon
system, combat sandbox, bots, imported animation, skinning palettes, and native
renderer. Browser refresh reconnects to the healthy owned native process.

## Native commands

```sh
./stw
./stw --playtest list
./stw --playtest game --quality hq
./stw --playtest game --quality standard
./stw --playtest game --frames 120 --no-input --auto-start
./stw --playtest game --frames 120 --no-input --auto-start \
  --capture /tmp/stw-game.png
```

`--playtest list` intentionally exposes only `game`. The former procedural
ribbon/bending scenes are not alternate production runtimes. Their auditable
glTF data remains available to the registered C++ regression tests.

Desktop controls:

- `Enter`, `Space`, or click: enter the map
- `WASD`: move
- mouse: look
- left mouse: fire
- `Shift`: sprint
- `Q`: cycle weapon
- `E`: reload
- `R`: reset
- `P`: pause/resume
- `Esc`: quit

Mobile controls retain independent pointer ownership for move, look, fire, and
sprint. Weapon, reload, pause, and reset are fixed actions. Lost focus clears
held input to prevent stuck movement or firing.

## Public transport contract

The browser uses only these endpoints:

```text
POST /stw-hq/api/connect   {}
POST /stw-hq/api/input     {strafe,forward,lookX,lookY,fire,sprint}
POST /stw-hq/api/action    {action:start|pause|reset|weapon|reload}
GET  /stw-hq/api/status
GET  /stw-hq/api/frame
```

Numeric input must be finite and within `[-1, 1]`. Booleans must be JSON
booleans. Unexpected keys, paths, executable names, arguments, and arbitrary
commands are rejected. The bridge starts the fixed native binary with
`shell: false`. A single atomically replaced `frame.png` provides bounded,
newest-frame-wins delivery.

The public page never asks for or stores a token. Optional server-side
technical mutation endpoints under `/stw-hq/api/debug/` are disabled unless
`STW_PLAYTEST_DEBUG_TOKEN` is configured with at least 16 bytes. They retain a
strict command allowlist and have no browser token-entry page. The obsolete
`STW_PLAYTEST_TOKEN` fallback is intentionally unsupported.

## Build and repository checks

On a prepared host:

```sh
cd /opt/wirrwar
git switch brauny/stw-game-production
git pull --ff-only origin brauny/stw-game-production

cmake -S stw-engine -B /tmp/stw-game-production-build \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/stw-game-production-build --target \
  stw cache_builder stw_runtime_model_tests \
  stw_gameplay_animation_tests stw_gameplay_presentation_tests \
  stw_combat_loop_tests -j2
ctest --test-dir /tmp/stw-game-production-build --output-on-failure

node --check stw-engine/playtest/bridge/bridge.mjs
node --check stw-engine/playtest/bridge/server.mjs
node --check stw-engine/playtest/web/app.js
node --test stw-engine/playtest/bridge/bridge.test.mjs \
  stw-engine/playtest/web/mobile-input.test.mjs
```

No package installation is implied by these commands. The authoritative VPS
must already provide the native build and OpenGL runtime dependencies.

## Persistent bridge

The reviewed example files remain at:

- `playtest/deploy/stw-playtest-v2.env.example`
- `playtest/deploy/stw-playtest-v2.service`

Their historical filenames are retained to avoid an unnecessary service-file
migration. The process they start is the canonical native game bridge.

The minimum environment is:

```text
STW_PLAYTEST_BINARY=/absolute/path/to/stw
STW_PLAYTEST_HOST=127.0.0.1
STW_PLAYTEST_PORT=8791
STW_PLAYTEST_STREAM_FPS=10
```

An OpenGL display must be supplied through `STW_PLAYTEST_DISPLAY`, `DISPLAY`,
or the existing `xvfb-run` service wrapper. The bridge fails clearly when the
binary or display is missing.

## Local verification

With the bridge already running on loopback:

```sh
curl -fsS http://127.0.0.1:8791/stw-hq/ >/dev/null
curl -fsS -X POST -H 'Content-Type: application/json' -d '{}' \
  http://127.0.0.1:8791/stw-hq/api/connect
curl -fsS http://127.0.0.1:8791/stw-hq/api/status \
  | python3 -m json.tool
curl -fsS -o /tmp/stw-live-frame.png \
  http://127.0.0.1:8791/stw-hq/api/frame
file /tmp/stw-live-frame.png
```

Status must identify `test: game`; gameplay fields are read-only data emitted
by the native runtime. The frame must be a real PNG produced by the native
capture path.

## Reverse-proxy contract

The repository does not assume nginx, Apache, Caddy, systemd, or a particular
public hostname. Inspect the real VPS configuration before changing it. The
proxy must:

1. forward public HTTPS `/stw-hq/` to the loopback bridge;
2. retain the complete `/stw-hq/` prefix and forward GET and POST;
3. disable buffering/caching for `/api/frame` and `/api/status`;
4. keep the bridge port loopback-only;
5. preserve the existing server-side access policy without adding an in-page
   token flow;
6. pass the proxy's own configuration validation before reload.

The public acceptance URL is:

```text
https://<actual-server-host>/stw-hq/
```

## Milestone contract

Every future visual/runtime change must retain:

1. automated tests;
2. a real native build;
3. the normal `--playtest game --quality hq` scenario;
4. documented expected behavior and controls;
5. mobile and desktop acceptance through `/stw-hq/` where relevant;
6. regression coverage for previously accepted behavior.

Manual visual/game-feel acceptance belongs to the user and cannot be inferred
from unit tests or status JSON.
