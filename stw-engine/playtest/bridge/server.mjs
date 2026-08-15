import { timingSafeEqual } from "node:crypto";
import { constants as fsConstants } from "node:fs";
import {
  access,
  mkdtemp,
  readFile,
  rename,
  writeFile,
} from "node:fs/promises";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawn } from "node:child_process";

const bridgeDirectory = path.dirname(fileURLToPath(import.meta.url));
const engineDirectory = path.resolve(bridgeDirectory, "../..");
const webDirectory = path.resolve(bridgeDirectory, "../web");
const routePrefix = "/stw-playtest";
const configuredToken = process.env.STW_PLAYTEST_TOKEN ?? "";
const host = "127.0.0.1";
const parsedPort = Number.parseInt(process.env.STW_PLAYTEST_PORT ?? "8791", 10);
const port = Number.isInteger(parsedPort) && parsedPort > 0 && parsedPort <= 65535
  ? parsedPort
  : 8791;
const binaryPath = path.resolve(
  process.env.STW_PLAYTEST_BINARY ?? path.join(engineDirectory, "build/stw"),
);
const knownTests = new Set(["skinning", "animation"]);
const simpleCommands = new Set(["play", "pause", "reset", "bind", "slow", "stop"]);

let activeProcess = null;
let activeSessionDirectory = null;
let activeTest = null;
let processState = "idle";
let processExitCode = null;
let processError = "";
let commandSequence = 0;

function setHeaders(response, status, contentType) {
  response.statusCode = status;
  response.setHeader("Content-Type", contentType);
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("X-Content-Type-Options", "nosniff");
  response.setHeader("Referrer-Policy", "no-referrer");
  response.setHeader("X-Frame-Options", "DENY");
}

function sendJson(response, status, payload) {
  setHeaders(response, status, "application/json; charset=utf-8");
  response.end(`${JSON.stringify(payload)}\n`);
}

function tokenMatches(candidate) {
  if (!configuredToken || typeof candidate !== "string") return false;
  const expected = Buffer.from(configuredToken);
  const received = Buffer.from(candidate);
  return expected.length === received.length && timingSafeEqual(expected, received);
}

function authorize(request, response) {
  if (!configuredToken) {
    sendJson(response, 503, {
      error: "remote playtest is disabled because STW_PLAYTEST_TOKEN is not configured",
    });
    return false;
  }
  const authorization = request.headers.authorization ?? "";
  const prefix = "Bearer ";
  if (!authorization.startsWith(prefix) ||
      !tokenMatches(authorization.slice(prefix.length))) {
    response.setHeader("WWW-Authenticate", "Bearer realm=\"stw-playtest\"");
    sendJson(response, 401, { error: "invalid playtest token" });
    return false;
  }
  return true;
}

async function readJsonBody(request) {
  const chunks = [];
  let bytes = 0;
  for await (const chunk of request) {
    bytes += chunk.length;
    if (bytes > 4096) throw new Error("request body exceeds 4096 bytes");
    chunks.push(chunk);
  }
  const text = Buffer.concat(chunks).toString("utf8");
  return text ? JSON.parse(text) : {};
}

function processIsRunning() {
  return activeProcess !== null && activeProcess.exitCode === null &&
    !activeProcess.killed && processState === "running";
}

function sanitizeDiagnostic(value) {
  return value
    .replace(/:\/\/[^/\s:@]+:[^@\s/]+@/g, "://[REDACTED]@")
    .replace(/\b(token|password|secret)\s*[=:]\s*[^\s]+/gi, "$1=[REDACTED]")
    .replace(/[^\x09\x0a\x0d\x20-\x7e]/g, "?")
    .slice(-1024)
    .trim();
}

async function publishCommand(commandText) {
  if (!activeSessionDirectory) throw new Error("no active playtest session");
  commandSequence += 1;
  const finalPath = path.join(activeSessionDirectory, "command.txt");
  const temporaryPath = `${finalPath}.tmp`;
  await writeFile(temporaryPath, `${commandSequence} ${commandText}\n`, {
    encoding: "utf8",
    mode: 0o600,
  });
  await rename(temporaryPath, finalPath);
}

async function startPlaytest(test) {
  if (!knownTests.has(test)) throw new Error("unknown playtest");
  if (processIsRunning()) throw new Error("a playtest is already running");
  await access(binaryPath, fsConstants.X_OK);

  activeSessionDirectory = await mkdtemp(path.join(tmpdir(), "stw-playtest-v1-"));
  activeTest = test;
  processState = "starting";
  processExitCode = null;
  processError = "";
  commandSequence = 0;

  const childEnvironment = { ...process.env };
  delete childEnvironment.STW_PLAYTEST_TOKEN;
  const argumentsList = [
    "--playtest",
    test,
    "--remote-dir",
    activeSessionDirectory,
    "--hidden",
    "--stream-fps",
    "10",
  ];
  const child = spawn(binaryPath, argumentsList, {
    cwd: engineDirectory,
    env: childEnvironment,
    shell: false,
    stdio: ["ignore", "pipe", "pipe"],
  });
  activeProcess = child;
  processState = "running";

  // Always drain child output. Only a bounded, credential-redacted stderr tail
  // is retained so an SDL/OpenGL startup blocker remains diagnosable.
  child.stdout.on("data", () => {});
  child.stderr.on("data", (chunk) => {
    processError = sanitizeDiagnostic(`${processError}\n${chunk.toString("utf8")}`);
  });
  child.on("error", (error) => {
    processState = "error";
    processError = error.message;
  });
  child.on("exit", (code, signal) => {
    processExitCode = code;
    processState = code === 0 ? "stopped" : "error";
    if (signal) processError = `playtest exited after signal ${signal}`;
    if (code !== 0 && !processError) {
      processError = `playtest exited with code ${String(code)}`;
    }
  });
}

async function serveStatic(response, pathname) {
  const staticFiles = new Map([
    [`${routePrefix}/`, ["index.html", "text/html; charset=utf-8"]],
    [`${routePrefix}/index.html`, ["index.html", "text/html; charset=utf-8"]],
    [`${routePrefix}/app.js`, ["app.js", "text/javascript; charset=utf-8"]],
    [`${routePrefix}/style.css`, ["style.css", "text/css; charset=utf-8"]],
  ]);
  const entry = staticFiles.get(pathname);
  if (!entry) return false;
  const [fileName, contentType] = entry;
  const contents = await readFile(path.join(webDirectory, fileName));
  setHeaders(response, 200, contentType);
  response.setHeader(
    "Content-Security-Policy",
    "default-src 'self'; img-src 'self' blob:; script-src 'self'; style-src 'self'; connect-src 'self'; base-uri 'none'; form-action 'none'",
  );
  response.end(contents);
  return true;
}

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url ?? "/", "http://localhost");
    if (request.method === "GET" && url.pathname === "/") {
      response.statusCode = 302;
      response.setHeader("Location", `${routePrefix}/`);
      response.end();
      return;
    }
    if (request.method === "GET" && await serveStatic(response, url.pathname)) return;

    if (!url.pathname.startsWith(`${routePrefix}/api/`)) {
      sendJson(response, 404, { error: "not found" });
      return;
    }
    if (!authorize(request, response)) return;

    if (request.method === "POST" && url.pathname === `${routePrefix}/api/start`) {
      const body = await readJsonBody(request);
      if (typeof body.test !== "string" || !knownTests.has(body.test)) {
        sendJson(response, 400, { error: "test must be skinning or animation" });
        return;
      }
      if (processIsRunning()) {
        sendJson(response, 409, { error: "a playtest is already running" });
        return;
      }
      try {
        await startPlaytest(body.test);
      } catch (error) {
        processState = "error";
        processError = error.message;
        sendJson(response, 503, { error: `could not start STW: ${error.message}` });
        return;
      }
      sendJson(response, 202, { test: activeTest, state: processState });
      return;
    }

    if (request.method === "POST" && url.pathname === `${routePrefix}/api/command`) {
      if (!processIsRunning()) {
        sendJson(response, 409, { error: "no playtest is running" });
        return;
      }
      const body = await readJsonBody(request);
      if (simpleCommands.has(body.command)) {
        await publishCommand(body.command);
      } else if (body.command === "camera" && Array.isArray(body.values) &&
                 body.values.length === 4 &&
                 body.values.every((value) => Number.isFinite(value) &&
                   value >= -1 && value <= 1)) {
        await publishCommand(`camera ${body.values.map(Number).join(" ")}`);
      } else {
        sendJson(response, 400, { error: "command is not allowlisted" });
        return;
      }
      sendJson(response, 202, { accepted: body.command });
      return;
    }

    if (request.method === "GET" && url.pathname === `${routePrefix}/api/status`) {
      let engineStatus = null;
      if (activeSessionDirectory) {
        try {
          engineStatus = JSON.parse(await readFile(
            path.join(activeSessionDirectory, "status.json"), "utf8"));
        } catch {
          engineStatus = null;
        }
      }
      sendJson(response, 200, {
        bridge: {
          state: processState,
          test: activeTest,
          exitCode: processExitCode,
          error: processError,
        },
        engine: engineStatus,
      });
      return;
    }

    if (request.method === "GET" && url.pathname === `${routePrefix}/api/frame`) {
      if (!activeSessionDirectory) {
        sendJson(response, 404, { error: "no frame is available" });
        return;
      }
      try {
        const frame = await readFile(path.join(activeSessionDirectory, "frame.png"));
        setHeaders(response, 200, "image/png");
        response.end(frame);
      } catch {
        sendJson(response, 404, { error: "no frame is available yet" });
      }
      return;
    }

    response.setHeader("Allow", "GET, POST");
    sendJson(response, 405, { error: "method not allowed" });
  } catch (error) {
    sendJson(response, 400, { error: error.message });
  }
});

server.listen(port, host, () => {
  const accessState = configuredToken ? "enabled" : "disabled (token missing)";
  console.log(`STW playtest bridge listening on http://${host}:${port}${routePrefix}/`);
  console.log(`Remote control: ${accessState}`);
});
