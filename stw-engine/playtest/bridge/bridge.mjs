import { timingSafeEqual } from "node:crypto";
import { spawn as spawnChild } from "node:child_process";
import { constants as fsConstants } from "node:fs";
import {
  access, mkdtemp, readFile, rename, rm, stat, writeFile,
} from "node:fs/promises";
import { createServer as createHttpServer } from "node:http";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const bridgeDirectory = path.dirname(fileURLToPath(import.meta.url));
const defaultEngineDirectory = path.resolve(bridgeDirectory, "../..");
const defaultWebDirectory = path.resolve(bridgeDirectory, "../web");

export const ROUTE_PREFIX = "/stw-hq";
export const DEBUG_COMMANDS = Object.freeze([
  "play", "pause", "reset", "stop",
]);
export const GAME_ACTIONS = Object.freeze([
  "start", "pause", "reset", "weapon", "reload",
]);

const debugCommands = new Set(DEBUG_COMMANDS);
const gameActions = new Set(GAME_ACTIONS);
const childEnvironmentNames = Object.freeze([
  "DRI_PRIME", "GALLIUM_DRIVER", "HOME", "LANG", "LC_ALL", "LC_CTYPE",
  "LD_LIBRARY_PATH", "LIBGL_ALWAYS_SOFTWARE", "LIBGL_DRIVERS_PATH",
  "MESA_LOADER_DRIVER_OVERRIDE", "PATH", "SDL_AUDIODRIVER",
  "SDL_VIDEODRIVER", "TMPDIR", "XAUTHORITY", "XDG_RUNTIME_DIR",
  "__GLX_VENDOR_LIBRARY_NAME",
]);

class HttpError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

function requiredEnvironment(environment, name) {
  const value = environment[name];
  if (typeof value !== "string" || value.length === 0) {
    throw new Error(`${name} is required`);
  }
  return value;
}

function parseInteger(value, name, minimum, maximum) {
  if (!/^[0-9]+$/.test(value)) {
    throw new Error(`${name} must be an integer between ${minimum} and ${maximum}`);
  }
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed < minimum || parsed > maximum) {
    throw new Error(`${name} must be an integer between ${minimum} and ${maximum}`);
  }
  return parsed;
}

export function loadBridgeConfig(environment = process.env) {
  const binaryPath = requiredEnvironment(environment, "STW_PLAYTEST_BINARY");
  if (!path.isAbsolute(binaryPath)) {
    throw new Error("STW_PLAYTEST_BINARY must be an absolute path");
  }
  const initialScene = environment.STW_PLAYTEST_SCENE ?? "game";
  if (initialScene !== "game") {
    throw new Error("STW_PLAYTEST_SCENE must be game");
  }
  const debugToken = environment.STW_PLAYTEST_DEBUG_TOKEN ?? "";
  if (debugToken && Buffer.byteLength(debugToken, "utf8") < 16) {
    throw new Error("STW_PLAYTEST_DEBUG_TOKEN must contain at least 16 bytes");
  }
  const host = environment.STW_PLAYTEST_HOST ?? "127.0.0.1";
  if (!host || !/^[A-Za-z0-9_.:-]+$/.test(host)) {
    throw new Error("STW_PLAYTEST_HOST contains unsupported characters");
  }
  const port = parseInteger(environment.STW_PLAYTEST_PORT ?? "8791",
                            "STW_PLAYTEST_PORT", 1, 65535);
  const streamFramesPerSecond = parseInteger(
    environment.STW_PLAYTEST_STREAM_FPS ?? "10",
    "STW_PLAYTEST_STREAM_FPS", 1, 30,
  );
  const display = environment.STW_PLAYTEST_DISPLAY || environment.DISPLAY;
  if (!display) {
    throw new Error(
      "no OpenGL display configured; set STW_PLAYTEST_DISPLAY or run under xvfb-run",
    );
  }
  return Object.freeze({
    binaryPath,
    debugToken,
    display,
    engineDirectory: defaultEngineDirectory,
    host,
    initialScene,
    port,
    routePrefix: ROUTE_PREFIX,
    stopTimeoutMilliseconds: 3000,
    streamFramesPerSecond,
    webDirectory: defaultWebDirectory,
  });
}

function sanitizeDiagnostic(value) {
  return String(value)
    .replace(/:\/\/[^/\s:@]+:[^@\s/]+@/g, "://[REDACTED]@")
    .replace(/\b(token|password|secret)\s*[=:]\s*[^\s]+/gi, "$1=[REDACTED]")
    .replace(/[^\x09\x0a\x0d\x20-\x7e]/g, "?")
    .slice(-2048)
    .trim();
}

function buildChildEnvironment(display) {
  const environment = { DISPLAY: display };
  for (const name of childEnvironmentNames) {
    if (typeof process.env[name] === "string") environment[name] = process.env[name];
  }
  return environment;
}

function waitForExit(child, milliseconds) {
  if (child.exitCode !== null || child.signalCode !== null) return Promise.resolve(true);
  return new Promise((resolve) => {
    let completed = false;
    const finish = (exited) => {
      if (completed) return;
      completed = true;
      clearTimeout(timer);
      child.off("exit", onExit);
      resolve(exited);
    };
    const onExit = () => finish(true);
    const timer = setTimeout(() => finish(false), milliseconds);
    timer.unref?.();
    child.once("exit", onExit);
  });
}

function tokenMatches(configuredToken, candidate) {
  if (!configuredToken || typeof candidate !== "string") return false;
  const expected = Buffer.from(configuredToken);
  const received = Buffer.from(candidate);
  return expected.length === received.length && timingSafeEqual(expected, received);
}

function setHeaders(response, status, contentType) {
  response.statusCode = status;
  response.setHeader("Content-Type", contentType);
  response.setHeader("Cache-Control", "no-store, max-age=0");
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setHeader("Referrer-Policy", "no-referrer");
  response.setHeader("X-Frame-Options", "DENY");
  response.setHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
}

function sendJson(response, status, payload) {
  setHeaders(response, status, "application/json; charset=utf-8");
  response.end(`${JSON.stringify(payload)}\n`);
}

function authorizeDebug(request, response, config) {
  if (!config.debugToken) {
    sendJson(response, 404, { error: "technical debug API is disabled" });
    return false;
  }
  const authorization = request.headers.authorization ?? "";
  const prefix = "Bearer ";
  if (!authorization.startsWith(prefix) ||
      !tokenMatches(config.debugToken, authorization.slice(prefix.length))) {
    response.setHeader("WWW-Authenticate", "Bearer realm=\"stw-playtest-debug\"");
    sendJson(response, 401, { error: "invalid debug token" });
    return false;
  }
  return true;
}

async function readJsonBody(request) {
  const contentType = request.headers["content-type"] ?? "";
  if (!contentType.toLowerCase().startsWith("application/json")) {
    throw new HttpError(415, "Content-Type must be application/json");
  }
  const declaredLength = Number(request.headers["content-length"] ?? 0);
  if (Number.isFinite(declaredLength) && declaredLength > 4096) {
    throw new HttpError(413, "request body exceeds 4096 bytes");
  }
  const chunks = [];
  let bytes = 0;
  for await (const chunk of request) {
    bytes += chunk.length;
    if (bytes > 4096) throw new HttpError(413, "request body exceeds 4096 bytes");
    chunks.push(chunk);
  }
  try {
    const text = Buffer.concat(chunks).toString("utf8");
    const parsed = text ? JSON.parse(text) : {};
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
      throw new Error("body is not an object");
    }
    return parsed;
  } catch {
    throw new HttpError(400, "request body must be a JSON object");
  }
}

function hasOnlyKeys(object, expectedKeys) {
  const actual = Object.keys(object).sort();
  const expected = [...expectedKeys].sort();
  return actual.length === expected.length &&
    actual.every((key, index) => key === expected[index]);
}

function validateDebugControl(body) {
  if (hasOnlyKeys(body, ["command"]) && debugCommands.has(body.command)) {
    return body.command;
  }
  throw new HttpError(400, "debug command is not allowlisted or has invalid fields");
}

function validateGameAction(body) {
  if (!hasOnlyKeys(body, ["action"]) || typeof body.action !== "string" ||
      !gameActions.has(body.action)) {
    throw new HttpError(400, "game action is not allowlisted or has invalid fields");
  }
  return body.action;
}

function validateGameInput(body) {
  const keys = ["fire", "forward", "lookX", "lookY", "sprint", "strafe"];
  if (!hasOnlyKeys(body, keys)) {
    throw new HttpError(400, "game input has missing or unexpected fields");
  }
  for (const name of ["forward", "lookX", "lookY", "strafe"]) {
    if (typeof body[name] !== "number" || !Number.isFinite(body[name]) ||
        body[name] < -1 || body[name] > 1) {
      throw new HttpError(400, `${name} must be a finite number between -1 and 1`);
    }
  }
  if (typeof body.fire !== "boolean" || typeof body.sprint !== "boolean") {
    throw new HttpError(400, "fire and sprint must be booleans");
  }
  return body;
}

export class PlaytestSupervisor {
  constructor(config, dependencies = {}) {
    this.config = config;
    this.spawnProcess = dependencies.spawnProcess ?? spawnChild;
    this.child = null;
    this.commandOperation = Promise.resolve();
    this.inputOperation = Promise.resolve();
    this.commandSequence = 0;
    this.inputSequence = 0;
    this.error = "";
    this.exitCode = null;
    this.expectedStop = false;
    this.operation = Promise.resolve();
    this.sessionDirectory = null;
    this.state = "idle";
    this.test = null;
  }

  snapshot() {
    return { state: this.state, test: this.test, exitCode: this.exitCode, error: this.error };
  }

  isRunning() {
    return this.child !== null && this.child.exitCode === null &&
      this.child.signalCode === null && this.state === "running";
  }

  _exclusive(operation) {
    const next = this.operation.then(operation, operation);
    this.operation = next.catch(() => {});
    return next;
  }

  start() { return this._exclusive(() => this._start()); }
  restart() {
    return this._exclusive(async () => {
      await this._stop({ cleanupSession: true });
      return this._start();
    });
  }
  connectGame() {
    return this._exclusive(async () => {
      if (this.isRunning() && this.test === "game") {
        return { reused: true, ...this.snapshot() };
      }
      if (this.child) await this._stop({ cleanupSession: true });
      return { reused: false, ...await this._start() };
    });
  }
  stop(options = {}) { return this._exclusive(() => this._stop(options)); }
  shutdown() { return this._exclusive(() => this._stop({ cleanupSession: true })); }

  async _start() {
    if (this.child && this.child.exitCode === null && this.child.signalCode === null) {
      throw new HttpError(409, "a playtest process is already active");
    }
    await access(this.config.binaryPath, fsConstants.X_OK);
    await this._cleanupSession();
    this.sessionDirectory = await mkdtemp(path.join(tmpdir(), "stw-playtest-v2-"));
    this.commandSequence = 0;
    this.inputSequence = 0;
    this.error = "";
    this.exitCode = null;
    this.expectedStop = false;
    this.state = "starting";
    this.test = "game";

    const argumentsList = [
      "--playtest", "game",
      "--remote-dir", this.sessionDirectory,
      "--hidden",
      "--stream-fps", String(this.config.streamFramesPerSecond),
    ];
    let child;
    try {
      child = this.spawnProcess(this.config.binaryPath, argumentsList, {
        cwd: this.config.engineDirectory,
        detached: false,
        env: buildChildEnvironment(this.config.display),
        shell: false,
        stdio: ["ignore", "pipe", "pipe"],
      });
    } catch (error) {
      this.state = "error";
      this.error = sanitizeDiagnostic(error.message);
      throw error;
    }
    this.child = child;
    child.stdout?.on("data", () => {});
    child.stderr?.on("data", (chunk) => {
      this.error = sanitizeDiagnostic(`${this.error}\n${chunk.toString("utf8")}`);
    });
    child.on("error", (error) => {
      if (this.child !== child) return;
      this.state = "error";
      this.error = sanitizeDiagnostic(error.message);
    });
    child.on("exit", (code, signal) => {
      if (this.child !== child) return;
      this.child = null;
      this.exitCode = Number.isInteger(code) ? code : null;
      if (this.expectedStop || code === 0) this.state = "stopped";
      else {
        this.state = "error";
        if (signal) this.error = `playtest exited after signal ${signal}`;
        else if (!this.error) this.error = `playtest exited with code ${String(code)}`;
      }
    });
    try {
      await new Promise((resolve, reject) => {
        const onSpawn = () => {
          child.off("error", onStartupError);
          resolve();
        };
        const onStartupError = (error) => {
          child.off("spawn", onSpawn);
          reject(error);
        };
        child.once("spawn", onSpawn);
        child.once("error", onStartupError);
      });
    } catch (error) {
      if (this.child === child) this.child = null;
      this.state = "error";
      this.error = sanitizeDiagnostic(error.message);
      throw error;
    }
    if (this.child === child) this.state = "running";
    return this.snapshot();
  }

  async _stop({ cleanupSession = false } = {}) {
    const child = this.child;
    if (child && child.exitCode === null && child.signalCode === null) {
      this.expectedStop = true;
      this.state = "stopping";
      try { await this._queueCommand("stop"); }
      catch (error) { this.error = sanitizeDiagnostic(error.message); }
      let exited = await waitForExit(child, this.config.stopTimeoutMilliseconds);
      if (!exited) {
        child.kill("SIGTERM");
        exited = await waitForExit(child, this.config.stopTimeoutMilliseconds);
      }
      if (!exited) {
        child.kill("SIGKILL");
        exited = await waitForExit(child, 1000);
      }
      if (!exited) {
        this.state = "error";
        this.error = "owned STW process did not exit after SIGKILL";
        throw new Error(this.error);
      }
    }
    if (this.state !== "error") this.state = "stopped";
    if (cleanupSession) await this._cleanupSession();
    return this.snapshot();
  }

  async _cleanupSession() {
    await Promise.all([
      this.commandOperation.catch(() => {}),
      this.inputOperation.catch(() => {}),
    ]);
    const directory = this.sessionDirectory;
    this.sessionDirectory = null;
    if (directory) await rm(directory, { force: true, recursive: true });
  }

  async _writeSequenced(fileName, sequence, payload, directory) {
    if (!directory) throw new Error("no active playtest session");
    const finalPath = path.join(directory, fileName);
    const temporaryPath = `${finalPath}.${sequence}.tmp`;
    await writeFile(temporaryPath, `${sequence} ${payload}\n`, {
      encoding: "utf8",
      mode: 0o600,
    });
    await rename(temporaryPath, finalPath);
  }

  _queueCommand(commandText) {
    const directory = this.sessionDirectory;
    const sequence = ++this.commandSequence;
    const next = this.commandOperation.then(
      () => this._writeSequenced("command.txt", sequence, commandText, directory),
    );
    this.commandOperation = next.catch(() => {});
    return next;
  }

  _queueInput(input) {
    const directory = this.sessionDirectory;
    const sequence = ++this.inputSequence;
    const values = [input.strafe, input.forward, input.lookX, input.lookY,
      input.fire ? 1 : 0, input.sprint ? 1 : 0].join(" ");
    const next = this.inputOperation.then(
      () => this._writeSequenced("input.txt", sequence, `input ${values}`, directory),
    );
    this.inputOperation = next.catch(() => {});
    return next;
  }

  async publishCommand(commandText) {
    if (!this.isRunning()) throw new HttpError(409, "no playtest is running");
    await this._queueCommand(commandText);
  }

  async publishGameAction(action) {
    if (!this.isRunning() || this.test !== "game") {
      throw new HttpError(409, "the real game runtime is not connected");
    }
    const command = { start: "game_start", pause: "game_pause",
      reset: "game_reset", weapon: "game_weapon", reload: "game_reload" }[action];
    await this._queueCommand(command);
  }

  async publishGameInput(input) {
    if (!this.isRunning() || this.test !== "game") {
      throw new HttpError(409, "the real game runtime is not connected");
    }
    await this._queueInput(input);
  }

  async readEngineStatus() {
    if (!this.sessionDirectory) return null;
    try {
      const statusPath = path.join(this.sessionDirectory, "status.json");
      const metadata = await stat(statusPath, { bigint: true });
      if (metadata.size > 65536n) throw new Error("native status file exceeds 64 KiB");
      const parsed = JSON.parse(await readFile(statusPath, "utf8"));
      return parsed && typeof parsed === "object" && !Array.isArray(parsed)
        ? parsed : null;
    } catch (error) {
      if (error.code === "ENOENT") return null;
      this.error = sanitizeDiagnostic(error.message);
      return null;
    }
  }

  async readFrame(ifNoneMatch = "") {
    if (!this.sessionDirectory) throw new HttpError(404, "no frame is available");
    try {
      const framePath = path.join(this.sessionDirectory, "frame.png");
      const metadata = await stat(framePath, { bigint: true });
      if (metadata.size > 32n * 1024n * 1024n) {
        throw new Error("native frame exceeds 32 MiB");
      }
      const etag = `\"${metadata.mtimeNs.toString(16)}-${metadata.size.toString(16)}\"`;
      if (ifNoneMatch === etag) return { etag, notModified: true };
      return { etag, frame: await readFile(framePath), notModified: false };
    } catch (error) {
      if (error.code === "ENOENT") throw new HttpError(404, "no frame is available yet");
      throw new HttpError(503, sanitizeDiagnostic(error.message));
    }
  }
}

async function serveStatic(response, pathname, config) {
  const files = new Map([
    [`${config.routePrefix}/`, ["index.html", "text/html; charset=utf-8"]],
    [`${config.routePrefix}/index.html`, ["index.html", "text/html; charset=utf-8"]],
    [`${config.routePrefix}/app.js`, ["app.js", "text/javascript; charset=utf-8"]],
    [`${config.routePrefix}/mobile-input.mjs`, ["mobile-input.mjs", "text/javascript; charset=utf-8"]],
    [`${config.routePrefix}/style.css`, ["style.css", "text/css; charset=utf-8"]],
  ]);
  const entry = files.get(pathname);
  if (!entry) return false;
  const [fileName, contentType] = entry;
  const contents = await readFile(path.join(config.webDirectory, fileName));
  setHeaders(response, 200, contentType);
  response.setHeader("Content-Security-Policy",
    "default-src 'self'; img-src 'self' blob:; script-src 'self'; style-src 'self'; connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
  response.end(contents);
  return true;
}

function createRequestHandler(config, supervisor) {
  return async (request, response) => {
    try {
      const url = new URL(request.url ?? "/", "http://localhost");
      if (request.method === "GET" && url.pathname === "/") {
        response.statusCode = 302;
        response.setHeader("Location", `${config.routePrefix}/`);
        response.end();
        return;
      }
      if (request.method === "GET" && await serveStatic(response, url.pathname, config)) return;
      if (!url.pathname.startsWith(`${config.routePrefix}/api/`)) {
        sendJson(response, 404, { error: "not found" });
        return;
      }

      if (url.pathname.startsWith(`${config.routePrefix}/api/debug/`)) {
        if (!authorizeDebug(request, response, config)) return;
        if (request.method === "POST" &&
            url.pathname === `${config.routePrefix}/api/debug/start`) {
          const body = await readJsonBody(request);
          if (!hasOnlyKeys(body, [])) {
            throw new HttpError(400, "debug start body must be empty");
          }
          if (supervisor.isRunning()) {
            sendJson(response, 200, { reused: true, ...supervisor.snapshot() });
          } else {
            sendJson(response, 202, await supervisor.start());
          }
          return;
        }
        if (request.method === "POST" &&
            url.pathname === `${config.routePrefix}/api/debug/restart`) {
          const body = await readJsonBody(request);
          if (!hasOnlyKeys(body, [])) {
            throw new HttpError(400, "debug restart body must be empty");
          }
          sendJson(response, 202, await supervisor.restart());
          return;
        }
        if (request.method === "POST" &&
            url.pathname === `${config.routePrefix}/api/debug/command`) {
          const command = validateDebugControl(await readJsonBody(request));
          if (command === "stop") await supervisor.stop();
          else await supervisor.publishCommand(command);
          sendJson(response, 202, { accepted: command, state: supervisor.state });
          return;
        }
        if (request.method === "GET" &&
            url.pathname === `${config.routePrefix}/api/debug/status`) {
          sendJson(response, 200, {
            bridge: supervisor.snapshot(),
            engine: await supervisor.readEngineStatus(),
            streamFramesPerSecond: config.streamFramesPerSecond,
          });
          return;
        }
        sendJson(response, 404, { error: "debug endpoint not found" });
        return;
      }

      if (request.method === "POST" &&
          url.pathname === `${config.routePrefix}/api/connect`) {
        const body = await readJsonBody(request);
        if (!hasOnlyKeys(body, [])) throw new HttpError(400, "connect body must be empty");
        sendJson(response, 202, await supervisor.connectGame());
        return;
      }
      if (request.method === "POST" &&
          url.pathname === `${config.routePrefix}/api/input`) {
        await supervisor.publishGameInput(validateGameInput(await readJsonBody(request)));
        sendJson(response, 202, { accepted: "input" });
        return;
      }
      if (request.method === "POST" &&
          url.pathname === `${config.routePrefix}/api/action`) {
        const action = validateGameAction(await readJsonBody(request));
        await supervisor.publishGameAction(action);
        sendJson(response, 202, { accepted: action });
        return;
      }
      if (request.method === "GET" &&
          url.pathname === `${config.routePrefix}/api/status`) {
        sendJson(response, 200, {
          bridge: supervisor.snapshot(),
          engine: await supervisor.readEngineStatus(),
          streamFramesPerSecond: config.streamFramesPerSecond,
        });
        return;
      }
      if (request.method === "GET" &&
          url.pathname === `${config.routePrefix}/api/frame`) {
        const result = await supervisor.readFrame(request.headers["if-none-match"] ?? "");
        response.setHeader("ETag", result.etag);
        if (result.notModified) {
          response.statusCode = 304;
          response.setHeader("Cache-Control", "no-store, max-age=0");
          response.end();
        } else {
          setHeaders(response, 200, "image/png");
          response.setHeader("Content-Length", result.frame.length);
          response.end(result.frame);
        }
        return;
      }
      response.setHeader("Allow", "GET, POST");
      sendJson(response, 405, { error: "method not allowed" });
    } catch (error) {
      const status = error instanceof HttpError ? error.status : 500;
      const message = status >= 500
        ? `bridge error: ${sanitizeDiagnostic(error.message)}` : error.message;
      sendJson(response, status, { error: message });
    }
  };
}

export function createPlaytestBridge(config, dependencies = {}) {
  const supervisor = new PlaytestSupervisor(config, dependencies);
  const server = createHttpServer(createRequestHandler(config, supervisor));
  let listening = false;
  return {
    server,
    supervisor,
    async listen() {
      await access(config.binaryPath, fsConstants.X_OK);
      await new Promise((resolve, reject) => {
        const onError = (error) => {
          server.off("listening", onListening);
          reject(error);
        };
        const onListening = () => {
          server.off("error", onError);
          listening = true;
          resolve();
        };
        server.once("error", onError);
        server.once("listening", onListening);
        server.listen(config.port, config.host);
      });
      return server.address();
    },
    async close() {
      let shutdownError = null;
      try { await supervisor.shutdown(); }
      catch (error) { shutdownError = error; }
      if (listening) {
        await new Promise((resolve, reject) => {
          server.close((error) => error ? reject(error) : resolve());
          server.closeIdleConnections?.();
        });
        listening = false;
      }
      if (shutdownError) throw shutdownError;
    },
  };
}
