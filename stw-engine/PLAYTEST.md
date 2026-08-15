# STW Playtest V1

This harness is the manual acceptance path for STW visual/runtime work. It does
not contain a browser renderer. Both desktop and mobile modes execute the same
STW C++ runtime, `GLRenderer`, GPU skinning shader, animation system, and glTF
loader. Mobile Safari only displays PNG readbacks from that process and sends a
small fixed command set.

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
OpenGL, import, palette, draw, or requested-capture operation returns a non-zero
exit code. `--capture` reads the real OpenGL backbuffer before swap; it does not
render a substitute image.

Desktop controls:

- `WASD`: move camera
- mouse: look
- `Space`: pause/resume
- `R`: restart animation
- `B` or `1`: bind pose
- `2`: animated pose from the beginning
- `L`: toggle slow motion
- `Esc`: exit

The native V1 HUD is a once-per-status-window terminal line containing test,
FPS, frame time, animation time, clip, joint count, palette size, playback
state, and last error.

## Included acceptance scenes

`skinning` builds a deterministic two-joint ribbon. Vertices blend between both
joints around the bend, then the existing Animator, SkinningPalette, and
`IRenderer::DrawWithSkinning` path drive visible deformation. A tiny procedural
normal texture makes transformed normals/tangents observable.

`animation` loads `playtest/assets/imported_animation.gltf`. The fixture is a
small, source-controlled text glTF with a documented two-joint ribbon. Its path
is deliberately the production import chain:

```text
glTF -> StwAnimation -> per-skin AnimationClip -> BindAnimation
     -> Animator -> SkinningPalette -> GLRenderer
```

No final `AnimationClip` is constructed manually in that scene.

## Mobile bridge

The bridge uses Node's standard library only. It binds to localhost, launches a
fixed STW binary with `shell: false`, accepts only the known `skinning` and
`animation` tests, and maps browser input to this allowlist:

```text
play pause reset bind slow stop camera
```

It cannot accept a command line, executable, path, or shell text from the
browser. It never stops a process with a signal; `stop` is written to the STW
command file and the engine exits normally. One `frame.png` is atomically
replaced at up to 10 FPS, so there is no frame queue or growing backlog. When
the host already provides zlib, CMake enables fast PNG compression; a
self-contained standards-compliant PNG fallback remains available otherwise.

Remote APIs are disabled when `STW_PLAYTEST_TOKEN` is unset. Never commit the
token. Example manual startup on a render-capable host:

```sh
cd /absolute/path/to/stw-engine
STW_PLAYTEST_BINARY=/absolute/path/to/build/stw \
STW_PLAYTEST_TOKEN='set-this-outside-git' \
node playtest/bridge/server.mjs
```

The local page is then:

```text
http://127.0.0.1:8791/stw-playtest/
```

For an iPhone, an administrator must explicitly reverse-proxy the isolated
`/stw-playtest/` path to `127.0.0.1:8791`, retain the path prefix, and provide
TLS. This repository intentionally does not modify the production server,
proxy, firewall, systemd units, or deployment. Enter the token in the page; it
is kept in `sessionStorage`, not in the URL or persistent storage.

The page works in portrait and landscape, provides left movement and right look
touch areas, play/pause/reset/bind/slow buttons, runtime status, and local-only
manual PASS/FAIL checklists.

## OpenGL and headless requirement

`SDL_WINDOW_HIDDEN` hides a window; it does not invent an OpenGL context. The
host still needs one of the following already configured:

1. a usable display/context,
2. an SDL offscreen-capable driver for the installed OpenGL stack, or
3. an existing Xvfb/software-Mesa setup.

The harness does not install packages and never falls back to WebGL or Three.js.
If context creation fails, compilation can still pass, but mobile visual
playtesting is not ready on that host.

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
