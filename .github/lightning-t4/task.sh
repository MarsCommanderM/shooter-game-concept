#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="${HOME}"
BUILD="${ROOT}/stw-o3de-build/linux/bin/profile"
PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
ENGINE="${ROOT}/o3de-2605"
GATE="${ROOT}/stw-o3de-gate"
ICD="${GATE}/nvidia_icd.json"

echo "STW LIGHTNING READ-ONLY RESUME CHECK"
echo "HOST: $(hostname)"
echo "TIME: $(date --iso-8601=seconds)"
echo "UID/GID: $(id -u)/$(id -g)"
echo "RUNNER:"
pgrep -a -f 'Runner.Listener|Runner.Worker|run.sh' || true
echo "GPU:"
nvidia-smi --query-gpu=name,driver_version,memory.total,memory.used,utilization.gpu --format=csv,noheader
echo "NVIDIA DEVICES:"
ls -l /dev/nvidia* 2>/dev/null || true
echo "VULKAN:"
VK_ICD_FILENAMES="${ICD}" vulkaninfo --summary 2>&1 | grep -E 'deviceName|driverVersion|deviceType|apiVersion' | sed -n '1,80p'
echo "DISPLAY:"
printf 'DISPLAY=%s\n' "${DISPLAY:-UNSET}"
for display in :0 :1 :97 :99; do
  if DISPLAY="${display}" xdpyinfo >/dev/null 2>&1; then
    echo "${display}: AVAILABLE"
    DISPLAY="${display}" xdpyinfo 2>/dev/null | grep -E 'dimensions:|vendor string:' | head -n 4 || true
  else
    echo "${display}: unavailable"
  fi
done
echo "ARTIFACTS:"
for file in "${BUILD}/STW.GameLauncher" "${BUILD}/Editor" "${BUILD}/AssetProcessorBatch"; do
  if [[ -e "${file}" ]]; then
    stat -c '%n | %s bytes | %y | executable=%A' "${file}"
  else
    echo "MISSING: ${file}"
  fi
done
echo "DEPENDENCIES:"
ldd "${BUILD}/STW.GameLauncher" 2>&1 | grep 'not found' || echo "STW.GameLauncher dependencies resolved"
echo "ASSET CACHE:"
if [[ -d "${PROJECT}/Cache/linux" ]]; then
  du -sh "${PROJECT}/Cache/linux"
  find "${PROJECT}/Cache/linux" -type f | wc -l
else
  echo "MISSING"
fi
echo "CURRENT GAME/EDITOR/AP PROCESSES:"
pgrep -a -f 'STW.GameLauncher|/Editor|AssetProcessorBatch' || true
echo "X11/XVFB PROCESSES:"
pgrep -a -f 'Xorg|Xvfb' || true
echo "RECENT GATE OUTPUTS:"
find "${GATE}" -maxdepth 3 -type f -printf '%TY-%Tm-%Td %TH:%TM:%TS %s %p\n' 2>/dev/null | sort -r | head -n 40 || true
echo "RECENT PROJECT LOGS:"
find "${PROJECT}" -maxdepth 5 -type f \( -iname '*.log' -o -iname '*.png' \) -printf '%TY-%Tm-%Td %TH:%TM:%TS %s %p\n' 2>/dev/null | sort -r | head -n 40 || true
echo "DISK/RAM:"
df -h "${ROOT}" "${BUILD}" "${PROJECT}" 2>/dev/null || true
free -h
echo "PINS:"
echo "PRODUCTION_SOURCE_HEAD=d8adba3627911c29db8c9daacdc8793505fd5c4f"
if git -C "${ENGINE}" rev-parse HEAD >/dev/null 2>&1; then
  echo "O3DE_HEAD=$(git -C "${ENGINE}" rev-parse HEAD)"
else
  echo "O3DE_HEAD=UNAVAILABLE_GIT_METADATA"
fi
echo "READ-ONLY CHECK COMPLETE"
echo "COST INCURRED: \$0.00"
