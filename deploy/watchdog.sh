#!/bin/bash
# WIRRWARR Watchdog: Healthcheck + Auto-Restart (per Cron oder Loop nutzen)
# Cron-Beispiel: * * * * * /path/to/deploy/watchdog.sh
if ! curl -sf -m 5 http://127.0.0.1:3000/health > /dev/null; then
  echo "[watchdog] $(date) – Server nicht erreichbar, restart …" >> /tmp/wirrwarr-watchdog.log
  pm2 restart wirrwarr || (cd "$(dirname "$0")/.." && pm2 start deploy/ecosystem.config.cjs)
fi
