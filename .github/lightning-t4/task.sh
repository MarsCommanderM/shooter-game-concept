#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="${HOME}"
STATE="${ROOT}/stw-o3de-gate"
echo "READ-ONLY XVFB FAILURE EVIDENCE"
latest="$(find "${STATE}" -maxdepth 1 -type d -name 'visual-gate-*' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n1 | cut -d' ' -f2-)"
echo "LATEST_GATE=${latest:-NONE}"
if [[ -n "${latest:-}" ]]; then
  for f in "${latest}/report.txt" "${latest}/xvfb.log" "${latest}/launcher.log"; do
    echo "===== ${f} ====="
    if [[ -f "${f}" ]]; then
      stat -c '%n | %s bytes | %y | %A | %U:%G' "${f}"
      sed -n '1,500p' "${f}"
    else
      echo "MISSING"
    fi
  done
fi
echo "XVFB BINARY:"
command -v Xvfb || true
Xvfb -version 2>&1 | sed -n '1,80p' || true
echo "XDPYINFO BINARY:"
command -v xdpyinfo || true
xdpyinfo -version 2>&1 | sed -n '1,40p' || true
echo "X11 SOCKETS:"
ls -la /tmp/.X11-unix 2>/dev/null || true
echo "COST INCURRED: \$0.00"
