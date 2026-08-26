#!/usr/bin/env bash
set -Eeuo pipefail
RUN_DIR="/teamspace/studios/this_studio/stw-o3de-gate/visual-gate-20260826T085208Z"
PROJECT="/teamspace/studios/this_studio/stw-o3de-worktree/stw-o3de/Project"
FRAME="${RUN_DIR}/stw-gamelauncher-x11.png"
THUMB="${RUN_DIR}/stw-gamelauncher-x11-thumb.jpg"
echo "READ-ONLY CORRELATED VISUAL EVIDENCE EXPORT"
[[ -s "${FRAME}" ]]
python3 - "${FRAME}" "${THUMB}" <<'PY'
from PIL import Image
import sys,os
with Image.open(sys.argv[1]) as im:
    rgb=im.convert("RGB")
    thumb=rgb.copy()
    thumb.thumbnail((640,360))
    thumb.save(sys.argv[2],"JPEG",quality=82,optimize=True)
    print("SOURCE_FORMAT=",im.format)
    print("SOURCE_RESOLUTION=",f"{im.width}x{im.height}")
    print("SOURCE_SIZE=",os.path.getsize(sys.argv[1]))
    print("THUMB_SIZE=",os.path.getsize(sys.argv[2]))
PY
echo "LAUNCH LOG FOCUS:"
grep -Ein 'Atom|RHI|Vulkan|Tesla T4|NVIDIA|defaultlevel|LoadLevel|level|spawnable|camera|Null Audio|Startup Error|error|failed|warning' "${RUN_DIR}/launcher.log" | sed -n '1,600p' || true
echo "GAME LOG:"
if [[ -f "${PROJECT}/user/log/Game.log" ]]; then sed -n '1,500p' "${PROJECT}/user/log/Game.log"; fi
echo "FRAME_BASE64_BEGIN"
base64 -w0 "${THUMB}"
echo
echo "FRAME_BASE64_END"
echo "BUILD_REPEATED=NO"
echo "ASSET_BUILD_REPEATED=NO"
echo "SOURCE_FILES_CHANGED=NO"
echo "COST INCURRED: \$0.00"
