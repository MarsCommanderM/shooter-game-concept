/* WIRRWARR Online-Server: Next-Handler + WebSocket auf Port 3000 (/ws) */
/* Räume: /ws?mode=ffa (Standard) & /ws?mode=tdm (Teams werden vergeben)  */
import { createServer } from "http";
import fs from "fs";
import path from "path";
import next from "next";
import { WebSocketServer } from "ws";

const port = 3000;
const app = next({ dev: false, hostname: "0.0.0.0", port });
const handle = app.getRequestHandler();
await app.prepare();

const startedAt = Date.now();
const server = createServer((req, res) => {
  if (req.url === "/health") {
    let clients = 0;
    for (const r of rooms.values()) clients += r.clients.size;
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ ok: true, uptime: Math.round((Date.now() - startedAt) / 1000), clients, rooms: rooms.size }));
    return;
  }
  handle(req, res);
});
const wss = new WebSocketServer({ noServer: true });

const rooms = new Map();
const getRoom = (key) => {
  let r = rooms.get(key);
  if (!r) { r = { clients: new Set(), teamCounter: 0 }; rooms.set(key, r); }
  return r;
};

let nextId = 1;

/* ---------- Leaderboards (persistent pro Modus) ---------- */
const SCORE_FILE = path.join(process.cwd(), "scores.json");
function loadScores() {
  try { return JSON.parse(fs.readFileSync(SCORE_FILE, "utf-8")); } catch { return {}; }
}
function saveScores(s) {
  try { fs.writeFileSync(SCORE_FILE, JSON.stringify(s)); } catch { /* */ }
}
const seasonNow = () => { const d = new Date(); return `S${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}`; };
function topFor(mode) {
  const s = loadScores();
  return (s[mode] ?? []).filter((e) => (e.season ?? "S0") === seasonNow()).slice(0, 10);
}

/* ---------- Server-autoritative Hit-Validation (Anti-Cheat-Basis) ---------- */
const posMap = new Map(); // id -> {x,z}
const MAX_RANGE = 75;
const MAX_HITS_PER_SEC = 12;
const MAX_DMG = 30;

function validateHit(ws, m) {
  const now = Date.now();
  if (typeof m.dmg !== "number" || m.dmg > MAX_DMG || m.dmg <= 0) {
    return { ok: false, reason: "dmg unplausibel" };
  }
  ws.hitTimes = (ws.hitTimes || []).filter((t) => now - t < 1000);
  ws.hitTimes.push(now);
  if (ws.hitTimes.length > MAX_HITS_PER_SEC) {
    return { ok: false, reason: " Feuerrate" };
  }
  const a = posMap.get(ws.pid);
  const b = posMap.get(m.target);
  if (a && b) {
    const d = Math.hypot(a.x - b.x, a.z - b.z);
    if (d > MAX_RANGE) return { ok: false, reason: `Reichweite ${Math.round(d)}` };
  }
  return { ok: true };
}

wss.on("connection", (ws, req, roomKey) => {
  const room = getRoom(roomKey);
  room.clients.add(ws);
  ws.pid = nextId++;
  ws.roomKey = roomKey;
  const base = roomKey.split("-")[0];
  ws.team = ["tdm", "hq", "dom"].includes(base) ? room.teamCounter++ % 2 : base === "inv" ? 0 : ws.pid;

  const broadcast = (msg, except) => {
    const data = JSON.stringify(msg);
    for (const c of room.clients) {
      if (c !== except && c.readyState === 1) c.send(data);
    }
  };
  broadcast({ t: "join", id: ws.pid }, ws);

  ws.on("message", (raw) => {
    let m;
    try { m = JSON.parse(raw.toString()); } catch { return; }
    if (m.t === "hello") {
      ws.name = String(m.name ?? "SPIELER").slice(0, 12);
      ws.color = typeof m.color === "number" ? m.color : 0x22ff55;
      broadcast({ t: "hello", id: ws.pid, name: ws.name, color: ws.color, team: ws.team }, ws);
      for (const c of room.clients) {
        if (c !== ws && c.name) {
          ws.send(JSON.stringify({ t: "hello", id: c.pid, name: c.name, color: c.color, team: c.team }));
        }
      }
      ws.send(JSON.stringify({ t: "you", id: ws.pid, team: ws.team }));
    } else if (m.t === "state") {
      posMap.set(ws.pid, { x: m.x, z: m.z });
      m.id = ws.pid;
      broadcast(m, ws);
    } else if (m.t === "chat") {
      const msg = { t: "chat", id: ws.pid, name: ws.name ?? "?", text: String(m.text ?? "").slice(0, 120), teamChat: !!m.teamChat, team: ws.team };
      for (const c of room.clients) {
        if (c.readyState !== 1) continue;
        if (msg.teamChat && c.team !== ws.team && c !== ws) continue;
        c.send(JSON.stringify(msg));
      }
      return;
    } else if (m.t === "score") {
      const s = loadScores();
      const mode = String(m.mode ?? "ffa");
      s[mode] = s[mode] ?? [];
      const entry = { name: String(m.name ?? "?").slice(0, 16), kills: Number(m.kills) || 0, date: Date.now(), season: String(m.season ?? seasonNow()) };
      s[mode].push(entry);
      const cm = entry.name.match(/^\[([^\]]+)\]/);
      if (cm) {
        s.clans = s.clans ?? [];
        let c = s.clans.find((x) => x.tag === cm[1] && x.season === entry.season);
        if (!c) { c = { tag: cm[1], kills: 0, season: entry.season }; s.clans.push(c); }
        c.kills += entry.kills;
        s.clans.sort((a, b) => b.kills - a.kills);
        s.clans = s.clans.slice(0, 100);
      }
      s[mode].sort((a, b) => b.kills - a.kills);
      s[mode] = s[mode].slice(0, 100);
      saveScores(s);
      ws.send(JSON.stringify({ t: "top", mode, top: topFor(mode) }));
      return;
    } else if (m.t === "heat") {
      const s = loadScores();
      s.heat = s.heat ?? {};
      const key = `${String(m.map ?? "sector")}`;
      s.heat[key] = s.heat[key] ?? {};
      for (const [cx, cz, n] of m.cells ?? []) {
        const k = `${cx},${cz}`;
        s.heat[key][k] = (s.heat[key][k] ?? 0) + n;
      }
      saveScores(s);
      return;
    } else if (m.t === "heattop") {
      const s = loadScores();
      const cells = Object.entries((s.heat ?? {})[String(m.map ?? "sector")] ?? {}).map(([k, n]) => {
        const [x, z] = k.split(",").map(Number);
        return [x, z, n];
      }).sort((a, b) => b[2] - a[2]).slice(0, 60);
      ws.send(JSON.stringify({ t: "heattop", cells }));
      return;
    } else if (m.t === "clantop") {
      const s = loadScores();
      ws.send(JSON.stringify({ t: "clantop", top: (s.clans ?? []).filter((e) => e.season === seasonNow()).slice(0, 10) }));
      return;
    } else if (m.t === "range") {
      const s = loadScores();
      s.range = s.range ?? [];
      s.range.push({ name: String(m.name ?? "?").slice(0, 16), acc: Number(m.acc) || 0, hs: Number(m.hs) || 0, season: String(m.season ?? seasonNow()), date: Date.now() });
      s.range.sort((a, b) => b.acc - a.acc);
      s.range = s.range.slice(0, 100);
      saveScores(s);
      ws.send(JSON.stringify({ t: "rangetop", top: (s.range ?? []).filter((e) => e.season === seasonNow()).slice(0, 10) }));
      return;
    } else if (m.t === "rangetop") {
      const s = loadScores();
      ws.send(JSON.stringify({ t: "rangetop", top: (s.range ?? []).filter((e) => e.season === seasonNow()).slice(0, 10) }));
      return;
    } else if (m.t === "top") {
      ws.send(JSON.stringify({ t: "top", mode: m.mode, top: topFor(String(m.mode ?? "ffa")) }));
      return;
    } else if (m.t === "vc-join-req") {
      const others = [...room.clients].filter((c) => c !== ws && c.readyState === 1).map((c) => c.pid);
      ws.send(JSON.stringify({ t: "vc-hello", id: ws.pid, others }));
      return;
    } else if (m.t === "vc-teamchange") {
      for (const c of room.clients) {
        if (c !== ws && c.readyState === 1) c.send(JSON.stringify({ ...m, from: ws.pid }));
      }
      return;
    } else if (m.t === "vc-bye-broadcast") {
      for (const c of room.clients) {
        if (c !== ws && c.readyState === 1) c.send(JSON.stringify({ t: "vc-bye", from: ws.pid }));
      }
      return;
    } else if (m.t && m.t.startsWith("vc-")) {
      // Voice-Signaling (WebRTC): zielgerichtet an einen Client relayen
      const target = [...room.clients].find((c) => c.pid === m.to);
      if (target && target.readyState === 1) {
        target.send(JSON.stringify({ ...m, from: ws.pid }));
      }
      return;
    } else if (m.t === "ping") {
      ws.send(JSON.stringify({ t: "pong", ts: m.ts }));
      return;
    } else if (m.t === "hit") {
      const v = validateHit(ws, m);
      if (!v.ok) {
        ws.warns = (ws.warns || 0) + 1;
        ws.send(JSON.stringify({ t: "warn", reason: v.reason, warns: ws.warns }));
        return; // Treffer wird NICHT verteilt – server-authoritativ
      }
      m.id = ws.pid;
      broadcast(m, ws);
    } else {
      m.id = ws.pid;
      broadcast(m, ws);
    }
  });

  ws.on("close", () => {
    room.clients.delete(ws);
    broadcast({ t: "leave", id: ws.pid });
  });
});

server.on("upgrade", (req, socket, head) => {
  const url = req.url || "";
  if (url.startsWith("/ws")) {
    const roomKey = url.includes("mode=tdm") ? "tdm" : "ffa";
    wss.handleUpgrade(req, socket, head, (ws) => wss.emit("connection", ws, req, roomKey));
  } else {
    socket.destroy();
  }
});

server.listen(port, "0.0.0.0", () => {
  console.log(`WIRRWARR online: http://0.0.0.0:${port} (WS /ws, Räume: ffa|tdm)`);
});
