# STW Playtest V1

This is the manual acceptance path for STW visual/runtime work. It does not
contain a browser renderer. Desktop and mobile modes execute the same STW C++
runtime, `GLRenderer`, GPU skinning shader, animation system, and glTF loader.
The browser only displays PNG readbacks from the native OpenGL backbuffer and
sends a small fixed control set.

## Desktop and smoke modes

From a configured build directory:

```sh
./stw --playtest list
./stw --playtest skinning
./stw --playtest animation --duration 10
./stw --playtest skinning --frames 120 --no-input
./stw --playtest animation --frames 120 --no-input --capture /tmp/stw-animation.png
```

`--no-input` advances by a deterministic 1/60 second per frame. A failed SDL,
OpenGL, import, palette, draw, or requested-capture operation returns non-zero.
`--capture` reads the real OpenGL backbuffer before swap.

Desktop controls:

- `WASD`: move camera
- mouse: look
- `Space`: pause/resume
- `R`: restart animation
- `B` or `1`: bind pose
- `2`: animated pose from the beginning
- `L`: toggle slow motion
- `Esc`: exit

The native status line contains test, FPS, frame time, animation time, clip,
joint count, palette size, playback state, and last error.

## Included acceptance scenes

`skinning` builds a deterministic two-joint ribbon and exercises the existing
Animator, SkinningPalette, and `IRenderer::DrawWithSkinning` path.

`animation` loads `playtest/assets/imported_animation.gltf` through the real
pipeline:

```text
glTF -> StwAnimation -> per-skin AnimationClip -> BindAnimation
     -> Animator -> SkinningPalette -> GLRenderer
```

No final `AnimationClip` is manually substituted in that scene.

## Mobile bridge contract

The bridge uses only Node's standard library. It starts exactly one configured
STW executable with `shell: false`, fixed native arguments, a fixed working
directory, and no access token in the child environment. Browser input cannot
provide an executable, filesystem path, command-line argument, or shell text.

Allowed scenes:

```text
skinning animation
```

Allowed controls:

```text
play pause reset bind slow stop camera
```

The engine atomically replaces one `frame.png` at the configured rate. The
HTTP endpoint always reads that fixed file, uses ETags to suppress duplicate
frames, and has no frame queue. Browser polling is single-flight and defaults
to approximately 10 received frames per second.

On `SIGINT` or `SIGTERM`, the bridge writes the native `stop` command and
waits for normal exit. It then uses `SIGTERM` and finally `SIGKILL` only for
its own STW child if required. Its private temporary session is removed.
Running the bridge under `xvfb-run` gives the Xvfb wrapper ownership of the
matching X server, so wrapper shutdown also cleans up that Xvfb process.

### Required environment

- `STW_PLAYTEST_BINARY`: required absolute path to the built `stw`
- `STW_PLAYTEST_SCENE`: required, `skinning` or `animation`
- `STW_PLAYTEST_TOKEN`: required, at least 16 bytes; never committed or logged
- `STW_PLAYTEST_HOST`: optional, defaults to `127.0.0.1`
- `STW_PLAYTEST_PORT`: optional, defaults to `8791`
- `STW_PLAYTEST_STREAM_FPS`: optional integer 1..30, defaults to `10`
- `STW_PLAYTEST_DISPLAY`: optional explicit display; otherwise `DISPLAY`
  must exist (as it does when launched with `xvfb-run`)

Missing binary, scene, token, or display configuration is a startup error.
Loopback binding is strongly recommended; remote access belongs behind an
authenticated TLS reverse proxy.

## Exact VPS runbook

These commands assume the verified checkout is `/opt/wirrwar`. They use a
separate build directory and do not modify a production build or service.

### A. Configure and build

```sh
cd /opt/wirrwar
git fetch origin
git switch codex/stw-playtest-mobile-deploy
git pull --ff-only origin codex/stw-playtest-mobile-deploy

node --version
command -v xvfb-run
cmake -S stw-engine \
  -B /tmp/stw-playtest-mobile-build \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/stw-playtest-mobile-build --target stw cache_builder -j2

node --test stw-engine/playtest/bridge/bridge.test.mjs
```

The bridge/self-test requires an already installed Node.js 18 or newer. Do not
install or replace Node as part of this runbook; report it as a VPS prerequisite
if the command is unavailable or older.

### B. Start the imported-animation bridge

First configure the token as described in section D, then keep this command in
the foreground:

```sh
cd /opt/wirrwar/stw-engine
export STW_PLAYTEST_BINARY=/tmp/stw-playtest-mobile-build/stw
export STW_PLAYTEST_SCENE=animation
export STW_PLAYTEST_HOST=127.0.0.1
export STW_PLAYTEST_PORT=8791
export STW_PLAYTEST_STREAM_FPS=10

xvfb-run -a \
  -s "-screen 0 1280x720x24 +extension GLX +render -noreset" \
  node playtest/bridge/server.mjs
```

### C. Start the GPU-skinning bridge

Use the same command with only the scene changed:

```sh
cd /opt/wirrwar/stw-engine
export STW_PLAYTEST_BINARY=/tmp/stw-playtest-mobile-build/stw
export STW_PLAYTEST_SCENE=skinning
export STW_PLAYTEST_HOST=127.0.0.1
export STW_PLAYTEST_PORT=8791
export STW_PLAYTEST_STREAM_FPS=10

xvfb-run -a \
  -s "-screen 0 1280x720x24 +extension GLX +render -noreset" \
  node playtest/bridge/server.mjs
```

If an administrator already owns a verified display, do not start a second
Xvfb. Set it explicitly and run Node directly:

```sh
export STW_PLAYTEST_DISPLAY=:99
node playtest/bridge/server.mjs
```

### D. Configure the token

Set the token in the same terminal before B or C. `read -s` keeps it out of
terminal echo and normal shell history:

```sh
read -rsp "STW playtest token (16+ bytes): " STW_PLAYTEST_TOKEN
printf '\n'
export STW_PLAYTEST_TOKEN
test "${#STW_PLAYTEST_TOKEN}" -ge 16
```

Enter that same value in the web page. The page stores it in
`sessionStorage`, not in its URL or committed source.

### E. Verify locally with curl

Use a second VPS terminal while the bridge is running:

```sh
curl -i http://127.0.0.1:8791/stw-playtest/api/status
# Expected: HTTP 401 without a token.

curl -fsS \
  -H "Authorization: Bearer $STW_PLAYTEST_TOKEN" \
  http://127.0.0.1:8791/stw-playtest/api/status \
  | python3 -m json.tool

curl -fsS \
  -H "Authorization: Bearer $STW_PLAYTEST_TOKEN" \
  -o /tmp/stw-playtest-frame.png \
  http://127.0.0.1:8791/stw-playtest/api/frame
file /tmp/stw-playtest-frame.png
```

The status must show bridge state `running`, the chosen native scene, and
native engine data. The frame must identify as PNG; it comes from the native
`FrameCapture` path.

### F. Reverse proxy `/stw-playtest/`

This repository contains no verified nginx, Apache, Caddy, systemd, firewall,
or production proxy configuration, so it deliberately does not invent or edit
one. Before changing the VPS, the administrator must inspect which proxy is
actually active.

The required proxy contract is:

1. public HTTPS path `/stw-playtest/` forwards to
   `http://127.0.0.1:8791`;
2. the complete `/stw-playtest/` prefix is retained;
3. GET and POST plus the `Authorization` request header are forwarded;
4. buffering/caching is disabled for `/api/frame` and `/api/status`;
5. request bodies remain bounded (the bridge itself rejects more than 4096
   bytes);
6. no WebSocket configuration is needed;
7. the upstream port remains loopback-only and is not opened directly;
8. existing site authentication/IP policy may add another access layer.

After the real proxy configuration is identified, validate the route from a
desktop browser before using the iPhone. Do not restart or reload a proxy until
its native configuration check succeeds.

### G. Open from iPhone or desktop

Open:

```text
https://<actual-server-host>/stw-playtest/
```

Enter the token, confirm `CONNECTED · RUNNING`, select a scene, and use
`START / SWITCH` only when changing/recovering the scene. The overlay reports
native state, native FPS, received-frame FPS, clip, time, loop/slow state,
joint/palette counts, and the last bridge/runtime error.

The viewer disables scroll/zoom gestures only over its interaction surface.
Movement and look pads work in portrait and landscape. Controls are
pause, reset, bind pose, play/animated pose, and slow motion.

### H. Stop and clean up

Press `Ctrl-C` in the foreground bridge terminal. Wait for:

```text
STW playtest bridge received SIGINT; stopping owned runtime
```

The bridge then stops its STW child and `xvfb-run` cleans up its owned Xvfb.
For a bridge started by an external supervisor, send `SIGTERM` to the bridge
process and allow the same cleanup path to finish. Do not kill all `stw` or
`Xvfb` processes globally.

### I. Troubleshoot blank frame or disconnected bridge

Check in this order:

```sh
curl -i http://127.0.0.1:8791/stw-playtest/
curl -fsS \
  -H "Authorization: Bearer $STW_PLAYTEST_TOKEN" \
  http://127.0.0.1:8791/stw-playtest/api/status \
  | python3 -m json.tool
curl -i \
  -H "Authorization: Bearer $STW_PLAYTEST_TOKEN" \
  http://127.0.0.1:8791/stw-playtest/api/frame

xvfb-run -a glxinfo -B
pgrep -a -f '/tmp/stw-playtest-mobile-build/stw --playtest'
```

- `DISCONNECTED`: bridge URL/proxy/token route is not reachable; inspect the
  bridge terminal and actual proxy logs.
- bridge `error`: read the redacted `bridge.error` status and native stderr
  tail; press `START / SWITCH` to create a clean child after fixing it.
- HTTP 404 frame with bridge `running`: native capture has not produced its
  first frame; verify the display with `glxinfo -B` under the same Xvfb mode.
- native status without changing frames: verify `STW_PLAYTEST_STREAM_FPS` is
  1..30 and inspect `lastError`.
- local curl works but the public URL fails: the issue is the real reverse
  proxy/TLS path, not a reason to add a browser renderer.

## Security and bridge self-test

The API requires a constant-time compared bearer token. Static HTML can load
without the token so the user can enter it. There is no CORS grant, no shell,
no arbitrary command endpoint, and no browser-selected path. Diagnostics are
bounded and credential-redacted.

Run the GPU-independent test suite with:

```sh
node --test stw-engine/playtest/bridge/bridge.test.mjs
```

It covers auth rejection/acceptance, malformed requests, command allowlisting,
path/argument injection rejection, newest-frame ETags, child crash/restart, and
owned-child/session cleanup. It does not claim GPU or mobile visual success.

## Future milestone acceptance contract

Every future visual/runtime milestone must provide:

1. automated tests,
2. a real build,
3. a manual playtest scenario,
4. documented expected behavior,
5. desktop controls,
6. mobile controls where relevant, and
7. a regression scenario for previous playtests.

A milestone is **not manually accepted** until the user has run the applicable
playtest and recorded the result.
