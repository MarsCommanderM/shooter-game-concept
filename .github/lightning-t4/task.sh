#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="/teamspace/studios/this_studio"
ENGINE="${ROOT}/o3de-2605"
O3DE_ROOT="${ROOT}/stw-o3de-worktree/stw-o3de"
PROJECT="${O3DE_ROOT}/Project"
GEM="${O3DE_ROOT}/Gems/STWGameplay"
BUILD="${ROOT}/stw-o3de-build/linux"
BIN="${BUILD}/bin/profile"
LAUNCHER="${BIN}/STW.GameLauncher"
ICD="${ROOT}/stw-o3de-gate/nvidia_icd.json"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="${ROOT}/stw-o3de-gate/player-slice-${RUN_ID}"
RUNTIME="${RUN_DIR}/xdg-runtime"
ALSA="${RUNTIME}/asound-null.conf"
BUILD_LOG="${RUN_DIR}/incremental-build.log"
TEST_LOG="${RUN_DIR}/tests.log"
LAUNCH_LOG="${RUN_DIR}/launcher.log"
FRAME="${RUN_DIR}/stw-player-slice.png"
THUMB="${RUN_DIR}/stw-player-slice-thumb.jpg"
xvfb_pid=""; launcher_pid=""
cleanup(){
  if [[ -n "${launcher_pid}" ]] && kill -0 "${launcher_pid}" 2>/dev/null; then kill -TERM -- "-${launcher_pid}" 2>/dev/null || true; fi
  if [[ -n "${xvfb_pid}" ]] && kill -0 "${xvfb_pid}" 2>/dev/null; then kill -TERM "${xvfb_pid}" 2>/dev/null || true; fi
}
trap cleanup EXIT INT TERM
mkdir -p "${RUN_DIR}" "${RUNTIME}"; chmod 700 "${RUNTIME}"
exec > >(tee "${RUN_DIR}/report.log") 2>&1

echo "STW O3DE PLAYER VERTICAL SLICE V1"
echo "SOURCE_COMMIT=${GITHUB_SHA}"
echo "INCREMENTAL_BUILD_ONLY=YES"
echo "COST_INCURRED=\$0.00"
[[ "${GITHUB_REPOSITORY}" == "MarsCommanderM/shooter-game-concept" ]]
[[ "${GITHUB_REF}" == "refs/heads/brauny/stw-game-production" ]]
checkout_head="$(git -C "${GITHUB_WORKSPACE}" rev-parse HEAD)"
echo "CHECKOUT_HEAD=${checkout_head}"
[[ "${checkout_head}" == "${GITHUB_SHA}" ]]
if engine_head="$(git -C "${ENGINE}" rev-parse HEAD 2>/dev/null)"; then
  echo "ENGINE_HEAD=${engine_head}"
  [[ "${engine_head}" == "3db6943249d8bd7960b9ed7e9aee310b7668586e" ]]
else
  echo "ENGINE_GIT_METADATA=UNAVAILABLE_PERSISTENT_BUILD"
  [[ -s "${ENGINE}/engine.json" ]]
  grep -Fq '3db6943249d8bd7960b9ed7e9aee310b7668586e' \
    "${GITHUB_WORKSPACE}/stw-o3de/O3DE_VERSION.md"
fi
[[ -x "${LAUNCHER}" && -d "${PROJECT}/Cache/linux" && -f "${GEM}/gem.json" ]]
cmake_command="$(sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' "${BUILD}/CMakeCache.txt" | head -1)"
[[ -x "${cmake_command}" ]]
cmake_bin_dir="$(dirname "${cmake_command}")"
export PATH="${cmake_bin_dir}:${PATH}"
echo "CMAKE_COMMAND=${cmake_command}"

echo "SYNCING_TRACKED_PRODUCTION_GEM=${GEM}"
cp -a "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/gem.json" "${GEM}/gem.json"
cp -a "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/CMakeLists.txt" "${GEM}/CMakeLists.txt"
cp -a "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/Code/." "${GEM}/Code/"
mkdir -p "${GEM}/Registry"
cp -a "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/Registry/." "${GEM}/Registry/"

start="$(date +%s)"
"${cmake_command}" --build "${BUILD}" --config profile --target STWGameplay.Tests -j 2 2>&1 | tee "${BUILD_LOG}"
echo "TEST_TARGET_BUILD_SECONDS=$(($(date +%s)-start))"
"${cmake_bin_dir}/ctest" --test-dir "${BUILD}" -C profile --output-on-failure -R 'STWGameplay' 2>&1 | tee "${TEST_LOG}"
grep -Eq '100% tests passed|The following tests passed' "${TEST_LOG}"

start="$(date +%s)"
"${cmake_command}" --build "${BUILD}" --config profile --target STW.GameLauncher -j 2 2>&1 | tee -a "${BUILD_LOG}"
echo "LAUNCHER_INCREMENTAL_BUILD_SECONDS=$(($(date +%s)-start))"
echo "FULL_ENGINE_REBUILD=NO"
echo "ASSET_PROCESSING=NOT_REQUIRED_SOURCE_ONLY"

nvidia-smi --query-gpu=name,driver_version --format=csv,noheader
VK_ICD_FILENAMES="${ICD}" vulkaninfo --summary 2>&1 | grep -E 'deviceName|driverVersion' | head -20
VK_ICD_FILENAMES="${ICD}" vulkaninfo --summary 2>&1 | grep -q 'Tesla T4'
display_number=""
for number in $(seq 90 120); do [[ ! -S "/tmp/.X11-unix/X${number}" ]] && { display_number="${number}"; break; }; done
[[ -n "${display_number}" ]]; display=":${display_number}"
Xvfb "${display}" -screen 0 1920x1080x24 -nolisten tcp -noreset >"${RUN_DIR}/xvfb.log" 2>&1 & xvfb_pid=$!
for _ in {1..40}; do [[ -S "/tmp/.X11-unix/X${display_number}" ]] && break; kill -0 "${xvfb_pid}"; sleep .25; done
cat >"${ALSA}" <<'EOF'
pcm.!default { type null hint { show on description "STW headless null output" } }
EOF
chmod 600 "${ALSA}"
setsid env DISPLAY="${display}" XDG_RUNTIME_DIR="${RUNTIME}" ALSA_CONFIG_PATH="${ALSA}" \
  VK_ICD_FILENAMES="${ICD}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
  "${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${ENGINE}" -sys_audio_disable 1 >"${LAUNCH_LOG}" 2>&1 &
launcher_pid=$!; echo "LAUNCHER_PID=${launcher_pid}"
for _ in $(seq 1 75); do
  kill -0 "${launcher_pid}" 2>/dev/null || { tail -n 200 "${LAUNCH_LOG}"; exit 1; }
  if grep -q 'Native Player Vertical Slice V1 active' "${LAUNCH_LOG}" && grep -Eqi 'Tesla T4|NVIDIA.*T4' "${LAUNCH_LOG}" && grep -Eqi 'Vulkan' "${LAUNCH_LOG}"; then sleep 20; break; fi
  sleep 1
done
grep -q 'Native Player Vertical Slice V1 active' "${LAUNCH_LOG}"
grep -Eqi 'Tesla T4|NVIDIA.*T4' "${LAUNCH_LOG}"
grep -Eqi 'Vulkan' "${LAUNCH_LOG}"
! grep -Eqi 'selected.*(llvmpipe|lavapipe|software)|adapter.*(llvmpipe|lavapipe)' "${LAUNCH_LOG}"

DISPLAY="${display}" python3 - "${FRAME}" <<'PY'
import ctypes, os, sys
from ctypes import *
from PIL import Image
class XImage(Structure):
    _fields_=[("width",c_int),("height",c_int),("xoffset",c_int),("format",c_int),("data",c_void_p),("byte_order",c_int),("bitmap_unit",c_int),("bitmap_bit_order",c_int),("bitmap_pad",c_int),("depth",c_int),("bytes_per_line",c_int),("bits_per_pixel",c_int),("red_mask",c_ulong),("green_mask",c_ulong),("blue_mask",c_ulong)]
class XWindowAttributes(Structure):
    _fields_=[("x",c_int),("y",c_int),("width",c_int),("height",c_int),("border_width",c_int),("depth",c_int),("visual",c_void_p),("root",c_ulong),("class_",c_int),("bit_gravity",c_int),("win_gravity",c_int),("backing_store",c_int),("backing_planes",c_ulong),("backing_pixel",c_ulong),("save_under",c_int),("colormap",c_ulong),("map_installed",c_int),("map_state",c_int),("all_event_masks",c_long),("your_event_mask",c_long),("do_not_propagate_mask",c_long),("override_redirect",c_int),("screen",c_void_p)]
x=CDLL("libX11.so.6"); x.XOpenDisplay.argtypes=[c_char_p]; x.XOpenDisplay.restype=c_void_p
x.XDefaultRootWindow.argtypes=[c_void_p]; x.XDefaultRootWindow.restype=c_ulong
x.XQueryTree.argtypes=[c_void_p,c_ulong,POINTER(c_ulong),POINTER(c_ulong),POINTER(POINTER(c_ulong)),POINTER(c_uint)]; x.XQueryTree.restype=c_int
x.XGetWindowAttributes.argtypes=[c_void_p,c_ulong,POINTER(XWindowAttributes)]; x.XGetWindowAttributes.restype=c_int
x.XFetchName.argtypes=[c_void_p,c_ulong,POINTER(c_char_p)]; x.XFetchName.restype=c_int
x.XGetImage.argtypes=[c_void_p,c_ulong,c_int,c_int,c_uint,c_uint,c_ulong,c_int]; x.XGetImage.restype=POINTER(XImage)
x.XFree.argtypes=[c_void_p]
d=x.XOpenDisplay(os.environ["DISPLAY"].encode())
if not d: raise SystemExit("XOpenDisplay failed")
root=x.XDefaultRootWindow(d); seen=set(); windows=[]
def walk(window, depth=0):
    if window in seen or depth > 8: return
    seen.add(window); attr=XWindowAttributes()
    if not x.XGetWindowAttributes(d,window,byref(attr)): return
    namep=c_char_p(); name=""
    if x.XFetchName(d,window,byref(namep)) and namep.value:
        name=namep.value.decode(errors="replace"); x.XFree(namep)
    windows.append((window,depth,attr.width,attr.height,attr.depth,attr.map_state,name))
    rr=c_ulong(); parent=c_ulong(); children=POINTER(c_ulong)(); count=c_uint()
    if x.XQueryTree(d,window,byref(rr),byref(parent),byref(children),byref(count)):
        for index in range(count.value): walk(children[index],depth+1)
        if children: x.XFree(children)
walk(root)
for row in windows: print("WINDOW",*row,sep="|")
candidates=[row for row in windows if row[0] != root and row[5] == 2 and row[2] >= 320 and row[3] >= 200]
if not candidates: raise SystemExit("no mapped launcher-sized X11 window")
target=max(candidates,key=lambda row:row[2]*row[3]); window,w,h=target[0],target[2],target[3]
p=x.XGetImage(d,window,0,0,w,h,c_ulong(-1).value,2)
if not p: raise SystemExit("XGetImage launcher window failed")
xi=p.contents; raw=string_at(xi.data,xi.bytes_per_line*h)
if xi.bits_per_pixel == 32 and xi.byte_order == 0:
    img=Image.frombytes("RGB",(w,h),raw,"raw","BGRX",xi.bytes_per_line,1)
elif xi.bits_per_pixel == 24 and xi.byte_order == 0:
    img=Image.frombytes("RGB",(w,h),raw,"raw","BGR",xi.bytes_per_line,1)
else:
    raise SystemExit(f"unsupported image format {xi.bits_per_pixel}/{xi.byte_order}")
img.save(sys.argv[1],"PNG")
pixels=list(img.getdata()); unique=len(set(pixels)); lums=[.2126*r+.7152*g+.0722*b for r,g,b in pixels]; mean=sum(lums)/len(lums); var=sum((v-mean)**2 for v in lums)/len(lums)
print(f"TARGET_WINDOW={target}\nFRAME={sys.argv[1]}\nRESOLUTION={w}x{h}\nSIZE={os.path.getsize(sys.argv[1])}\nUNIQUE_COLORS={unique}\nPIXEL_VARIANCE={var:.4f}\nMEAN_LUMINANCE={mean:.4f}")
if unique <= 16 or var <= 4.0: raise SystemExit("native frame is empty")
PY
python3 - "${FRAME}" "${THUMB}" <<'PY'
import sys
from PIL import Image
with Image.open(sys.argv[1]) as image:
    thumbnail = image.convert("RGB")
    thumbnail.thumbnail((640, 360))
    thumbnail.save(sys.argv[2], "JPEG", quality=82, optimize=True)
PY
echo "FRAME_BASE64_BEGIN"
base64 -w0 "${THUMB}"
echo
echo "FRAME_BASE64_END"
echo "ATOM_RHI_EVIDENCE:"
grep -Ein 'Atom|RHI|Vulkan|Tesla T4|NVIDIA|defaultlevel|Native Player Vertical Slice' "${LAUNCH_LOG}" | tail -120
echo "TEST_LOG=${TEST_LOG}"
echo "BUILD_LOG=${BUILD_LOG}"
echo "LAUNCH_LOG=${LAUNCH_LOG}"
echo "FRAME=${FRAME}"
echo "RESULT=PASS"
