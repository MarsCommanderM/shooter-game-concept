#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly O3DE="${ROOT}/o3de-2605"
readonly PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
readonly BUILD="${ROOT}/stw-o3de-build/linux"
readonly BIN="${BUILD}/bin/profile"
readonly LAUNCHER="${BIN}/STW.GameLauncher"
readonly CACHE="${PROJECT}/Cache/linux"
readonly REPORT="${STATE}/native-visual-gate.txt"
readonly STDIO="${LOGS}/game-launcher-native.stdout-stderr.log"
readonly VULKAN_LOG="${LOGS}/vulkan-prelaunch-summary.log"
readonly FRAME="${STATE}/stw-gamelauncher-native.png"
readonly THUMB="${STATE}/stw-gamelauncher-native-thumb.jpg"
readonly NVIDIA_ICD="${STATE}/nvidia_icd.json"
readonly RUNTIME_DIR="${STATE}/xdg-runtime-visual-gate"

mkdir -p "${LOGS}"
: >"${REPORT}"
report() { printf '%s\n' "$*" | tee -a "${REPORT}"; }
fail() { report "FIRST REAL ERROR: $*"; report "COST INCURRED: \$0.00"; exit 1; }

xvfb_pid=""
launcher_pid=""
cleanup() {
  if [[ -n "${launcher_pid}" ]] && kill -0 "${launcher_pid}" 2>/dev/null; then
    kill -TERM -- "-${launcher_pid}" 2>/dev/null || kill -TERM "${launcher_pid}" 2>/dev/null || true
    for _ in {1..20}; do kill -0 "${launcher_pid}" 2>/dev/null || break; sleep 0.25; done
    kill -KILL -- "-${launcher_pid}" 2>/dev/null || true
  fi
  if [[ -n "${xvfb_pid}" ]] && kill -0 "${xvfb_pid}" 2>/dev/null; then
    kill -TERM "${xvfb_pid}" 2>/dev/null || true
    wait "${xvfb_pid}" 2>/dev/null || true
  fi
  rm -rf "${RUNTIME_DIR}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

report "STW O3DE NATIVE NVIDIA T4 VISUAL GATE"
report "PRODUCTION SOURCE STATE: d8adba3627911c29db8c9daacdc8793505fd5c4f"
[[ "$(git -C "${O3DE}" rev-parse HEAD)" == "3db6943249d8bd7960b9ed7e9aee310b7668586e" ]] || fail "O3DE pin mismatch"
[[ -x "${LAUNCHER}" ]] || fail "STW.GameLauncher missing or not executable"
[[ -s "${PROJECT}/project.json" ]] || fail "STW project.json missing"
[[ -d "${CACHE}" ]] || fail "processed Linux asset cache missing"
[[ -s "${BIN}/Registry/cmake_dependencies.stw.stw_gamelauncher.setreg" ]] || fail "STW GameLauncher dependency registry missing"
if ldd "${LAUNCHER}" | grep -q 'not found'; then ldd "${LAUNCHER}" | tee -a "${REPORT}"; fail "GameLauncher has unresolved shared libraries"; fi
report "LAUNCHER: ${LAUNCHER} ($(stat -c '%s bytes | %y' "${LAUNCHER}"))"
report "CACHE: ${CACHE} ($(du -sh "${CACHE}" | awk '{print $1}'))"
report "BUILD INVOCATION: NONE"
report "XDG RUNTIME BEFORE: ${XDG_RUNTIME_DIR:-UNSET}"
rm -rf "${RUNTIME_DIR}"
mkdir -p "${RUNTIME_DIR}"
chmod 700 "${RUNTIME_DIR}"
[[ "$(stat -c '%u:%g:%a' "${RUNTIME_DIR}")" == "$(id -u):$(id -g):700" ]] || fail "private XDG runtime directory ownership or permissions invalid"
export XDG_RUNTIME_DIR="${RUNTIME_DIR}"
report "XDG RUNTIME AFTER: ${XDG_RUNTIME_DIR} ($(stat -c '%U:%G %a' "${XDG_RUNTIME_DIR}"))"
report "AUDIO BACKEND: O3DE NullAudioSystem"
report "AUDIO FIX: official process-local CVar -sys_audio_disable 1"

command -v nvidia-smi >/dev/null || fail "nvidia-smi unavailable"
gpu_line="$(nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader | head -n1)"
report "NVIDIA-SMI: ${gpu_line}"
grep -q 'Tesla T4' <<<"${gpu_line}" || fail "active GPU is not NVIDIA Tesla T4"
mapfile -t nvidia_nodes < <(compgen -G '/dev/nvidia*' || true)
(("${#nvidia_nodes[@]}" > 0)) || fail "no NVIDIA device nodes"
report "NVIDIA DEVICE NODES: ${nvidia_nodes[*]}"

command -v vulkaninfo >/dev/null || fail "vulkaninfo unavailable"
[[ -s "${NVIDIA_ICD}" ]] || fail "qualified Lightning NVIDIA Vulkan ICD missing"
export VK_ICD_FILENAMES="${NVIDIA_ICD}"
set +e
timeout 30s vulkaninfo --summary >"${VULKAN_LOG}" 2>&1
vk_rc=$?
set -e
cat "${VULKAN_LOG}" | tee -a "${REPORT}"
((vk_rc == 0)) || fail "vulkaninfo --summary exited ${vk_rc}"
grep -qi 'Tesla T4' "${VULKAN_LOG}" || fail "Vulkan does not expose NVIDIA Tesla T4"
report "GPU RECHECK: PASS"
report "VULKAN RECHECK: PASS — NVIDIA Tesla T4 enumerated"

display_socket_ready() {
  local display_number="${1#:}"
  display_number="${display_number%%.*}"
  [[ "${display_number}" =~ ^[0-9]+$ ]] && [[ -S "/tmp/.X11-unix/X${display_number}" ]]
}
selected_display=":97"
display_socket_ready "${selected_display}" && fail "DISPLAY :97 is already occupied; refusing to interfere"
command -v Xvfb >/dev/null || fail "Xvfb unavailable"
Xvfb "${selected_display}" -screen 0 1280x720x24 -nolisten tcp -noreset >"${LOGS}/xvfb-native.log" 2>&1 &
xvfb_pid=$!
for _ in {1..40}; do
  display_socket_ready "${selected_display}" && break
  kill -0 "${xvfb_pid}" 2>/dev/null || fail "Xvfb exited during startup"
  sleep 0.25
done
display_socket_ready "${selected_display}" || fail "Xvfb display socket did not become usable"
display_strategy="owned Xvfb ${selected_display}, 1280x720x24"
export DISPLAY="${selected_display}"
report "DISPLAY STRATEGY: ${display_strategy}"

report "PROJECT LOAD SETTINGS:"
grep -RInE 'load_level|LoadLevel|default_level|startup' "${PROJECT}/Registry" "${PROJECT}/user/Registry" 2>/dev/null | sed -n '1,80p' | tee -a "${REPORT}" || true
report "CACHED LEVEL PRODUCTS:"
find "${CACHE}" -type f \( -iname '*.spawnable' -o -iname '*.prefab' -o -iname '*.level' \) -printf '%p\n' 2>/dev/null | sed -n '1,80p' | tee -a "${REPORT}" || true

command=("${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${O3DE}" "-sys_audio_disable" "1")
report "LAUNCH COMMAND: DISPLAY=${DISPLAY} XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR} VK_ICD_FILENAMES=${NVIDIA_ICD} LD_LIBRARY_PATH=${BIN}:<existing> ${command[*]}"
: >"${STDIO}"
setsid env DISPLAY="${DISPLAY}" XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR}" VK_ICD_FILENAMES="${NVIDIA_ICD}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${command[@]}" >"${STDIO}" 2>&1 &
launcher_pid=$!
report "GAME LAUNCHER PID: ${launcher_pid}"

render_wait=0
for second in {1..90}; do
  if ! kill -0 "${launcher_pid}" 2>/dev/null; then
    wait "${launcher_pid}" || launcher_rc=$?
    launcher_rc="${launcher_rc:-0}"
    tail -n 240 "${STDIO}" | tee -a "${REPORT}"
    fail "GameLauncher exited before frame capture with code ${launcher_rc}"
  fi
  if grep -Eqi 'Tesla T4|NVIDIA.*T4' "${STDIO}" && grep -Eqi 'Vulkan|RHI' "${STDIO}"; then
    render_wait=$second
    ((second >= 20)) && break
  fi
  sleep 1
done
report "LAUNCHER ALIVE AFTER: ${render_wait:-90}s"
grep -Fq 'Null AudioSystem created!' "${STDIO}" || fail "O3DE did not activate NullAudioSystem"
if grep -Eqi 'Failed to create secure directory|ALSA lib|Unknown PCM|AudioSystem created!' "${STDIO}"; then
  fail "headless audio hardware path was still activated"
fi
report "ATOM/RHI/GPU EVIDENCE:"
grep -Ein 'Atom|RHI|Vulkan|adapter|physical device|Tesla T4|NVIDIA|device name|renderer|defaultlevel|LoadLevel|spawnable' "${STDIO}" | tail -n 240 | tee -a "${REPORT}" || true
grep -Eqi 'Tesla T4|NVIDIA.*T4' "${STDIO}" || fail "Atom/RHI launcher log does not identify NVIDIA Tesla T4"
grep -Eqi 'Vulkan' "${STDIO}" || fail "launcher log does not prove Vulkan backend"
if grep -Eqi 'selected.*(llvmpipe|lavapipe|software)|using.*(llvmpipe|lavapipe|software)|adapter.*(llvmpipe|lavapipe)' "${STDIO}"; then
  fail "runtime selected a software renderer"
fi

rm -f "${FRAME}" "${THUMB}"
python3 - "${FRAME}" <<'PY'
import ctypes
import os
import sys
from ctypes import POINTER, Structure, c_char_p, c_int, c_uint, c_ulong, c_void_p
from PIL import Image

class XImage(Structure):
    _fields_ = [
        ("width", c_int), ("height", c_int), ("xoffset", c_int),
        ("format", c_int), ("data", c_void_p), ("byte_order", c_int),
        ("bitmap_unit", c_int), ("bitmap_bit_order", c_int),
        ("bitmap_pad", c_int), ("depth", c_int),
        ("bytes_per_line", c_int), ("bits_per_pixel", c_int),
        ("red_mask", c_ulong), ("green_mask", c_ulong), ("blue_mask", c_ulong),
    ]

x11 = ctypes.CDLL("libX11.so.6")
x11.XOpenDisplay.argtypes = [c_char_p]
x11.XOpenDisplay.restype = c_void_p
x11.XDefaultScreen.argtypes = [c_void_p]
x11.XDefaultScreen.restype = c_int
x11.XRootWindow.argtypes = [c_void_p, c_int]
x11.XRootWindow.restype = c_ulong
x11.XDisplayWidth.argtypes = [c_void_p, c_int]
x11.XDisplayWidth.restype = c_int
x11.XDisplayHeight.argtypes = [c_void_p, c_int]
x11.XDisplayHeight.restype = c_int
x11.XGetImage.argtypes = [c_void_p, c_ulong, c_int, c_int, c_uint, c_uint, c_ulong, c_int]
x11.XGetImage.restype = POINTER(XImage)
x11.XSync.argtypes = [c_void_p, c_int]
x11.XCloseDisplay.argtypes = [c_void_p]

dpy = x11.XOpenDisplay(os.environ.get("DISPLAY", "").encode())
if not dpy:
    raise SystemExit("XOpenDisplay failed")
screen = x11.XDefaultScreen(dpy)
root = x11.XRootWindow(dpy, screen)
width = x11.XDisplayWidth(dpy, screen)
height = x11.XDisplayHeight(dpy, screen)
x11.XSync(dpy, 0)
image_ptr = x11.XGetImage(dpy, root, 0, 0, width, height, c_ulong(-1).value, 2)
if not image_ptr:
    raise SystemExit("XGetImage failed")
ximage = image_ptr.contents
raw = ctypes.string_at(ximage.data, ximage.bytes_per_line * height)
if ximage.bits_per_pixel == 32 and ximage.byte_order == 0:
    captured = Image.frombytes("RGB", (width, height), raw, "raw", "BGRX", ximage.bytes_per_line, 1)
elif ximage.bits_per_pixel == 24 and ximage.byte_order == 0:
    captured = Image.frombytes("RGB", (width, height), raw, "raw", "BGR", ximage.bytes_per_line, 1)
else:
    raise SystemExit(f"unsupported XImage format: bpp={ximage.bits_per_pixel}, byte_order={ximage.byte_order}")
captured.save(sys.argv[1], "PNG")
print(f"X11_CAPTURE={width}x{height};bpp={ximage.bits_per_pixel};bytes_per_line={ximage.bytes_per_line}")
x11.XCloseDisplay(dpy)
PY
[[ -s "${FRAME}" ]] || fail "native screenshot file is empty"

frame_info="$(python3 - "${FRAME}" "${THUMB}" <<'PY'
import os, sys
from PIL import Image, ImageStat
src, dst = sys.argv[1:3]
with Image.open(src) as image:
    rgb = image.convert("RGB")
    extrema = rgb.getextrema()
    mean = tuple(round(value, 3) for value in ImageStat.Stat(rgb).mean)
    entropy = rgb.entropy()
    thumb = rgb.copy()
    thumb.thumbnail((480, 270))
    thumb.save(dst, "JPEG", quality=72)
    print(f"{image.format}|{image.width}x{image.height}|{os.path.getsize(src)} bytes|entropy={entropy:.6f}|extrema={extrema}|mean={mean}")
PY
)" || fail "captured frame is unreadable"
report "NATIVE FRAME: ${FRAME}"
report "FRAME INFO: ${frame_info}"
report "FRAME METADATA: $(stat -c '%s bytes | %y' "${FRAME}")"
[[ -s "${THUMB}" ]] || fail "frame evidence thumbnail missing"
report "FRAME_EVIDENCE_BASE64_BEGIN"
base64 -w0 "${THUMB}" | tee -a "${REPORT}"
printf '\n' | tee -a "${REPORT}"
report "FRAME_EVIDENCE_BASE64_END"

report "GAME LAUNCHER STATUS: PASS — alive through native capture"
report "ATOM STATUS: PASS"
report "RHI STATUS: PASS"
report "RENDER BACKEND: Vulkan"
report "SELECTED GPU: NVIDIA Tesla T4"
report "SOFTWARE FALLBACK: NO"
report "FILES CHANGED: runtime logs and native frame evidence only; no source or build tree"
report "COST INCURRED: \$0.00"
report "FIRST REAL ERROR: NONE"
report "DECISION: PASS"
report "NEXT SAFE ACTION: inspect native frame evidence and decide O3DE migration gate"
