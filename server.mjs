/* WIRRWARR Online-Server: Next-Handler + WebSocket auf Port 3000 (/ws) */
/* Räume: /ws?mode=ffa (Standard) & /ws?mode=tdm (Teams werden vergeben)  */
import { createServer } from "http";
import next from "next";
import { WebSocketServer } from "ws";

const port = 3000;
const app = next({ dev: false, hostname: "0.0.0.0", port });
const handle = app.getRequestHandler();
await app.prepare();

const server = createServer((req, res) => handle(req, res));
const wss = new WebSocketServer({ noServer: true });

const rooms = new Map();
const getRoom = (key) => {
  let r = rooms.get(key);
  if (!r) { r = { clients: new Set(), teamCounter: 0 }; rooms.set(key, r); }
  return r;
};

let nextId = 1;

wss.on("connection", (ws, req, roomKey) => {
  const room = getRoom(roomKey);
  room.clients.add(ws);
  ws.pid = nextId++;
  ws.roomKey = roomKey;
  ws.team = roomKey === "tdm" ? room.teamCounter++ % 2 : ws.pid;

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
