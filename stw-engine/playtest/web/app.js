const apiRoot = "/stw-playtest/api";
const tokenInput = document.querySelector("#token");
const testSelect = document.querySelector("#test-select");
const connection = document.querySelector("#connection");
const frame = document.querySelector("#frame");
const framePlaceholder = document.querySelector("#frame-placeholder");
const checklist = document.querySelector("#checklist");
const verdict = document.querySelector("#verdict");
let currentFrameUrl = null;
let movement = { x: 0, y: 0 };
let look = { x: 0, y: 0 };
let bridgeRunning = false;

const checklistItems = {
  skinning: [
    "mesh visible",
    "bind pose works",
    "animation bends mesh",
    "no exploding vertices",
    "no visible NaN/flicker",
    "pause/resume works",
    "reset works",
  ],
  animation: [
    "glTF loaded",
    "clip detected",
    "animation moves joints",
    "loop works",
    "bind pose works",
    "pause/resume works",
    "reset works",
  ],
};

tokenInput.value = sessionStorage.getItem("stw-playtest-token") ?? "";
tokenInput.addEventListener("input", () => {
  sessionStorage.setItem("stw-playtest-token", tokenInput.value);
});

function authorizationHeaders(json = false) {
  const headers = { Authorization: `Bearer ${tokenInput.value}` };
  if (json) headers["Content-Type"] = "application/json";
  return headers;
}

async function requestJson(path, options = {}) {
  const response = await fetch(`${apiRoot}${path}`, {
    ...options,
    cache: "no-store",
    headers: {
      ...authorizationHeaders(options.body !== undefined),
      ...(options.headers ?? {}),
    },
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(payload.error ?? `HTTP ${response.status}`);
  return payload;
}

function setConnection(online, label = online ? "ONLINE" : "OFFLINE") {
  connection.textContent = label;
  connection.classList.toggle("online", online);
  connection.classList.toggle("offline", !online);
}

function formatNumber(value, digits = 1) {
  return Number.isFinite(value) ? Number(value).toFixed(digits) : "—";
}

function updateStatus(payload) {
  const engine = payload.engine;
  const bridge = payload.bridge ?? {};
  bridgeRunning = bridge.state === "running";
  setConnection(bridgeRunning, (bridge.state ?? "offline").toUpperCase());
  document.querySelector("#status-test").textContent = engine?.playtest ?? bridge.test ?? "—";
  document.querySelector("#status-state").textContent = engine?.state ?? bridge.state ?? "—";
  document.querySelector("#status-fps").textContent = formatNumber(engine?.fps, 1);
  document.querySelector("#status-frame").textContent = engine ? `${formatNumber(engine.frameTimeMs, 1)} ms` : "—";
  document.querySelector("#status-time").textContent = engine ? `${formatNumber(engine.animationTime, 2)} s` : "—";
  document.querySelector("#status-clip").textContent = engine?.clip ?? "—";
  document.querySelector("#status-joints").textContent = engine?.jointCount ?? "—";
  document.querySelector("#status-palette").textContent = engine?.paletteSize ?? "—";
  const error = engine?.lastError || bridge.error || "";
  const errorElement = document.querySelector("#last-error");
  errorElement.textContent = `Last error: ${error || "none"}`;
  errorElement.classList.toggle("active", Boolean(error));
}

async function pollStatus() {
  if (!tokenInput.value) {
    setConnection(false, "TOKEN NEEDED");
    setTimeout(pollStatus, 600);
    return;
  }
  try {
    updateStatus(await requestJson("/status"));
  } catch (error) {
    bridgeRunning = false;
    setConnection(false, "OFFLINE");
    document.querySelector("#last-error").textContent = `Last error: ${error.message}`;
  }
  setTimeout(pollStatus, 500);
}

async function pollFrame() {
  if (tokenInput.value && bridgeRunning) {
    try {
      const response = await fetch(`${apiRoot}/frame?fresh=${Date.now()}`, {
        cache: "no-store",
        headers: authorizationHeaders(),
      });
      if (response.ok) {
        const blob = await response.blob();
        const nextUrl = URL.createObjectURL(blob);
        frame.src = nextUrl;
        frame.style.display = "block";
        framePlaceholder.style.display = "none";
        if (currentFrameUrl) URL.revokeObjectURL(currentFrameUrl);
        currentFrameUrl = nextUrl;
      }
    } catch {
      // Status polling owns connection/error reporting.
    }
  }
  setTimeout(pollFrame, 100);
}

async function sendCommand(command, values) {
  if (!tokenInput.value) throw new Error("enter the playtest token first");
  return requestJson("/command", {
    method: "POST",
    body: JSON.stringify(values ? { command, values } : { command }),
  });
}

document.querySelector("#start").addEventListener("click", async () => {
  try {
    if (!tokenInput.value) throw new Error("enter the playtest token first");
    await requestJson("/start", {
      method: "POST",
      body: JSON.stringify({ test: testSelect.value }),
    });
    setConnection(true, "STARTING");
  } catch (error) {
    setConnection(false, "ERROR");
    document.querySelector("#last-error").textContent = `Last error: ${error.message}`;
  }
});

document.querySelectorAll("[data-command]").forEach((button) => {
  button.addEventListener("click", () => {
    sendCommand(button.dataset.command).catch((error) => {
      document.querySelector("#last-error").textContent = `Last error: ${error.message}`;
    });
  });
});

function bindPad(pad, knob, onValue) {
  let pointerId = null;
  const update = (event) => {
    const bounds = pad.getBoundingClientRect();
    const radius = bounds.width * 0.38;
    let x = event.clientX - (bounds.left + bounds.width / 2);
    let y = event.clientY - (bounds.top + bounds.height / 2);
    const length = Math.hypot(x, y);
    if (length > radius) {
      x = (x / length) * radius;
      y = (y / length) * radius;
    }
    knob.style.transform = `translate(${x}px, ${y}px)`;
    onValue(x / radius, y / radius);
  };
  const release = (event) => {
    if (pointerId !== event.pointerId) return;
    pointerId = null;
    knob.style.transform = "translate(0, 0)";
    onValue(0, 0);
  };
  pad.addEventListener("pointerdown", (event) => {
    event.preventDefault();
    pointerId = event.pointerId;
    pad.setPointerCapture(pointerId);
    update(event);
  });
  pad.addEventListener("pointermove", (event) => {
    if (pointerId === event.pointerId) update(event);
  });
  pad.addEventListener("pointerup", release);
  pad.addEventListener("pointercancel", release);
}

bindPad(document.querySelector("#move-pad"), document.querySelector("#move-knob"), (x, y) => {
  movement = { x, y: -y };
});
bindPad(document.querySelector("#look-pad"), document.querySelector("#look-knob"), (x, y) => {
  look = { x, y };
});

setInterval(() => {
  if (!bridgeRunning || !tokenInput.value) return;
  sendCommand("camera", [movement.x, movement.y, look.x, look.y]).catch(() => {});
}, 100);

function storageKey(suffix) {
  return `stw-playtest:${testSelect.value}:${suffix}`;
}

function renderChecklist() {
  const saved = JSON.parse(localStorage.getItem(storageKey("checks")) ?? "[]");
  checklist.replaceChildren();
  checklistItems[testSelect.value].forEach((label, index) => {
    const row = document.createElement("label");
    row.className = "check-item";
    const checkbox = document.createElement("input");
    checkbox.type = "checkbox";
    checkbox.checked = Boolean(saved[index]);
    checkbox.addEventListener("change", () => {
      const checks = [...checklist.querySelectorAll("input")].map((item) => item.checked);
      localStorage.setItem(storageKey("checks"), JSON.stringify(checks));
    });
    row.append(checkbox, document.createTextNode(label));
    checklist.append(row);
  });
  const savedVerdict = localStorage.getItem(storageKey("verdict"));
  showVerdict(savedVerdict);
}

function showVerdict(value) {
  verdict.className = "verdict";
  if (value === "PASS" || value === "FAIL") {
    verdict.textContent = `${value} · saved locally`;
    verdict.classList.add(value.toLowerCase());
  } else {
    verdict.textContent = "Not reported";
  }
}

document.querySelector("#pass-verdict").addEventListener("click", () => {
  localStorage.setItem(storageKey("verdict"), "PASS");
  showVerdict("PASS");
});
document.querySelector("#fail-verdict").addEventListener("click", () => {
  localStorage.setItem(storageKey("verdict"), "FAIL");
  showVerdict("FAIL");
});
testSelect.addEventListener("change", renderChecklist);

renderChecklist();
pollStatus();
pollFrame();
