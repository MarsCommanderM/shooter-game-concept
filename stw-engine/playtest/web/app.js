import {
  MobilePointerInput,
  POINTER_ROLE,
  REMOTE_LOOK_MOUSE_COUNTS_PER_UNIT,
} from "./mobile-input.mjs";

const apiRoot = "/stw-playtest/api";
const game = document.querySelector("#game");
const frame = document.querySelector("#frame");
const connecting = document.querySelector("#connecting");
const connectingText = document.querySelector("#connecting-text");
const menuStart = document.querySelector("#menu-start");
const connection = document.querySelector("#connection");
const lastError = document.querySelector("#last-error");

const input = {
  strafe: 0,
  forward: 0,
  lookX: 0,
  lookY: 0,
  fire: false,
  sprint: false,
};
const keyboard = new Set();
const mobileInput = new MobilePointerInput();
const capturedPointers = new Map();
let mouseFire = false;
let connected = false;
let engineState = "";
let frameEtag = "";
let currentFrameUrl = null;
let frameInFlight = false;
let inputInFlight = false;
let statusInFlight = false;
let connectInFlight = false;
let reconnectTimer = null;
let reconnectDelay = 500;
let frameArrivals = [];

async function jsonRequest(path, body) {
  const response = await fetch(`${apiRoot}${path}`, {
    method: body === undefined ? "GET" : "POST",
    cache: "no-store",
    headers: body === undefined ? {} : { "Content-Type": "application/json" },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(payload.error ?? `HTTP ${response.status}`);
  return payload;
}

function showError(error) {
  const message = error instanceof Error ? error.message : String(error);
  lastError.textContent = message;
  lastError.classList.add("error");
}

function recordFrame() {
  const now = performance.now();
  frameArrivals.push(now);
  frameArrivals = frameArrivals.filter((time) => now - time <= 2000);
  const first = frameArrivals[0];
  const rate = frameArrivals.length > 1 && now > first
    ? (frameArrivals.length - 1) / ((now - first) / 1000)
    : frameArrivals.length;
  document.querySelector("#rx-fps").textContent = rate.toFixed(1);
}

async function connect() {
  if (connectInFlight) return;
  connectInFlight = true;
  if (reconnectTimer !== null) {
    clearTimeout(reconnectTimer);
    reconnectTimer = null;
  }
  try {
    await jsonRequest("/connect", {});
    connected = true;
    reconnectDelay = 500;
    connection.textContent = "CONNECTED";
    connection.classList.add("online");
    connectingText.textContent = "Waiting for the native renderer…";
  } catch (error) {
    connected = false;
    connection.textContent = "DISCONNECTED";
    connection.classList.remove("online");
    connectingText.textContent = "Native runtime unavailable — reconnecting…";
    showError(error);
    reconnectTimer = setTimeout(() => void connect(), reconnectDelay);
    reconnectDelay = Math.min(reconnectDelay * 2, 5000);
  } finally {
    connectInFlight = false;
  }
}

function renderStatus(payload) {
  const bridge = payload.bridge ?? {};
  const engine = payload.engine ?? null;
  connected = bridge.state === "running" && bridge.test === "game";
  connection.textContent = connected ? "CONNECTED" : String(bridge.state ?? "OFFLINE").toUpperCase();
  connection.classList.toggle("online", connected);
  engineState = engine?.state ?? "";
  document.querySelector("#scene").textContent = engine?.scene ?? "starting";
  document.querySelector("#native-fps").textContent = Number.isFinite(engine?.fps)
    ? Number(engine.fps).toFixed(1) : "0.0";
  document.querySelector("#weapon").textContent = engine?.weapon ?? "—";
  document.querySelector("#ammo-magazine").textContent =
    Number.isInteger(engine?.ammoMagazine) ? engine.ammoMagazine : "—";
  document.querySelector("#ammo-reserve").textContent =
    Number.isInteger(engine?.ammoReserve) ? engine.ammoReserve : "—";
  document.querySelector("#reload-state").textContent = engine?.reloading
    ? `RELOAD ${Math.round(Number(engine.reloadProgress ?? 0) * 100)}%`
    : String(engine?.quality ?? "READY");
  document.querySelector("#combat-weapon").textContent = engine?.weapon ?? "—";
  document.querySelector("#combat-magazine").textContent =
    Number.isInteger(engine?.ammoMagazine) ? engine.ammoMagazine : "—";
  document.querySelector("#combat-reserve").textContent =
    Number.isInteger(engine?.ammoReserve) ? engine.ammoReserve : "—";
  const health = Number.isFinite(engine?.player?.health)
    ? Math.max(0, Math.round(Number(engine.player.health))) : null;
  const botsAlive = Number.isInteger(engine?.botsAlive) ? engine.botsAlive : null;
  const botsTotal = Number.isInteger(engine?.botsTotal) ? engine.botsTotal : null;
  document.querySelector("#player-health").textContent = health ?? "—";
  document.querySelector("#bots-alive").textContent = botsAlive ?? "—";
  document.querySelector("#bots-total").textContent = botsTotal ?? "—";
  document.querySelector("#combat-health").textContent = health ?? "—";
  document.querySelector("#combat-bots").textContent = botsAlive ?? "—";
  document.querySelector("#combat-health").classList.toggle(
    "critical", health !== null && health <= 25,
  );
  document.querySelector("#combat-reload").textContent = engine?.reloading
    ? "RELOADING" : "";
  document.querySelector("#animation-state").textContent =
    engine?.animationState ?? "Unavailable";
  document.querySelector("#animation-clip").textContent =
    `clip ${engine?.clip || "—"}`;
  document.querySelector("#animation-time").textContent =
    Number.isFinite(engine?.animationTime)
      ? Number(engine.animationTime).toFixed(2) : "0.00";
  const error = engine?.lastError || bridge.error || "";
  lastError.textContent = error || "no error";
  lastError.classList.toggle("error", Boolean(error));
  menuStart.classList.toggle("visible", engineState === "MENU");
  if (!connected && bridge.state !== "starting") {
    void connect();
  }
}

async function pollStatus() {
  if (statusInFlight) return;
  statusInFlight = true;
  try {
    renderStatus(await jsonRequest("/status"));
  } catch (error) {
    connected = false;
    showError(error);
  } finally {
    statusInFlight = false;
  }
}

async function pollFrame() {
  if (frameInFlight || !connected) return;
  frameInFlight = true;
  try {
    const headers = frameEtag ? { "If-None-Match": frameEtag } : {};
    const response = await fetch(`${apiRoot}/frame`, { cache: "no-store", headers });
    if (response.ok) {
      frameEtag = response.headers.get("etag") ?? "";
      const nextUrl = URL.createObjectURL(await response.blob());
      frame.src = nextUrl;
      frame.classList.add("ready");
      connecting.classList.add("hidden");
      if (currentFrameUrl) URL.revokeObjectURL(currentFrameUrl);
      currentFrameUrl = nextUrl;
      recordFrame();
    } else if (response.status !== 304 && response.status !== 404) {
      const payload = await response.json().catch(() => ({}));
      throw new Error(payload.error ?? `frame HTTP ${response.status}`);
    }
  } catch (error) {
    showError(error);
  } finally {
    frameInFlight = false;
  }
}

async function sendAction(action) {
  try {
    await jsonRequest("/action", { action });
  } catch (error) {
    showError(error);
  }
}

function updateKeyboardInput() {
  const movement = mobileInput.movement;
  input.forward = Math.max(-1, Math.min(1, movement.forward +
    (keyboard.has("KeyW") ? 1 : 0) - (keyboard.has("KeyS") ? 1 : 0)));
  input.strafe = Math.max(-1, Math.min(1, movement.strafe +
    (keyboard.has("KeyD") ? 1 : 0) - (keyboard.has("KeyA") ? 1 : 0)));
  input.sprint = keyboard.has("ShiftLeft") || keyboard.has("ShiftRight") ||
    mobileInput.isHeld("sprint");
  input.fire = mouseFire || mobileInput.isHeld("fire");
}

async function sendInput() {
  if (!connected || inputInFlight) return;
  updateKeyboardInput();
  const look = mobileInput.consumeLook();
  input.lookX = look.x;
  input.lookY = look.y;
  const payload = { ...input };
  input.lookX = 0;
  input.lookY = 0;
  inputInFlight = true;
  try {
    await jsonRequest("/input", payload);
  } catch (error) {
    showError(error);
  } finally {
    inputInFlight = false;
  }
}

function viewportShortSide() {
  const width = window.visualViewport?.width ?? window.innerWidth;
  const height = window.visualViewport?.height ?? window.innerHeight;
  return Math.min(width, height);
}

function capturePointer(element, pointerId) {
  try {
    element.setPointerCapture?.(pointerId);
    capturedPointers.set(pointerId, element);
  } catch {
    // Pointer Events still work while the finger remains over the element.
  }
}

function releaseCapture(pointerId) {
  const element = capturedPointers.get(pointerId);
  capturedPointers.delete(pointerId);
  if (!element) return;
  try {
    if (!element.hasPointerCapture || element.hasPointerCapture(pointerId)) {
      element.releasePointerCapture?.(pointerId);
    }
  } catch {
    // Pointer capture may already have been released by the browser.
  }
}

function bindStick(pad, knob) {
  const update = (event) => {
    const movement = mobileInput.updateMove(
      event.pointerId,
      event.clientX,
      event.clientY,
      pad.getBoundingClientRect(),
    );
    if (!movement) return;
    knob.style.transform = `translate(${movement.knobX}px, ${movement.knobY}px)`;
  };
  const release = (event) => {
    if (mobileInput.roleOf(event.pointerId) !== POINTER_ROLE.Move) return;
    mobileInput.releasePointer(event.pointerId, {
      cancelled: event.type !== "pointerup",
    });
    releaseCapture(event.pointerId);
    knob.style.transform = "translate(0, 0)";
  };
  pad.addEventListener("pointerdown", (event) => {
    if (event.pointerType === "mouse" ||
        !mobileInput.beginMove(event.pointerId)) return;
    event.preventDefault();
    event.stopPropagation();
    capturePointer(pad, event.pointerId);
    update(event);
  });
  pad.addEventListener("pointermove", (event) => {
    if (mobileInput.roleOf(event.pointerId) !== POINTER_ROLE.Move) return;
    event.preventDefault();
    update(event);
  });
  pad.addEventListener("pointerup", release);
  pad.addEventListener("pointercancel", release);
  pad.addEventListener("lostpointercapture", release);
}

function bindLook(pad) {
  const release = (event) => {
    if (mobileInput.roleOf(event.pointerId) !== POINTER_ROLE.Look) return;
    mobileInput.releasePointer(event.pointerId, {
      cancelled: event.type !== "pointerup",
    });
    releaseCapture(event.pointerId);
  };
  pad.addEventListener("pointerdown", (event) => {
    if (event.pointerType === "mouse" ||
        !mobileInput.beginLook(event.pointerId, event.clientX, event.clientY)) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    capturePointer(pad, event.pointerId);
  });
  pad.addEventListener("pointermove", (event) => {
    if (mobileInput.roleOf(event.pointerId) !== POINTER_ROLE.Look) return;
    event.preventDefault();
    mobileInput.updateLook(
      event.pointerId,
      event.clientX,
      event.clientY,
      viewportShortSide(),
    );
  });
  pad.addEventListener("pointerup", release);
  pad.addEventListener("pointercancel", release);
  pad.addEventListener("lostpointercapture", release);
}

function bindHeldButton(button, field) {
  const sync = () => button.classList.toggle("held", mobileInput.isHeld(field));
  const release = (event) => {
    if (mobileInput.roleOf(event.pointerId) !== `button:${field}`) return;
    mobileInput.releasePointer(event.pointerId, {
      cancelled: event.type !== "pointerup",
    });
    releaseCapture(event.pointerId);
    sync();
  };
  button.addEventListener("pointerdown", (event) => {
    if (!mobileInput.beginButton(event.pointerId, field)) return;
    event.preventDefault();
    event.stopPropagation();
    capturePointer(button, event.pointerId);
    sync();
  });
  button.addEventListener("pointerup", release);
  button.addEventListener("pointercancel", release);
  button.addEventListener("lostpointercapture", release);
}

const moveKnob = document.querySelector("#move-knob");
const fireButton = document.querySelector("#fire");
const sprintButton = document.querySelector("#sprint");
bindStick(document.querySelector("#move-pad"), moveKnob);
bindLook(document.querySelector("#look-pad"));
bindHeldButton(fireButton, "fire");
bindHeldButton(sprintButton, "sprint");

menuStart.addEventListener("click", () => void sendAction("start"));
document.querySelector("#weapon-button").addEventListener("click", () => void sendAction("weapon"));
document.querySelector("#reload").addEventListener("click", () => void sendAction("reload"));
document.querySelector("#pause").addEventListener("click", () => void sendAction("pause"));
document.querySelector("#reset").addEventListener("click", () => void sendAction("reset"));
document.querySelector("#hud-toggle").addEventListener("click", () => {
  document.querySelector("#hud").classList.toggle("expanded");
});

function clearActiveInput() {
  keyboard.clear();
  mouseFire = false;
  mobileInput.clearAll();
  for (const pointerId of [...capturedPointers.keys()]) releaseCapture(pointerId);
  moveKnob.style.transform = "translate(0, 0)";
  fireButton.classList.remove("held");
  sprintButton.classList.remove("held");
  input.strafe = 0;
  input.forward = 0;
  input.lookX = 0;
  input.lookY = 0;
  input.fire = false;
  input.sprint = false;
}

window.addEventListener("keydown", (event) => {
  keyboard.add(event.code);
  if (["KeyW", "KeyA", "KeyS", "KeyD", "Space"].includes(event.code)) event.preventDefault();
  if (event.code === "Enter" || event.code === "Space") void sendAction("start");
  if (event.code === "KeyQ" && !event.repeat) void sendAction("weapon");
  if (event.code === "KeyE" && !event.repeat) void sendAction("reload");
  if (event.code === "KeyR" && !event.repeat) void sendAction("reset");
  if (event.code === "KeyP" && !event.repeat) void sendAction("pause");
});
window.addEventListener("keyup", (event) => keyboard.delete(event.code));
window.addEventListener("blur", clearActiveInput);
window.addEventListener("pagehide", clearActiveInput);
document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "hidden") clearActiveInput();
});

game.addEventListener("mousedown", (event) => {
  if (event.button !== 0 || event.target.closest("button")) return;
  if (engineState === "MENU") void sendAction("start");
  else {
    mouseFire = true;
    game.requestPointerLock?.();
  }
});
window.addEventListener("mouseup", (event) => {
  if (event.button === 0) mouseFire = false;
});
window.addEventListener("mousemove", (event) => {
  if (document.pointerLockElement !== game) return;
  mobileInput.addLookPixels(
    event.movementX,
    event.movementY,
    REMOTE_LOOK_MOUSE_COUNTS_PER_UNIT,
  );
});

game.addEventListener("touchmove", (event) => event.preventDefault(), { passive: false });
document.addEventListener("gesturestart", (event) => event.preventDefault());
document.addEventListener("dragstart", (event) => event.preventDefault());
document.addEventListener("contextmenu", (event) => event.preventDefault());

setInterval(() => void pollStatus(), 500);
setInterval(() => void pollFrame(), 70);
setInterval(() => void sendInput(), 50);
void connect();
void pollStatus();
