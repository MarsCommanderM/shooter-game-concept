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
Xvfb "${display}" -screen 0 1280x720x24 -nolisten tcp -noreset >"${RUN_DIR}/xvfb.log" 2>&1 & xvfb_pid=$!
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
  if grep -q 'Native Player Vertical Slice V1 active' "${LAUNCH_LOG}" && grep -Eqi 'Tesla T4|NVIDIA.*T4' "${LAUNCH_LOG}" && grep -Eqi 'Vulkan' "${LAUNCH_LOG}"; then sleep 8; break; fi
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
x=CDLL("libX11.so.6"); x.XOpenDisplay.argtypes=[c_char_p]; x.XOpenDisplay.restype=c_void_p
x.XDefaultScreen.argtypes=[c_void_p]; x.XDefaultScreen.restype=c_int; x.XRootWindow.argtypes=[c_void_p,c_int]; x.XRootWindow.restype=c_ulong
x.XDisplayWidth.argtypes=[c_void_p,c_int]; x.XDisplayWidth.restype=c_int; x.XDisplayHeight.argtypes=[c_void_p,c_int]; x.XDisplayHeight.restype=c_int
x.XGetImage.argtypes=[c_void_p,c_ulong,c_int,c_int,c_uint,c_uint,c_ulong,c_int]; x.XGetImage.restype=POINTER(XImage)
d=x.XOpenDisplay(os.environ["DISPLAY"].encode()); s=x.XDefaultScreen(d); root=x.XRootWindow(d,s); w=x.XDisplayWidth(d,s); h=x.XDisplayHeight(d,s)
p=x.XGetImage(d,root,0,0,w,h,c_ulong(-1).value,2); xi=p.contents; raw=string_at(xi.data,xi.bytes_per_line*h)
img=Image.frombytes("RGB",(w,h),raw,"raw","BGRX" if xi.bits_per_pixel==32 else "BGR",xi.bytes_per_line,1); img.save(sys.argv[1],"PNG")
pixels=list(img.getdata()); unique=len(set(pixels)); lums=[.2126*r+.7152*g+.0722*b for r,g,b in pixels]; mean=sum(lums)/len(lums); var=sum((v-mean)**2 for v in lums)/len(lums)
print(f"FRAME={sys.argv[1]}\nRESOLUTION={w}x{h}\nSIZE={os.path.getsize(sys.argv[1])}\nUNIQUE_COLORS={unique}\nPIXEL_VARIANCE={var:.4f}")
if unique <= 16 or var <= 4.0: raise SystemExit("native frame is empty")
PY
echo "ATOM_RHI_EVIDENCE:"
grep -Ein 'Atom|RHI|Vulkan|Tesla T4|NVIDIA|defaultlevel|Native Player Vertical Slice' "${LAUNCH_LOG}" | tail -120
echo "TEST_LOG=${TEST_LOG}"
echo "BUILD_LOG=${BUILD_LOG}"
echo "LAUNCH_LOG=${LAUNCH_LOG}"
echo "FRAME=${FRAME}"
echo "RESULT=PASS"
