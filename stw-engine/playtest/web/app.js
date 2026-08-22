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
const touchMovement = { strafe: 0, forward: 0 };
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
  input.forward = Math.max(-1, Math.min(1, touchMovement.forward +
    (keyboard.has("KeyW") ? 1 : 0) - (keyboard.has("KeyS") ? 1 : 0)));
  input.strafe = Math.max(-1, Math.min(1, touchMovement.strafe +
    (keyboard.has("KeyD") ? 1 : 0) - (keyboard.has("KeyA") ? 1 : 0)));
  input.sprint = keyboard.has("ShiftLeft") || keyboard.has("ShiftRight") ||
    document.querySelector("#sprint").classList.contains("held");
}

async function sendInput() {
  if (!connected || inputInFlight) return;
  updateKeyboardInput();
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

function bindStick(pad, knob) {
  let pointerId = null;
  const update = (event) => {
    const bounds = pad.getBoundingClientRect();
    const radius = bounds.width * 0.38;
    let x = event.clientX - bounds.left - bounds.width / 2;
    let y = event.clientY - bounds.top - bounds.height / 2;
    const length = Math.hypot(x, y);
    if (length > radius) {
      x = x / length * radius;
      y = y / length * radius;
    }
    knob.style.transform = `translate(${x}px, ${y}px)`;
    touchMovement.strafe = x / radius;
    touchMovement.forward = -y / radius;
  };
  const release = (event) => {
    if (event.pointerId !== pointerId) return;
    pointerId = null;
    knob.style.transform = "translate(0, 0)";
    touchMovement.strafe = 0;
    touchMovement.forward = 0;
  };
  pad.addEventListener("pointerdown", (event) => {
    if (event.pointerType === "mouse") return;
    event.preventDefault();
    pointerId = event.pointerId;
    pad.setPointerCapture(pointerId);
    update(event);
  });
  pad.addEventListener("pointermove", (event) => {
    if (event.pointerId === pointerId) update(event);
  });
  pad.addEventListener("pointerup", release);
  pad.addEventListener("pointercancel", release);
}

function bindLook(pad) {
  let pointerId = null;
  let previousX = 0;
  let previousY = 0;
  pad.addEventListener("pointerdown", (event) => {
    if (event.pointerType === "mouse") return;
    event.preventDefault();
    pointerId = event.pointerId;
    previousX = event.clientX;
    previousY = event.clientY;
    pad.setPointerCapture(pointerId);
  });
  pad.addEventListener("pointermove", (event) => {
    if (event.pointerId !== pointerId) return;
    input.lookX = Math.max(-1, Math.min(1, input.lookX + (event.clientX - previousX) / 45));
    input.lookY = Math.max(-1, Math.min(1, input.lookY + (event.clientY - previousY) / 45));
    previousX = event.clientX;
    previousY = event.clientY;
  });
  const release = (event) => {
    if (event.pointerId === pointerId) pointerId = null;
  };
  pad.addEventListener("pointerup", release);
  pad.addEventListener("pointercancel", release);
}

function bindHeldButton(button, field) {
  const set = (held) => {
    input[field] = held;
    button.classList.toggle("held", held);
  };
  button.addEventListener("pointerdown", (event) => {
    event.preventDefault();
    button.setPointerCapture(event.pointerId);
    set(true);
  });
  button.addEventListener("pointerup", () => set(false));
  button.addEventListener("pointercancel", () => set(false));
}

bindStick(document.querySelector("#move-pad"), document.querySelector("#move-knob"));
bindLook(document.querySelector("#look-pad"));
bindHeldButton(document.querySelector("#fire"), "fire");
bindHeldButton(document.querySelector("#sprint"), "sprint");

menuStart.addEventListener("click", () => void sendAction("start"));
document.querySelector("#weapon-button").addEventListener("click", () => void sendAction("weapon"));
document.querySelector("#reload").addEventListener("click", () => void sendAction("reload"));
document.querySelector("#pause").addEventListener("click", () => void sendAction("pause"));
document.querySelector("#reset").addEventListener("click", () => void sendAction("reset"));
document.querySelector("#hud-toggle").addEventListener("click", () => {
  document.querySelector("#hud").classList.toggle("expanded");
});

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
window.addEventListener("blur", () => {
  keyboard.clear();
  input.fire = false;
  input.sprint = false;
});

game.addEventListener("mousedown", (event) => {
  if (event.button !== 0 || event.target.closest("button")) return;
  if (engineState === "MENU") void sendAction("start");
  else {
    input.fire = true;
    game.requestPointerLock?.();
  }
});
window.addEventListener("mouseup", (event) => {
  if (event.button === 0) input.fire = false;
});
window.addEventListener("mousemove", (event) => {
  if (document.pointerLockElement !== game) return;
  input.lookX = Math.max(-1, Math.min(1, input.lookX + event.movementX / 55));
  input.lookY = Math.max(-1, Math.min(1, input.lookY + event.movementY / 55));
});

game.addEventListener("touchmove", (event) => event.preventDefault(), { passive: false });
document.addEventListener("gesturestart", (event) => event.preventDefault());
document.addEventListener("contextmenu", (event) => event.preventDefault());

setInterval(() => void pollStatus(), 500);
setInterval(() => void pollFrame(), 70);
setInterval(() => void sendInput(), 50);
void connect();
void pollStatus();
