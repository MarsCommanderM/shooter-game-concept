import assert from "node:assert/strict";
import { access, chmod, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { afterEach, beforeEach, test } from "node:test";
import { fileURLToPath } from "node:url";

import { createPlaytestBridge, loadBridgeConfig, ROUTE_PREFIX } from "./bridge.mjs";

const debugToken = "debug-token-0000000000000000";
const bridgeDirectory = path.dirname(fileURLToPath(import.meta.url));
let bridge;
let root;
let baseUrl;

async function makeFakeStw(directory) {
  const executable = path.join(directory, "fake-stw.mjs");
  const source = [
    "#!/usr/bin/env node",
    "import { readFile, writeFile } from 'node:fs/promises';",
    "import path from 'node:path';",
    "const args = process.argv.slice(2);",
    "const remoteIndex = args.indexOf('--remote-dir');",
    "if (remoteIndex < 0 || !args[remoteIndex + 1]) process.exit(64);",
    "const directory = args[remoteIndex + 1];",
    "const testName = args[args.indexOf('--playtest') + 1];",
    "let state = testName === 'game' ? 'MENU' : 'PLAYING';",
    "let commandSequence = 0; let inputSequence = 0;",
    "let lastCommand = ''; let lastInput = '';",
    "async function publish() {",
    "  await writeFile(path.join(directory, 'status.json'), JSON.stringify({",
    "    playtest: testName, runtime: 'fake-test-only', state, scene: state === 'MENU' ? 'main-menu' : 'training-map',",
    "    fps: 60, frameTimeMs: 16.7, weapon: 'M16', lastCommand, lastInput,",
    "    tokenVisible: Boolean(process.env.STW_PLAYTEST_DEBUG_TOKEN || process.env.STW_PLAYTEST_TOKEN)",
    "  }));",
    "}",
    "await publish(); await writeFile(path.join(directory, 'frame.png'), 'FRAME-1');",
    "const timer = setInterval(async () => {",
    "  try {",
    "    const parts = (await readFile(path.join(directory, 'command.txt'), 'utf8')).trim().split(/\\s+/);",
    "    const sequence = Number(parts[0]);",
    "    if (Number.isSafeInteger(sequence) && sequence > commandSequence) {",
    "      commandSequence = sequence; lastCommand = parts.slice(1).join(' ');",
    "      if (parts[1] === 'stop') { clearInterval(timer); process.exit(0); }",
    "      if (parts[1] === 'game_start') state = 'PLAYING';",
    "      if (parts[1] === 'game_pause') state = state === 'PAUSED' ? 'PLAYING' : 'PAUSED';",
    "      if (parts[1] === 'play' || parts[1] === 'reset') state = 'PLAYING';",
    "      if (parts[1] === 'pause') state = 'PAUSED';",
    "      if (parts[1] === 'bind') state = 'BIND';",
    "      await publish();",
    "    }",
    "  } catch (error) { if (error.code !== 'ENOENT') throw error; }",
    "  try {",
    "    const parts = (await readFile(path.join(directory, 'input.txt'), 'utf8')).trim().split(/\\s+/);",
    "    const sequence = Number(parts[0]);",
    "    if (Number.isSafeInteger(sequence) && sequence > inputSequence) {",
    "      inputSequence = sequence; lastInput = parts.slice(1).join(' '); await publish();",
    "    }",
    "  } catch (error) { if (error.code !== 'ENOENT') throw error; }",
    "}, 8);",
    "process.on('SIGTERM', () => process.exit(0));",
  ].join("\n");
  await writeFile(executable, source, { mode: 0o700 });
  await chmod(executable, 0o700);
  return executable;
}

async function startBridge() {
  root = await mkdtemp(path.join(tmpdir(), "stw-bridge-v2-test-"));
  const binaryPath = await makeFakeStw(root);
  bridge = createPlaytestBridge({
    binaryPath,
    debugToken,
    display: ":test",
    engineDirectory: root,
    host: "127.0.0.1",
    initialScene: "game",
    port: 0,
    routePrefix: ROUTE_PREFIX,
    stopTimeoutMilliseconds: 500,
    streamFramesPerSecond: 10,
    webDirectory: path.resolve(bridgeDirectory, "../web"),
  });
  const address = await bridge.listen();
  baseUrl = `http://127.0.0.1:${address.port}/stw-playtest`;
}

async function post(pathname, body, headers = {}) {
  return fetch(`${baseUrl}/api${pathname}`, {
    method: "POST",
    headers: { "Content-Type": "application/json", ...headers },
    body: typeof body === "string" ? body : JSON.stringify(body),
  });
}

async function waitForEngine(predicate = () => true) {
  for (let attempt = 0; attempt < 100; ++attempt) {
    const response = await fetch(`${baseUrl}/api/status`);
    const payload = await response.json();
    if (payload.engine && predicate(payload.engine)) return payload.engine;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error("native status did not reach expected state");
}

beforeEach(startBridge);
afterEach(async () => {
  await bridge?.close();
  if (root) await rm(root, { force: true, recursive: true });
  bridge = null;
  root = null;
});

test("configuration defaults to game and fails clearly without binary or display", () => {
  assert.throws(() => loadBridgeConfig({}), /STW_PLAYTEST_BINARY is required/);
  assert.throws(() => loadBridgeConfig({ STW_PLAYTEST_BINARY: "/tmp/stw" }), /no OpenGL display/);
  const config = loadBridgeConfig({
    STW_PLAYTEST_BINARY: "/tmp/stw",
    STW_PLAYTEST_DISPLAY: ":123",
  });
  assert.equal(config.initialScene, "game");
  assert.equal(config.debugToken, "");
  assert.throws(() => loadBridgeConfig({
    STW_PLAYTEST_BINARY: "/tmp/stw",
    STW_PLAYTEST_DISPLAY: ":123",
    STW_PLAYTEST_DEBUG_TOKEN: "short",
  }), /at least 16 bytes/);
});

test("default page and real-game connect require no token", async () => {
  let response = await fetch(`${baseUrl}/`);
  assert.equal(response.status, 200);
  assert.doesNotMatch(await response.text(), /Session token|Playtest V1/);
  response = await post("/connect", {});
  assert.equal(response.status, 202);
  const engine = await waitForEngine();
  assert.equal(engine.playtest, "game");
  assert.equal(engine.state, "MENU");
  assert.equal(engine.tokenVisible, false);
  const reconnect = await post("/connect", {});
  assert.equal(reconnect.status, 202);
  assert.equal((await reconnect.json()).reused, true);
});

test("public game input and action accept only fixed typed messages", async () => {
  assert.equal((await post("/connect", {})).status, 202);
  assert.equal((await post("/action", { action: "start" })).status, 202);
  assert.equal((await post("/input", {
    strafe: -0.5, forward: 1, lookX: 0.25, lookY: -0.25,
    fire: true, sprint: false,
  })).status, 202);
  const engine = await waitForEngine((value) => value.lastInput !== "");
  assert.equal(engine.state, "PLAYING");
  assert.match(engine.lastInput, /^input -0.5 1 0.25 -0.25 1 0$/);
  assert.equal((await post("/action", { action: "reload" })).status, 202);
  const reloaded = await waitForEngine(
    (value) => value.lastCommand === "game_reload",
  );
  assert.equal(reloaded.lastCommand, "game_reload");

  let response = await post("/action", { action: "exec", command: "id" });
  assert.equal(response.status, 400);
  response = await post("/input", {
    strafe: 0, forward: 0, lookX: 2, lookY: 0, fire: false, sprint: false,
  });
  assert.equal(response.status, 400);
  response = await post("/input", { path: "../../etc/passwd" });
  assert.equal(response.status, 400);
  response = await post("/action", '{"action":');
  assert.equal(response.status, 400);
});

test("technical debug mutation stays bearer-token protected", async () => {
  let response = await post("/debug/restart", { test: "animation" });
  assert.equal(response.status, 401);
  response = await post("/debug/restart", { test: "animation" }, {
    Authorization: "Bearer wrong-token-value",
  });
  assert.equal(response.status, 401);
  response = await post("/debug/restart", { test: "animation" }, {
    Authorization: `Bearer ${debugToken}`,
  });
  assert.equal(response.status, 202);
  assert.equal(bridge.supervisor.test, "animation");

  response = await post("/debug/command", { command: "exec" }, {
    Authorization: `Bearer ${debugToken}`,
  });
  assert.equal(response.status, 400);
});

test("frame endpoint serves only newest native frame and supports ETags", async () => {
  assert.equal((await post("/connect", {})).status, 202);
  const framePath = path.join(bridge.supervisor.sessionDirectory, "frame.png");
  for (let attempt = 0; attempt < 100; ++attempt) {
    try { await access(framePath); break; }
    catch { await new Promise((resolve) => setTimeout(resolve, 10)); }
  }
  let response = await fetch(`${baseUrl}/api/frame`);
  assert.equal(response.status, 200);
  assert.equal(await response.text(), "FRAME-1");
  const firstEtag = response.headers.get("etag");
  response = await fetch(`${baseUrl}/api/frame`, { headers: { "If-None-Match": firstEtag } });
  assert.equal(response.status, 304);

  await new Promise((resolve) => setTimeout(resolve, 5));
  await writeFile(framePath, "FRAME-2-newest");
  response = await fetch(`${baseUrl}/api/frame`, { headers: { "If-None-Match": firstEtag } });
  assert.equal(response.status, 200);
  assert.equal(await response.text(), "FRAME-2-newest");
});

test("crashed child is visible and public reconnect starts a clean game", async () => {
  assert.equal((await post("/connect", {})).status, 202);
  bridge.supervisor.child.kill("SIGKILL");
  for (let attempt = 0; attempt < 100 && bridge.supervisor.state !== "error"; ++attempt) {
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  assert.equal(bridge.supervisor.state, "error");
  assert.equal((await post("/connect", {})).status, 202);
  assert.equal(bridge.supervisor.state, "running");
  assert.equal(bridge.supervisor.test, "game");
});

test("paths cannot traverse static allowlist", async () => {
  const response = await fetch(`${baseUrl}/../../etc/passwd`);
  assert.equal(response.status, 404);
});

test("bridge shutdown stops owned child and removes private session", async () => {
  assert.equal((await post("/connect", {})).status, 202);
  const child = bridge.supervisor.child;
  const sessionDirectory = bridge.supervisor.sessionDirectory;
  await bridge.close();
  await assert.rejects(access(sessionDirectory));
  assert.notEqual(child.exitCode, null);
});
