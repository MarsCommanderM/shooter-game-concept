import assert from "node:assert/strict";
import { access, chmod, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";
import { afterEach, beforeEach, test } from "node:test";
import { fileURLToPath } from "node:url";

import {
  createPlaytestBridge,
  loadBridgeConfig,
  ROUTE_PREFIX,
} from "./bridge.mjs";

const token = "test-token-0000000000000000";
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
    "const test = args[args.indexOf('--playtest') + 1];",
    "const statusPath = path.join(directory, 'status.json');",
    "const framePath = path.join(directory, 'frame.png');",
    "let state = 'PLAYING';",
    "let sequence = 0;",
    "async function publish() {",
    "  await writeFile(statusPath, JSON.stringify({",
    "    playtest: test, state, fps: 60, frameTimeMs: 16.7,",
    "    animationTime: 1, clip: 'Fixture', jointCount: 2,",
    "    paletteSize: 2, looping: true, slowMotion: false, lastError: '',",
    "    tokenVisible: Boolean(process.env.STW_PLAYTEST_TOKEN)",
    "  }));",
    "}",
    "await publish();",
    "await writeFile(framePath, 'FRAME-1');",
    "const timer = setInterval(async () => {",
    "  try {",
    "    const payload = await readFile(path.join(directory, 'command.txt'), 'utf8');",
    "    const parts = payload.trim().split(/\\s+/);",
    "    const nextSequence = Number(parts[0]);",
    "    if (!Number.isSafeInteger(nextSequence) || nextSequence <= sequence) return;",
    "    sequence = nextSequence;",
    "    if (parts[1] === 'stop') { clearInterval(timer); process.exit(0); }",
    "    if (parts[1] === 'pause') state = 'PAUSED';",
    "    if (parts[1] === 'play' || parts[1] === 'reset') state = 'PLAYING';",
    "    if (parts[1] === 'bind') state = 'BIND';",
    "    await publish();",
    "  } catch (error) { if (error.code !== 'ENOENT') throw error; }",
    "}, 10);",
    "process.on('SIGTERM', () => process.exit(0));",
  ].join("\n");
  await writeFile(executable, source, { mode: 0o700 });
  await chmod(executable, 0o700);
  return executable;
}

async function startBridge() {
  root = await mkdtemp(path.join(tmpdir(), "stw-bridge-test-"));
  const binaryPath = await makeFakeStw(root);
  const config = {
    binaryPath,
    display: ":test",
    engineDirectory: root,
    host: "127.0.0.1",
    initialScene: "animation",
    port: 0,
    routePrefix: ROUTE_PREFIX,
    stopTimeoutMilliseconds: 500,
    streamFramesPerSecond: 10,
    token,
    webDirectory: path.resolve(bridgeDirectory, "../web"),
  };
  bridge = createPlaytestBridge(config);
  const address = await bridge.listen();
  baseUrl = `http://127.0.0.1:${address.port}/stw-playtest`;
}

function headers(validToken = token) {
  return {
    Authorization: `Bearer ${validToken}`,
    "Content-Type": "application/json",
  };
}

async function post(endpoint, body, validToken = token) {
  return fetch(`${baseUrl}/api/${endpoint}`, {
    method: "POST",
    headers: headers(validToken),
    body: typeof body === "string" ? body : JSON.stringify(body),
  });
}

beforeEach(startBridge);

afterEach(async () => {
  await bridge?.close();
  if (root) await rm(root, { force: true, recursive: true });
  bridge = null;
  root = null;
});

test("configuration fails clearly when security or display inputs are missing", () => {
  assert.throws(() => loadBridgeConfig({}), /STW_PLAYTEST_TOKEN is required/);
  assert.throws(() => loadBridgeConfig({
    STW_PLAYTEST_TOKEN: token,
    STW_PLAYTEST_BINARY: "/tmp/stw",
    STW_PLAYTEST_SCENE: "animation",
  }), /no OpenGL display configured/);
  assert.throws(() => loadBridgeConfig({
    DISPLAY: ":99",
    STW_PLAYTEST_TOKEN: token,
    STW_PLAYTEST_BINARY: "/tmp/stw",
    STW_PLAYTEST_SCENE: "unknown",
  }), /must be skinning or animation/);

  const config = loadBridgeConfig({
    STW_PLAYTEST_BINARY: "/tmp/stw",
    STW_PLAYTEST_DISPLAY: ":123",
    STW_PLAYTEST_HOST: "127.0.0.1",
    STW_PLAYTEST_PORT: "9123",
    STW_PLAYTEST_SCENE: "skinning",
    STW_PLAYTEST_STREAM_FPS: "10",
    STW_PLAYTEST_TOKEN: token,
  });
  assert.equal(config.binaryPath, "/tmp/stw");
  assert.equal(config.display, ":123");
  assert.equal(config.host, "127.0.0.1");
  assert.equal(config.port, 9123);
  assert.equal(config.initialScene, "skinning");
  assert.equal(config.streamFramesPerSecond, 10);
});

test("API rejects missing and invalid auth while valid auth can read status", async () => {
  let response = await fetch(`${baseUrl}/api/status`);
  assert.equal(response.status, 401);
  response = await fetch(`${baseUrl}/api/status`, {
    headers: { Authorization: "Bearer wrong-token-value" },
  });
  assert.equal(response.status, 401);
  response = await fetch(`${baseUrl}/api/status`, {
    headers: { Authorization: `Bearer ${token}` },
  });
  assert.equal(response.status, 200);
  assert.equal((await response.json()).bridge.state, "idle");
});

test("only fixed controls are accepted and malformed messages are rejected", async () => {
  assert.equal((await post("start", { test: "animation" })).status, 202);

  let statusResponse;
  for (let attempt = 0; attempt < 100; ++attempt) {
    statusResponse = await fetch(`${baseUrl}/api/status`, {
      headers: { Authorization: `Bearer ${token}` },
    });
    if ((await statusResponse.clone().json()).engine) break;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  assert.equal((await statusResponse.json()).engine.tokenVisible, false);

  for (const command of ["play", "pause", "reset", "bind", "slow"]) {
    assert.equal((await post("command", { command })).status, 202);
  }
  assert.equal((await post("command", {
    command: "camera",
    values: [-1, 0.25, 1, -0.5],
  })).status, 202);

  let response = await post("command", { command: "exec", values: ["id"] });
  assert.equal(response.status, 400);
  response = await post("command", {
    command: "camera",
    values: [0, 0, 0, 2],
  });
  assert.equal(response.status, 400);
  response = await post("command", '{"command":');
  assert.equal(response.status, 400);
  response = await fetch(`${baseUrl}/api/command`, {
    method: "POST",
    headers: { Authorization: `Bearer ${token}` },
    body: JSON.stringify({ command: "pause" }),
  });
  assert.equal(response.status, 415);
});

test("browser input cannot inject executable arguments or filesystem paths", async () => {
  let response = await post("start", {
    test: "animation",
    binary: "/bin/sh",
  });
  assert.equal(response.status, 400);
  response = await post("start", {
    test: "../../etc/passwd",
  });
  assert.equal(response.status, 400);
  response = await fetch(`${baseUrl}/../../etc/passwd`, {
    headers: { Authorization: `Bearer ${token}` },
  });
  assert.equal(response.status, 404);
});

test("frame endpoint serves only the newest native frame and supports ETags", async () => {
  assert.equal((await post("start", { test: "skinning" })).status, 202);
  const framePath = path.join(bridge.supervisor.sessionDirectory, "frame.png");
  for (let attempt = 0; attempt < 100; ++attempt) {
    try {
      await access(framePath);
      break;
    } catch {
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
  }

  let response = await fetch(`${baseUrl}/api/frame`, {
    headers: { Authorization: `Bearer ${token}` },
  });
  assert.equal(response.status, 200);
  assert.equal(await response.text(), "FRAME-1");
  const firstEtag = response.headers.get("etag");
  assert.ok(firstEtag);

  response = await fetch(`${baseUrl}/api/frame`, {
    headers: {
      Authorization: `Bearer ${token}`,
      "If-None-Match": firstEtag,
    },
  });
  assert.equal(response.status, 304);

  await new Promise((resolve) => setTimeout(resolve, 5));
  await writeFile(framePath, "FRAME-2-newest");
  response = await fetch(`${baseUrl}/api/frame`, {
    headers: {
      Authorization: `Bearer ${token}`,
      "If-None-Match": firstEtag,
    },
  });
  assert.equal(response.status, 200);
  assert.equal(await response.text(), "FRAME-2-newest");
});

test("child crash is visible and restart creates a clean connected state", async () => {
  assert.equal((await post("start", { test: "animation" })).status, 202);
  bridge.supervisor.child.kill("SIGKILL");
  for (let attempt = 0; attempt < 100 && bridge.supervisor.state !== "error"; ++attempt) {
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  assert.equal(bridge.supervisor.state, "error");

  const response = await post("restart", { test: "skinning" });
  assert.equal(response.status, 202);
  assert.equal(bridge.supervisor.state, "running");
  assert.equal(bridge.supervisor.test, "skinning");
});

test("bridge shutdown stops its owned child and removes its private session", async () => {
  assert.equal((await post("start", { test: "animation" })).status, 202);
  const child = bridge.supervisor.child;
  const sessionDirectory = bridge.supervisor.sessionDirectory;
  assert.ok(child.pid > 0);

  await bridge.close();
  await assert.rejects(access(sessionDirectory));
  assert.notEqual(child.exitCode, null);
});
