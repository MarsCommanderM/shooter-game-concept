#!/bin/bash
# WIRRWARR One-Command-Setup für Ubuntu/Debian (Contabo VPS)
# Nutzung:  bash deploy/setup.sh
set -e
export DEBIAN_FRONTEND=noninteractive
SUDO=""
if [ "$(id -u)" != "0" ]; then SUDO="sudo"; fi
echo "🎮 WIRRWARR-Setup startet …"

# 1) Node 20 sicherstellen
NEED_NODE=1
if command -v node >/dev/null 2>&1; then
  MAJOR=$(node -v | sed 's/v//' | cut -d. -f1)
  if [ "$MAJOR" -ge 20 ]; then NEED_NODE=0; fi
fi
if [ "$NEED_NODE" = "1" ]; then
  echo "📦 Installiere Node 20 …"
  if [ -n "$SUDO" ]; then curl -fsSL https://deb.nodesource.com/setup_20.x | $SUDO bash -; else curl -fsSL https://deb.nodesource.com/setup_20.x | bash -; fi
  $SUDO apt-get install -y nodejs
fi

# 2) Projekt bauen
cd "$(dirname "$0")/.."
echo "📦 npm install …"
npm install --no-audit --no-fund
echo "🏗️  Production-Build …"
npm run build

# 3) PM2 installieren + Server starten
echo "🚀 Starte Server mit PM2 …"
$SUDO npm i -g pm2 >/dev/null 2>&1 || npm i -g pm2
pm2 delete wirrwarr >/dev/null 2>&1 || true
pm2 start deploy/ecosystem.config.cjs
pm2 save >/dev/null 2>&1 || true
pm2 install pm2-logrotate >/dev/null 2>&1 || true
pm2 set pm2-logrotate:max_size 10M >/dev/null 2>&1 || true
pm2 set pm2-logrotate:retain 7 >/dev/null 2>&1 || true

# Watchdog per Cron: jede Minute Healthcheck + Auto-Restart
CRON_LINE="* * * * * $(pwd)/deploy/watchdog.sh"
( crontab -l 2>/dev/null | grep -v "wirrwarr-watchdog" ; echo "$CRON_LINE # wirrwarr-watchdog" ) | crontab - 2>/dev/null || true

# 4) Firewall: Port 3000 freigeben (falls ufw aktiv)
if command -v ufw >/dev/null 2>&1; then
  $SUDO ufw allow 3000/tcp >/dev/null 2>&1 || true
fi

IP=$(hostname -I | awk '{print $1}')
echo ""
echo "✅ FERTIG! Dein Spiel läuft jetzt auf:"
echo ""
echo "      👉  http://$IP:3000        (Website + /play)"
echo "      👉  http://$IP:3000/online (Multiplayer – schick den Link an Freunde!)"
echo ""
echo "Logs:      pm2 logs wirrwarr"
echo "Restart:   pm2 restart wirrwarr"
echo "Update:    git pull && npm run build && pm2 restart wirrwarr"
