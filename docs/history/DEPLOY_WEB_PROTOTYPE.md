# WIRRWARR – Deployment auf deinem eigenen Server

Der Online-Modus (`/online`) braucht den Custom-Server `server.mjs`
(Next.js-Handler **und** WebSocket `/ws` auf Port 3000).
Auf Serverless-Hosts (v0/Vercel) läuft nur die Website + `/play` – **Online braucht deinen Server**.

## 1) Einmalig einrichten

```bash
# Repo holen
git clone https://github.com/MarsCommanderM/shooter-game-concept.git
cd shooter-game-concept

# Node 20+ vorausgesetzt
npm ci            # falls ci meckert (lockfile pnpm): npm install
npm run build     # Next-Production-Build

# PM2 (Prozess-Manager, Auto-Restart)
npm i -g pm2
pm2 start deploy/ecosystem.config.cjs
pm2 save
```

## 2) Nginx als Reverse Proxy (Domain + WebSocket)

```bash
sudo cp deploy/nginx.conf /etc/nginx/sites-available/wirrwarr
# Domain anpassen, dann:
sudo ln -s /etc/nginx/sites-available/wirrwarr /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx

# HTTPS + WSS kostenlos:
sudo apt install certbot python3-certbot-nginx
sudo certbot --nginx -d wirrwarr.DEINE-DOMAIN.de
```

Der Client verbindet sich auf `wss://DEINE-DOMAIN/ws` **automatisch same-origin** –
keine Client-Änderung nötig, die Nginx-Config mit den `Upgrade`-Headern macht WSS draus.

## 3) Updates ziehen

```bash
cd shooter-game-concept
git pull
npm run build
pm2 restart wirrwarr
```

## 4) Quick-Checks

```bash
pm2 status
pm2 logs wirrwarr          # "WIRRWARR online: http://0.0.0.0:3000 (WS /ws, Räume: ffa|tdm)"
curl -I https://DEINE-DOMAIN.de/online
```

Danach: `/online` im Browser öffnen (zwei Geräte/Tabs) → FFA oder TDM spielen. 🎮

## Hinweise

- **Eine World pro Prozess**: `instances: 1` bewusst – Spieler teilen sich eine Arena.
  Mehr Welten = mehrere Prozesse auf anderen Ports + eigenes Routing (Roadmap).
- **Anti-Cheat-Grundschutz** ist serverseitig aktiv (Hit-Validation: Damage, Rate, Reichweite).
- Logs für Verhaltensdetektion liegen in `pm2 logs` – Basis für späteres Report-System.
