#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="${HOME}"
STATE="${ROOT}/stw-o3de-gate"
LOGS="${STATE}/logs"
ENGINE="${ROOT}/o3de-2605"
PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
BIN="${ROOT}/stw-o3de-build/linux/bin/profile"
LAUNCHER="${BIN}/STW.GameLauncher"
ICD="${STATE}/nvidia_icd.json"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="${STATE}/visual-gate-${RUN_ID}"
RUNTIME="${RUN_DIR}/xdg-runtime"
ALSA="${RUNTIME}/asound-null.conf"
LAUNCH_LOG="${RUN_DIR}/launcher.log"
XVFB_LOG="${RUN_DIR}/xvfb.log"
FRAME="${RUN_DIR}/stw-gamelauncher-x11.png"
REPORT="${RUN_DIR}/report.txt"
xvfb_pid=""
launcher_pid=""
cleanup(){
  if [[ -n "${launcher_pid}" ]] && kill -0 "${launcher_pid}" 2>/dev/null; then
    kill -TERM -- "-${launcher_pid}" 2>/dev/null || kill -TERM "${launcher_pid}" 2>/dev/null || true
    for _ in {1..20}; do kill -0 "${launcher_pid}" 2>/dev/null || break; sleep .25; done
    kill -KILL -- "-${launcher_pid}" 2>/dev/null || true
  fi
  if [[ -n "${xvfb_pid}" ]] && kill -0 "${xvfb_pid}" 2>/dev/null; then
    kill -TERM "${xvfb_pid}" 2>/dev/null || true
    wait "${xvfb_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM
mkdir -p "${RUN_DIR}" "${RUNTIME}" "${LOGS}"
chmod 700 "${RUNTIME}"
exec > >(tee "${REPORT}") 2>&1

echo "STW O3DE EXISTING-BINARY NATIVE FRAME GATE"
echo "RUN_ID=${RUN_ID}"
echo "NO BUILD / NO CMAKE / NO ASSET PROCESSING / NO INSTALL / NO SOURCE CHANGE"

echo "CAPTURE CAPABILITY INVENTORY:"
echo "PROJECT GEMS:"
python3 - "${PROJECT}/project.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]))
print("project_name=",p.get("project_name"))
for k in ("gem_names","enabled_gems","external_subdirectories"):
    if k in p: print(k,"=",p[k])
PY
echo "FRAMECAPTURE SOURCE:"
find "${ENGINE}/Gems/Atom" -type f \( -iname '*FrameCapture*' -o -iname '*Screenshot*' \) -print 2>/dev/null | sort | sed -n '1,160p'
echo "FRAMECAPTURE API:"
grep -RInE 'CaptureScreenshot|CaptureScreenshotForWindow|CaptureScreenshotWithPreview|CapturePassAttachment|FrameCaptureRequestBus'   "${ENGINE}/Gems/Atom" --include='*.h' --include='*.cpp' --include='*.inl' 2>/dev/null | sed -n '1,260p' || true
echo "SCRIPT AUTOMATION SOURCE/BUILD/ENABLEMENT:"
find "${ENGINE}/Gems/ScriptAutomation" -maxdepth 3 -type f \( -name 'gem.json' -o -iname '*Screenshot*.lua' \) -print 2>/dev/null | sort | sed -n '1,120p' || true
find "${BIN}" -maxdepth 2 -type f -iname '*ScriptAutomation*' -printf '%p\n' 2>/dev/null | sort || true
grep -RIn 'ScriptAutomation' "${PROJECT}/project.json" "${PROJECT}/Registry" "${BIN}/Registry" 2>/dev/null | sed -n '1,160p' || true
echo "CAPTURE TOOLS:"
for t in xwd import ffmpeg maim scrot grim gnome-screenshot; do printf '%s=' "${t}"; command -v "${t}" || echo MISSING; done

[[ -x "${LAUNCHER}" ]] || { echo "FIRST REAL BLOCKER: GameLauncher missing"; exit 1; }
[[ -s "${ICD}" ]] || { echo "FIRST REAL BLOCKER: NVIDIA ICD missing"; exit 1; }
ldd "${LAUNCHER}" | grep 'not found' && { echo "FIRST REAL BLOCKER: unresolved launcher dependency"; exit 1; } || true

echo "GPU PRECHECK:"
nvidia-smi --query-gpu=name,driver_version,memory.total,memory.used,utilization.gpu --format=csv,noheader
echo "VULKAN PRECHECK:"
VK_ICD_FILENAMES="${ICD}" vulkaninfo --summary 2>&1 | grep -E 'apiVersion|driverVersion|deviceType|deviceName' | sed -n '1,80p'
VK_ICD_FILENAMES="${ICD}" vulkaninfo --summary 2>&1 | grep -q 'Tesla T4' || { echo "FIRST REAL BLOCKER: Vulkan Tesla T4 missing"; exit 1; }

display_number=""
for n in $(seq 90 120); do
  if [[ ! -S "/tmp/.X11-unix/X${n}" ]]; then display_number="${n}"; break; fi
done
[[ -n "${display_number}" ]] || { echo "FIRST REAL BLOCKER: no unused X display 90-120"; exit 1; }
DISPLAY_VALUE=":${display_number}"
Xvfb "${DISPLAY_VALUE}" -screen 0 1280x720x24 -nolisten tcp -noreset >"${XVFB_LOG}" 2>&1 &
xvfb_pid=$!
for _ in {1..40}; do
  [[ -S "/tmp/.X11-unix/X${display_number}" ]] && break
  kill -0 "${xvfb_pid}" 2>/dev/null || { echo "FIRST REAL BLOCKER: Xvfb exited"; exit 1; }
  sleep .25
done
DISPLAY="${DISPLAY_VALUE}" python3 - <<'PY'
import ctypes, os
from ctypes import c_char_p, c_int, c_void_p
x=ctypes.CDLL("libX11.so.6")
x.XOpenDisplay.argtypes=[c_char_p]; x.XOpenDisplay.restype=c_void_p
x.XDefaultScreen.argtypes=[c_void_p]; x.XDefaultScreen.restype=c_int
x.XDisplayWidth.argtypes=[c_void_p,c_int]; x.XDisplayWidth.restype=c_int
x.XDisplayHeight.argtypes=[c_void_p,c_int]; x.XDisplayHeight.restype=c_int
x.XCloseDisplay.argtypes=[c_void_p]
d=x.XOpenDisplay(os.environ["DISPLAY"].encode())
if not d: raise SystemExit("XOpenDisplay failed")
s=x.XDefaultScreen(d)
print(f"X11_DISPLAY_OK={os.environ['DISPLAY']}")
print(f"X11_DIMENSIONS={x.XDisplayWidth(d,s)}x{x.XDisplayHeight(d,s)}")
x.XCloseDisplay(d)
PY
echo "DISPLAY=${DISPLAY_VALUE}"
echo "XVFB_PID=${xvfb_pid}"

cat >"${ALSA}" <<'EOF'
pcm.!default {
    type null
    hint { show on description "STW headless null output" }
}
EOF
chmod 600 "${ALSA}"

launch_epoch="$(date +%s)"
echo "LAUNCH_TIMESTAMP=$(date -u --iso-8601=seconds)"
echo "LAUNCH_COMMAND=DISPLAY=${DISPLAY_VALUE} XDG_RUNTIME_DIR=${RUNTIME} ALSA_CONFIG_PATH=${ALSA} VK_ICD_FILENAMES=${ICD} LD_LIBRARY_PATH=${BIN}:<existing> ${LAUNCHER} --project-path=${PROJECT} --engine-path=${ENGINE} -sys_audio_disable 1"
setsid env DISPLAY="${DISPLAY_VALUE}" XDG_RUNTIME_DIR="${RUNTIME}" ALSA_CONFIG_PATH="${ALSA}"   VK_ICD_FILENAMES="${ICD}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"   "${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${ENGINE}" -sys_audio_disable 1   >"${LAUNCH_LOG}" 2>&1 &
launcher_pid=$!
echo "LAUNCHER_PID=${launcher_pid}"

ready=0
for second in $(seq 1 90); do
  if ! kill -0 "${launcher_pid}" 2>/dev/null; then
    echo "FIRST REAL BLOCKER: GameLauncher exited before capture"
    tail -n 260 "${LAUNCH_LOG}"
    exit 1
  fi
  if grep -Eqi 'Tesla T4|NVIDIA.*T4' "${LAUNCH_LOG}" &&
     grep -Eqi 'Vulkan' "${LAUNCH_LOG}" &&
     grep -Eqi 'defaultlevel|level.*loaded|LoadLevel|spawnable' "${LAUNCH_LOG}"; then
    ready=1
    [[ "${second}" -ge 25 ]] && break
  fi
  sleep 1
done
echo "READY_EVIDENCE=${ready}"
echo "ATOM_RHI_EVIDENCE:"
grep -Ein 'Atom|RHI|Vulkan|adapter|physical device|Tesla T4|NVIDIA|device name|renderer|defaultlevel|LoadLevel|spawnable|Null Audio|Startup Error|error|failed' "${LAUNCH_LOG}" | tail -n 320 || true

grep -Eqi 'Tesla T4|NVIDIA.*T4' "${LAUNCH_LOG}" || { echo "FIRST REAL BLOCKER: launcher log lacks Tesla T4"; exit 1; }
grep -Eqi 'Vulkan' "${LAUNCH_LOG}" || { echo "FIRST REAL BLOCKER: launcher log lacks Vulkan"; exit 1; }
if grep -Eqi 'selected.*(llvmpipe|lavapipe|software)|adapter.*(llvmpipe|lavapipe)' "${LAUNCH_LOG}"; then
  echo "FIRST REAL BLOCKER: software renderer selected"; exit 1
fi

capture_timestamp="$(date -u --iso-8601=seconds)"
DISPLAY="${DISPLAY_VALUE}" python3 - "${FRAME}" <<'PY'
import ctypes, os, sys, math
from ctypes import *
from PIL import Image, ImageStat
class XImage(Structure):
    _fields_=[("width",c_int),("height",c_int),("xoffset",c_int),("format",c_int),("data",c_void_p),
              ("byte_order",c_int),("bitmap_unit",c_int),("bitmap_bit_order",c_int),("bitmap_pad",c_int),
              ("depth",c_int),("bytes_per_line",c_int),("bits_per_pixel",c_int),
              ("red_mask",c_ulong),("green_mask",c_ulong),("blue_mask",c_ulong)]
x=CDLL("libX11.so.6")
x.XOpenDisplay.argtypes=[c_char_p]; x.XOpenDisplay.restype=c_void_p
x.XDefaultScreen.argtypes=[c_void_p]; x.XDefaultScreen.restype=c_int
x.XRootWindow.argtypes=[c_void_p,c_int]; x.XRootWindow.restype=c_ulong
x.XDisplayWidth.argtypes=[c_void_p,c_int]; x.XDisplayWidth.restype=c_int
x.XDisplayHeight.argtypes=[c_void_p,c_int]; x.XDisplayHeight.restype=c_int
x.XGetImage.argtypes=[c_void_p,c_ulong,c_int,c_int,c_uint,c_uint,c_ulong,c_int]; x.XGetImage.restype=POINTER(XImage)
x.XSync.argtypes=[c_void_p,c_int]
d=x.XOpenDisplay(os.environ["DISPLAY"].encode())
if not d: raise SystemExit("XOpenDisplay failed")
s=x.XDefaultScreen(d); root=x.XRootWindow(d,s)
w=x.XDisplayWidth(d,s); h=x.XDisplayHeight(d,s)
x.XSync(d,0)
p=x.XGetImage(d,root,0,0,w,h,c_ulong(-1).value,2)
if not p: raise SystemExit("XGetImage root failed")
xi=p.contents; raw=string_at(xi.data,xi.bytes_per_line*h)
if xi.bits_per_pixel==32 and xi.byte_order==0:
    img=Image.frombytes("RGB",(w,h),raw,"raw","BGRX",xi.bytes_per_line,1)
elif xi.bits_per_pixel==24 and xi.byte_order==0:
    img=Image.frombytes("RGB",(w,h),raw,"raw","BGR",xi.bytes_per_line,1)
else: raise SystemExit(f"unsupported XImage {xi.bits_per_pixel}/{xi.byte_order}")
img.save(sys.argv[1],"PNG")
pixels=list(img.getdata())
unique=len(set(pixels))
lums=[0.2126*r+0.7152*g+0.0722*b for r,g,b in pixels]
mean=sum(lums)/len(lums)
var=sum((v-mean)**2 for v in lums)/len(lums)
near=sum(1 for v in lums if v<8.0)*100.0/len(lums)
print(f"FRAME_RESOLUTION={w}x{h}")
print(f"FRAME_SIZE={os.path.getsize(sys.argv[1])}")
print(f"UNIQUE_COLORS={unique}")
print(f"PIXEL_VARIANCE={var:.6f}")
print(f"MEAN_LUMINANCE={mean:.6f}")
print(f"MIN_LUMINANCE={min(lums):.6f}")
print(f"MAX_LUMINANCE={max(lums):.6f}")
print(f"NEAR_BLACK_PERCENT={near:.6f}")
print(f"FRAME_VALID={'YES' if unique>16 and var>4.0 and near<99.0 else 'NO'}")
PY
echo "CAPTURE_TIMESTAMP=${capture_timestamp}"
stat -c 'FRAME_PATH=%n%nFRAME_BYTES=%s%nFRAME_TIMESTAMP=%y' "${FRAME}"
echo "CORRELATION=launcher_pid:${launcher_pid};launch_epoch:${launch_epoch};capture:${capture_timestamp};display:${DISPLAY_VALUE}"
echo "LAUNCH_LOG=${LAUNCH_LOG}"
echo "XVFB_LOG=${XVFB_LOG}"
echo "BUILD_REPEATED=NO"
echo "ASSET_BUILD_REPEATED=NO"
echo "SOURCE_FILES_CHANGED=NO"
echo "COST INCURRED: \$0.00"
