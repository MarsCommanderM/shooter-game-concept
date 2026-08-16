const root = "/stw-playtest/api";
const token = document.querySelector("#token");
const status = document.querySelector("#status");
const frame = document.querySelector("#frame");
let frameUrl = null;
token.value = sessionStorage.getItem("stw-debug-token") ?? "";
token.addEventListener("input", () => sessionStorage.setItem("stw-debug-token", token.value));

async function request(path, body) {
  const response = await fetch(`${root}/debug${path}`, {
    method: body === undefined ? "GET" : "POST",
    cache: "no-store",
    headers: {
      Authorization: `Bearer ${token.value}`,
      ...(body === undefined ? {} : { "Content-Type": "application/json" }),
    },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(payload.error ?? `HTTP ${response.status}`);
  return payload;
}

document.querySelector("#restart").addEventListener("click", async () => {
  try {
    await request("/restart", { test: document.querySelector("#scene").value });
  } catch (error) { status.textContent = error.message; }
});
document.querySelectorAll("[data-command]").forEach((button) => {
  button.addEventListener("click", async () => {
    try { await request("/command", { command: button.dataset.command }); }
    catch (error) { status.textContent = error.message; }
  });
});

setInterval(async () => {
  if (!token.value) return;
  try { status.textContent = JSON.stringify(await request("/status"), null, 2); }
  catch (error) { status.textContent = error.message; }
}, 700);
setInterval(async () => {
  try {
    const response = await fetch(`${root}/frame`, { cache: "no-store" });
    if (!response.ok) return;
    const next = URL.createObjectURL(await response.blob());
    frame.src = next;
    if (frameUrl) URL.revokeObjectURL(frameUrl);
    frameUrl = next;
  } catch { /* Status view reports bridge errors. */ }
}, 120);
