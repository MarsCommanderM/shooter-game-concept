#!/usr/bin/env bash
set -Eeuo pipefail

readonly STATE="${HOME}/stw-o3de-gate"
readonly STDIO="${STATE}/logs/game-launcher-native.stdout-stderr.log"
readonly FRAME="${STATE}/stw-gamelauncher-native.png"

echo "STW O3DE SAVED VISUAL-GATE DIAGNOSTIC"
echo "NO BUILD"
echo "NO LAUNCH"
echo "NO SOURCE CHANGE"
[[ -s "${STDIO}" ]] || { echo "FIRST REAL ERROR: saved launcher log missing"; exit 1; }
echo "LAUNCHER LOG: ${STDIO}"
stat -c "LOG METADATA: %s bytes | %y" "${STDIO}"
echo "FIRST 240 LOG LINES:"
sed -n "1,240p" "${STDIO}"
echo "ERROR CONTEXT:"
grep -nEi -C 8 "error|failed|failure|secure directory|ALSA|level|scene|spawnable|vulkan|tesla|nvidia|rhi|atom" "${STDIO}" | sed -n "1,800p" || true
if [[ -s "${FRAME}" ]]; then
  stat -c "FRAME METADATA: %s bytes | %y" "${FRAME}"
  echo "FRAME_ORIGINAL_BASE64_BEGIN"
  base64 -w0 "${FRAME}"
  printf "\n"
  echo "FRAME_ORIGINAL_BASE64_END"
fi
echo "COST INCURRED: $0.00"
