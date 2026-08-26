#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="${HOME}"; STATE="${ROOT}/stw-o3de-gate"; LOGS="${STATE}/logs"
O3DE="${ROOT}/o3de-2605"; PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
BIN="${ROOT}/stw-o3de-build/linux/bin/profile"; LAUNCHER="${BIN}/STW.GameLauncher"
ICD="${STATE}/nvidia_icd.json"; RUNTIME="${STATE}/xdg-runtime-visual-gate-window"
ALSA="${RUNTIME}/asound-null.conf"; STDIO="${LOGS}/game-launcher-window.stdout-stderr.log"
FRAME="${STATE}/stw-gamelauncher-window.png"; DISPLAY_NUM=":97"
xvfb_pid=""; launcher_pid=""
cleanup(){
  if [[ -n "${launcher_pid}" ]] && kill -0 "${launcher_pid}" 2>/dev/null; then
    kill -TERM -- "-${launcher_pid}" 2>/dev/null || kill -TERM "${launcher_pid}" 2>/dev/null || true
    sleep 1; kill -KILL -- "-${launcher_pid}" 2>/dev/null || true
  fi
  if [[ -n "${xvfb_pid}" ]] && kill -0 "${xvfb_pid}" 2>/dev/null; then kill -TERM "${xvfb_pid}" 2>/dev/null || true; fi
}
trap cleanup EXIT INT TERM
echo "STW O3DE NATIVE WINDOW CAPTURE DIAGNOSTIC"
echo "NO BUILD / NO CMAKE / NO ASSET PROCESSING / NO INSTALL / NO SOURCE CHANGE"
[[ -x "${LAUNCHER}" ]] || { echo "FIRST REAL ERROR: launcher missing"; exit 1; }
[[ -s "${ICD}" ]] || { echo "FIRST REAL ERROR: NVIDIA ICD missing"; exit 1; }
mkdir -p "${LOGS}" "${RUNTIME}"; chmod 700 "${RUNTIME}"
cat >"${ALSA}" <<'EOF'
pcm.!default {
    type null
    hint { show on description "STW headless null output" }
}
EOF
chmod 600 "${ALSA}"
nvidia-smi --query-gpu=name,driver_version --format=csv,noheader
VK_ICD_FILENAMES="${ICD}" vulkaninfo --summary 2>&1 | grep -E 'deviceName|driverName|driverInfo' | head -n 20
[[ ! -S /tmp/.X11-unix/X97 ]] || { echo "FIRST REAL ERROR: display 97 occupied"; exit 1; }
Xvfb "${DISPLAY_NUM}" -screen 0 1280x720x24 -nolisten tcp -noreset >"${LOGS}/xvfb-window.log" 2>&1 &
xvfb_pid=$!
for _ in {1..40}; do [[ -S /tmp/.X11-unix/X97 ]] && break; sleep .25; done
[[ -S /tmp/.X11-unix/X97 ]] || { echo "FIRST REAL ERROR: Xvfb unavailable"; exit 1; }
: >"${STDIO}"
setsid env DISPLAY="${DISPLAY_NUM}" XDG_RUNTIME_DIR="${RUNTIME}" ALSA_CONFIG_PATH="${ALSA}"   VK_ICD_FILENAMES="${ICD}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"   "${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${O3DE}" -sys_audio_disable 1   >"${STDIO}" 2>&1 &
launcher_pid=$!
echo "PID: ${launcher_pid}"
for _ in {1..30}; do
  kill -0 "${launcher_pid}" 2>/dev/null || { echo "FIRST REAL ERROR: launcher exited"; tail -n 250 "${STDIO}"; exit 1; }
  sleep 1
done
echo "X11 WINDOW INVENTORY AND DIRECT CAPTURE:"
DISPLAY="${DISPLAY_NUM}" python3 - "${FRAME}" <<'PY'
import ctypes, os, sys
from ctypes import *
from PIL import Image, ImageStat

class XImage(Structure):
    _fields_=[("width",c_int),("height",c_int),("xoffset",c_int),("format",c_int),("data",c_void_p),
              ("byte_order",c_int),("bitmap_unit",c_int),("bitmap_bit_order",c_int),("bitmap_pad",c_int),
              ("depth",c_int),("bytes_per_line",c_int),("bits_per_pixel",c_int),
              ("red_mask",c_ulong),("green_mask",c_ulong),("blue_mask",c_ulong)]
class XWindowAttributes(Structure):
    _fields_=[("x",c_int),("y",c_int),("width",c_int),("height",c_int),("border_width",c_int),("depth",c_int),
              ("visual",c_void_p),("root",c_ulong),("class_",c_int),("bit_gravity",c_int),("win_gravity",c_int),
              ("backing_store",c_int),("backing_planes",c_ulong),("backing_pixel",c_ulong),("save_under",c_int),
              ("colormap",c_ulong),("map_installed",c_int),("map_state",c_int),("all_event_masks",c_long),
              ("your_event_mask",c_long),("do_not_propagate_mask",c_long),("override_redirect",c_int),("screen",c_void_p)]
x=CDLL("libX11.so.6")
x.XOpenDisplay.argtypes=[c_char_p]; x.XOpenDisplay.restype=c_void_p
x.XDefaultRootWindow.argtypes=[c_void_p]; x.XDefaultRootWindow.restype=c_ulong
x.XQueryTree.argtypes=[c_void_p,c_ulong,POINTER(c_ulong),POINTER(c_ulong),POINTER(POINTER(c_ulong)),POINTER(c_uint)]
x.XQueryTree.restype=c_int
x.XGetWindowAttributes.argtypes=[c_void_p,c_ulong,POINTER(XWindowAttributes)]; x.XGetWindowAttributes.restype=c_int
x.XFetchName.argtypes=[c_void_p,c_ulong,POINTER(c_char_p)]; x.XFetchName.restype=c_int
x.XGetImage.argtypes=[c_void_p,c_ulong,c_int,c_int,c_uint,c_uint,c_ulong,c_int]; x.XGetImage.restype=POINTER(XImage)
x.XFree.argtypes=[c_void_p]
d=x.XOpenDisplay(os.environ["DISPLAY"].encode())
if not d: raise SystemExit("XOpenDisplay failed")
root=x.XDefaultRootWindow(d)
seen=set(); rows=[]
def walk(w,depth=0):
    if w in seen or depth>8:return
    seen.add(w); attr=XWindowAttributes()
    if not x.XGetWindowAttributes(d,w,byref(attr)):return
    namep=c_char_p(); name=""
    if x.XFetchName(d,w,byref(namep)) and namep.value:
        name=namep.value.decode(errors="replace"); x.XFree(namep)
    rows.append((w,depth,attr.width,attr.height,attr.depth,attr.map_state,name))
    rr=c_ulong(); parent=c_ulong(); kids=POINTER(c_ulong)(); count=c_uint()
    if x.XQueryTree(d,w,byref(rr),byref(parent),byref(kids),byref(count)):
        for i in range(count.value): walk(kids[i],depth+1)
        if kids: x.XFree(kids)
walk(root)
for row in rows: print("WINDOW",*row,sep="|")
candidates=[r for r in rows if r[0]!=root and r[5]==2 and r[2]>=320 and r[3]>=200]
if not candidates: raise SystemExit("no mapped launcher-sized X11 window")
target=max(candidates,key=lambda r:r[2]*r[3]); w,width,height=target[0],target[2],target[3]
im=x.XGetImage(d,w,0,0,width,height,c_ulong(-1).value,2)
if not im: raise SystemExit("XGetImage launcher window failed")
xi=im.contents; raw=string_at(xi.data,xi.bytes_per_line*height)
if xi.bits_per_pixel==32 and xi.byte_order==0: pic=Image.frombytes("RGB",(width,height),raw,"raw","BGRX",xi.bytes_per_line,1)
elif xi.bits_per_pixel==24 and xi.byte_order==0: pic=Image.frombytes("RGB",(width,height),raw,"raw","BGR",xi.bytes_per_line,1)
else: raise SystemExit(f"unsupported image format {xi.bits_per_pixel}/{xi.byte_order}")
pic.save(sys.argv[1],"PNG")
print("TARGET_WINDOW",target)
print("FRAME",pic.size,os.path.getsize(sys.argv[1]),pic.getextrema(),ImageStat.Stat(pic).mean,pic.entropy())
PY
echo "GAME LOG TAIL:"
tail -n 180 "${PROJECT}/user/log/Game.log"
echo "LAUNCH STDIO FOCUS:"
grep -Ein 'Null Audio|ALSA|Pulse|error|failed|warning|RHI|Vulkan|Tesla|level|spawnable|camera|viewport|window|swapchain|render' "${STDIO}" | tail -n 300 || true
stat -c 'FRAME: %n | %s bytes | %y' "${FRAME}"
echo "COST INCURRED: \$0.00"
