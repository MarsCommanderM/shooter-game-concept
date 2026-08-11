#!/bin/bash
# WIRRWARR Remote-Installer – EINE Zeile, kein Paste-Chaos:
#   curl -sL https://raw.githubusercontent.com/MarsCommanderM/shooter-game-concept/main/deploy/remote-install.sh | bash
set -e
export DEBIAN_FRONTEND=noninteractive
echo "🎮 WIRRWARR Remote-Install startet …"

# Root-Check ohne sudo-Zwang
SUDO=""
if [ "$(id -u)" != "0" ]; then SUDO="sudo"; fi

echo "📦 Pakete …"
$SUDO apt-get update -y > /dev/null 2>&1 || true
$SUDO apt-get install -y git curl > /dev/null 2>&1

echo "📥 Repo …"
if [ ! -d /opt/wirrwarr/.git ]; then
  rm -rf /opt/wirrwarr
  git clone https://github.com/MarsCommanderM/shooter-game-concept.git /opt/wirrwarr
else
  cd /opt/wirrwarr && git pull || true
fi
cd /opt/wirrwarr

echo "🏗️  Setup (Node, Build, PM2, Watchdog) …"
bash deploy/setup.sh

echo ""
echo "✅ FERTIG! Öffne auf dem Handy (Safari):"
echo "   http://$(hostname -I | awk '{print $1}'):3000"
echo "   Teilen → 'Zum Home-Bildschirm' = App-Icon 🎮"
