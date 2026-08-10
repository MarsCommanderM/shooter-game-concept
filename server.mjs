/* WIRRWARR Online-Server: Next-Handler + WebSocket auf Port 3000 (/ws) */
import { createServer } from "http";
import next from "next";
import { WebSocketServer } from "ws";

const port = 3000;
const app = next({ dev: false, hostname: "0.0.0.0", port });
const handle = app.getRequestHandler();
await app.prepare();

const server = createServer((req, res) => handle(req, res));
const wss = new WebSocketServer({ noServer: true });

let nextId = 1;

wss.on("connection", (ws) => {
  ws.pid = nextId++;
  const broadcast = (msg, except) => {
    const data = JSON.stringify(msg);
    for (const c of wss.clients) {
      if (c !== except && c.readyState === 1) c.send(data);
    }
  };
  broadcast({ t: "join", id: ws.pid });

  ws.on("message", (raw) => {
    let m;
    try { m = JSON.parse(raw.toString()); } catch { return; }
    if (m.t === "hello") {
      ws.name = String(m.name ?? "SPIELER").slice(0, 12);
      ws.color = typeof m.color === "number" ? m.color : 0x22ff55;
      broadcast({ t: "hello", id: ws.pid, name: ws.name, color: ws.color }, ws);
      for (const c of wss.clients) {
        if (c !== ws && c.name) {
          ws.send(JSON.stringify({ t: "hello", id: c.pid, name: c.name, color: c.color }));
        }
      }
      ws.send(JSON.stringify({ t: "you", id: ws.pid }));
    } else {
      m.id = ws.pid;
      broadcast(m, ws);
    }
  });

  ws.on("close", () => broadcast({ t: "leave", id: ws.pid }));
});

server.on("upgrade", (req, socket, head) => {
  if ((req.url || "").startsWith("/ws")) {
    wss.handleUpgrade(req, socket, head, (ws) => wss.emit("connection", ws, req));
  } else {
    socket.destroy();
  }
});

server.listen(port, "0.0.0.0", () => {
  console.log(`WIRRWARR online: http://0.0.0.0:${port} (WS auf /ws)`);
});
