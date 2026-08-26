#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
readonly CACHE="${PROJECT}/Cache/linux"
readonly STDIO="${LOGS}/game-launcher-native.stdout-stderr.log"
readonly FRAME="${STATE}/stw-gamelauncher-native.png"

echo "STW O3DE BLACK-FRAME READ-ONLY DIAGNOSTIC"
echo "NO BUILD"
echo "NO LAUNCH"
echo "NO ASSET PROCESSING"
echo "NO INSTALL"
echo "NO SOURCE CHANGE"
[[ -s "${STDIO}" ]] || { echo "FIRST REAL ERROR: launcher log missing"; exit 1; }
stat -c 'LAUNCHER LOG: %n | %s bytes | %y' "${STDIO}"
stat -c 'FRAME: %n | %s bytes | %y' "${FRAME}" 2>/dev/null || true

echo "FULL LAUNCHER STDOUT/STDERR:"
nl -ba "${STDIO}" | sed -n '1,700p'

echo "FOCUSED LEVEL/SCENE/RENDER CONTEXT:"
grep -nEi -C 10 'error|failed|warning|loadlevel|defaultlevel|spawnable|level|scene|camera|viewport|window|swapchain|render pipeline|pass system|rhi|vulkan|tesla|nvidia|null audiosystem' "${STDIO}" | sed -n '1,1200p' || true

echo "RECENT PROJECT RUNTIME LOGS:"
find "${PROJECT}/user/log" -maxdepth 2 -type f -mmin -30 -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort | sed -n '1,300p'
while IFS= read -r log; do
  echo "===== ${log} ====="
  tail -n 500 "${log}" 2>/dev/null || true
done < <(find "${PROJECT}/user/log" -maxdepth 1 -type f -mmin -30 -name '*.log' -print 2>/dev/null | sort)

echo "LEVEL CONFIG:"
grep -RInE 'LoadLevel|defaultlevel|spawnable' "${PROJECT}/Registry" "${PROJECT}/user/Registry" 2>/dev/null | sed -n '1,300p' || true
find "${CACHE}/levels/defaultlevel" -maxdepth 3 -type f -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort | sed -n '1,300p'

echo "FRAME PIXEL PROOF:"
python3 - "${FRAME}" <<'PY'
import sys
from PIL import Image, ImageStat
with Image.open(sys.argv[1]) as image:
    rgb=image.convert("RGB")
    print("format", image.format)
    print("size", image.size)
    print("extrema", rgb.getextrema())
    print("mean", ImageStat.Stat(rgb).mean)
    print("entropy", rgb.entropy())
PY
echo "COST INCURRED: \$0.00"
