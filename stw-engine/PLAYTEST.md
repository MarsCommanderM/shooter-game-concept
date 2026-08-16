# STW Playtest V2 — real game boot

`/stw-playtest/` is the primary manual acceptance experience. It displays PNG
readbacks from the real native STW OpenGL backbuffer and sends a fixed set of
typed inputs to that same process. The browser contains no renderer, gameplay,
map, weapon logic, or animation substitute.

Both normal native startup and `--playtest game` enter `RunGameRuntime`. The
runtime owns the menu state, training map, `FpsController`, `WeaponSystem`,
`TargetWorld`, glTF loading, and `GLRenderer`. The bridge starts the runtime at
its native menu and reconnects to it after browser refresh.

## Native modes

```sh
./stw
./stw --playtest list
./stw --playtest game
./stw --playtest game --frames 120 --no-input --auto-start
./stw --playtest game --frames 120 --no-input --auto-start \
  --capture /tmp/stw-game.png
```

Desktop game controls:

- `Enter`, `Space`, or click: enter the map from the menu
- `WASD`: move
- mouse: look
- left mouse: fire
- `Shift`: sprint
- `Q`: cycle weapon
- `R`: reset player, weapon, and targets
- `P`: pause/resume
- `Esc`: quit

The existing technical regression scenes remain available:

```sh
./stw --playtest skinning --frames 120 --no-input
./stw --playtest animation --frames 120 --no-input
```

They continue to exercise T3-A GPU skinning and the T3-B path:

```text
glTF -> StwAnimation -> AnimationClip -> Animator
     -> SkinningPalette -> GLRenderer
```

## Browser behavior

The default page is a full-viewport remote display for `--playtest game`.
There is no in-page token field. It supports touch move/look, held fire and
sprint, weapon, pause, reset, portrait/landscape safe areas, and desktop
keyboard/mouse input. A compact optional HUD reports connection, native scene,
native/received FPS, weapon, and the last native or bridge error.

The fixed public API accepts only:

```text
POST /stw-playtest/api/connect   {}
POST /stw-playtest/api/input     {strafe,forward,lookX,lookY,fire,sprint}
POST /stw-playtest/api/action    {action:start|pause|reset|weapon}
GET  /stw-playtest/api/status
GET  /stw-playtest/api/frame
```

All numeric input must be finite and within `[-1, 1]`; booleans must be real
JSON booleans. There is no executable, path, argument, shell, or general
command field. The native process is spawned with `shell: false`. Frames use a
single atomically replaced `frame.png`, so delivery is newest-frame-wins with
no unbounded queue.

The secondary `/stw-playtest/debug/` page can switch `game`, `skinning`, and
`animation`. Its mutation/status endpoints require the optional
`STW_PLAYTEST_DEBUG_TOKEN` (16+ bytes). That token is never passed to STW and
is not needed by the default game page.

## One-time VPS deployment

The following commands assume the verified checkout is `/opt/wirrwar` and a
Debian/Ubuntu-style host. Inspect commands before running them. Package
installation is only needed if the corresponding check fails; the previously
verified STW VPS already has these dependencies.

### 1. Verify prerequisites and build

```sh
command -v cmake c++ node xvfb-run glxinfo
pkg-config --modversion sdl2

cd /opt/wirrwar
git fetch origin
git switch codex/stw-playtest-v2-real-game-boot
git pull --ff-only origin codex/stw-playtest-v2-real-game-boot

id stw-playtest >/dev/null 2>&1 || sudo useradd --system \
  --home-dir /var/lib/stw-playtest --create-home \
  --shell /usr/sbin/nologin stw-playtest
sudo install -d -o stw-playtest -g stw-playtest /var/lib/stw-playtest/build
sudo -u stw-playtest cmake -S /opt/wirrwar/stw-engine \
  -B /var/lib/stw-playtest/build -DCMAKE_BUILD_TYPE=Release
sudo -u stw-playtest cmake --build /var/lib/stw-playtest/build \
  --target stw cache_builder -j2

node --test stw-engine/playtest/bridge/bridge.test.mjs
```

Only on an otherwise unprepared Debian/Ubuntu host, the corresponding package
set is typically:

```sh
sudo apt-get update
sudo apt-get install cmake g++ libsdl2-dev libglm-dev libgl1-mesa-dev \
  mesa-utils xvfb nodejs
```

Do not run package commands merely because they are listed here.

### 2. Verify the real runtime before service setup

```sh
cd /opt/wirrwar/stw-engine
xvfb-run -a -s "-screen 0 1280x720x24 +extension GLX +render -noreset" \
  /var/lib/stw-playtest/build/stw --playtest game \
  --frames 120 --no-input --auto-start --capture /tmp/stw-game.png
file /tmp/stw-game.png

xvfb-run -a -s "-screen 0 1280x720x24 +extension GLX +render -noreset" \
  /var/lib/stw-playtest/build/stw --playtest skinning --frames 120 --no-input
xvfb-run -a -s "-screen 0 1280x720x24 +extension GLX +render -noreset" \
  /var/lib/stw-playtest/build/stw --playtest animation --frames 120 --no-input
```

The capture must be a real PNG produced by `IRenderer::RequestFrameCapture`.

### 3. Install the persistent bridge service

The repository provides a reviewed example unit at
`playtest/deploy/stw-playtest-v2.service`. It runs loopback-only and uses
`xvfb-run`; it does not deploy or reload a reverse proxy.

```sh
sudo install -m 0600 \
  /opt/wirrwar/stw-engine/playtest/deploy/stw-playtest-v2.env.example \
  /etc/stw-playtest-v2.env
sudo install -m 0644 \
  /opt/wirrwar/stw-engine/playtest/deploy/stw-playtest-v2.service \
  /etc/systemd/system/stw-playtest-v2.service

sudo systemctl daemon-reload
sudo systemctl enable --now stw-playtest-v2.service
sudo systemctl status --no-pager stw-playtest-v2.service
```

To enable the secondary technical debug page, add a secret generated outside
the repository to `/etc/stw-playtest-v2.env`, then restart only this service:

```sh
read -rsp "STW debug token (16+ bytes): " STW_DEBUG_TOKEN; printf '\n'
printf 'STW_PLAYTEST_DEBUG_TOKEN=%s\n' "$STW_DEBUG_TOKEN" \
  | sudo tee -a /etc/stw-playtest-v2.env >/dev/null
unset STW_DEBUG_TOKEN
sudo systemctl restart stw-playtest-v2.service
```

The default game page does not request or expose this token.

### 4. Verify locally with curl

```sh
curl -fsS http://127.0.0.1:8791/stw-playtest/ >/dev/null
curl -fsS -X POST -H 'Content-Type: application/json' -d '{}' \
  http://127.0.0.1:8791/stw-playtest/api/connect
curl -fsS http://127.0.0.1:8791/stw-playtest/api/status \
  | python3 -m json.tool
curl -fsS -o /tmp/stw-live-frame.png \
  http://127.0.0.1:8791/stw-playtest/api/frame
file /tmp/stw-live-frame.png

curl -fsS -X POST -H 'Content-Type: application/json' \
  -d '{"action":"start"}' \
  http://127.0.0.1:8791/stw-playtest/api/action
```

Status must report `test: game`, runtime `STW native OpenGL`, and scene
`main-menu` or `training-map`.

### 5. Reverse-proxy contract

This repository does not prove whether nginx, Apache, Caddy, or another proxy
owns the public site, so it deliberately does not edit or restart one. Inspect
the actual VPS configuration first. The proxy must:

1. forward public HTTPS `/stw-playtest/` to `http://127.0.0.1:8791`;
2. retain the complete `/stw-playtest/` prefix;
3. forward GET and POST;
4. disable response buffering/caching for `/api/frame` and `/api/status`;
5. keep port `8791` loopback-only;
6. apply the site's existing authenticated session, VPN, or IP allowlist if
   the route must not be generally reachable; this must not require an in-page
   STW token field;
7. pass its native configuration check before any reload.

No WebSocket configuration is required. Once the real proxy is configured and
the service is enabled, normal playtesting requires no terminal command.

### 6. Open and stop

Open on iPhone or desktop:

```text
https://<actual-server-host>/stw-playtest/
```

The page connects automatically. Press `DEPLOY` to leave the native menu.
Refreshing reuses a healthy game child; after a crash, the next connect starts
a clean game child.

Stop only the dedicated service when intentionally taking this harness down:

```sh
sudo systemctl stop stw-playtest-v2.service
```

Do not kill unrelated `stw`, `Xvfb`, NOVA, or production service processes.

## Troubleshooting

```sh
sudo systemctl status --no-pager stw-playtest-v2.service
sudo journalctl -u stw-playtest-v2.service -n 100 --no-pager
curl -i http://127.0.0.1:8791/stw-playtest/api/status
xvfb-run -a glxinfo -B
```

- `DISCONNECTED`: inspect the dedicated unit and public proxy route.
- HTTP 404 frame while starting: wait for the first native readback.
- bridge `error`: inspect its redacted error/status and unit journal.
- local curl succeeds but HTTPS fails: fix the real proxy/TLS route; do not add
  a browser renderer.
- frozen controls: status should still update; input expires natively after
  350 ms so a disconnected browser cannot leave movement/fire held forever.

## Future milestone acceptance contract

Every future visual/runtime milestone must leave this real-game URL working
and provide:

1. automated tests,
2. a real build,
3. a manual in-game scenario,
4. documented expected behavior,
5. desktop controls,
6. mobile controls where relevant, and
7. regression runs for `skinning`, `animation`, and the real game boot.

A milestone is not manually accepted until the user has played it and reported
how it looks and feels.
