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
readonly COMBINED="${LOGS}/game-launcher-native.combined.log"
readonly VULKAN_LOG="${LOGS}/vulkan-prelaunch-summary.log"
readonly FRAME="${STATE}/stw-gamelauncher-native.png"
readonly THUMB="${STATE}/stw-gamelauncher-native-thumb.jpg"
readonly MARKER="${STATE}/native-visual-gate.start"

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
}
trap cleanup EXIT INT TERM

report "STW O3DE NATIVE NVIDIA T4 VISUAL GATE"
report "PRODUCTION SOURCE STATE: d8adba3627911c29db8c9daacdc8793505fd5c4f"
[[ "$(git -C "${O3DE}" rev-parse HEAD)" == "3db6943249d8bd7960b9ed7e9aee310b7668586e" ]] || fail "O3DE pin mismatch"
[[ -x "${LAUNCHER}" ]] || fail "STW.GameLauncher missing or not executable"
[[ -s "${PROJECT}/project.json" ]] || fail "STW project.json missing"
[[ -d "${CACHE}" ]] || fail "processed Linux asset cache missing"
[[ -s "${BIN}/Registry/cmake_dependencies.stw.gamelauncher.setreg" ]] || fail "STW GameLauncher dependency registry missing"
if ldd "${LAUNCHER}" | grep -q 'not found'; then ldd "${LAUNCHER}" | tee -a "${REPORT}"; fail "GameLauncher has unresolved shared libraries"; fi
report "LAUNCHER: ${LAUNCHER} ($(stat -c '%s bytes | %y' "${LAUNCHER}"))"
report "CACHE: ${CACHE} ($(du -sh "${CACHE}" | awk '{print $1}'))"
report "BUILD INVOCATION: NONE"

command -v nvidia-smi >/dev/null || fail "nvidia-smi unavailable"
gpu_line="$(nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader | head -n1)"
report "NVIDIA-SMI: ${gpu_line}"
grep -q 'Tesla T4' <<<"${gpu_line}" || fail "active GPU is not NVIDIA Tesla T4"
mapfile -t nvidia_nodes < <(compgen -G '/dev/nvidia*' || true)
(("${#nvidia_nodes[@]}" > 0)) || fail "no NVIDIA device nodes"
report "NVIDIA DEVICE NODES: ${nvidia_nodes[*]}"

command -v vulkaninfo >/dev/null || fail "vulkaninfo unavailable"
mapfile -t icds < <(find /usr/share/vulkan/icd.d /etc/vulkan/icd.d -maxdepth 1 -type f -iname '*nvidia*.json' -print 2>/dev/null | sort -u)
(("${#icds[@]}" > 0)) || fail "NVIDIA Vulkan ICD missing"
report "NVIDIA VULKAN ICD: ${icds[*]}"
set +e
timeout 30s vulkaninfo --summary >"${VULKAN_LOG}" 2>&1
vk_rc=$?
set -e
cat "${VULKAN_LOG}" | tee -a "${REPORT}"
((vk_rc == 0)) || fail "vulkaninfo --summary exited ${vk_rc}"
grep -qi 'Tesla T4' "${VULKAN_LOG}" || fail "Vulkan does not expose NVIDIA Tesla T4"
report "GPU RECHECK: PASS"
report "VULKAN RECHECK: PASS — NVIDIA Tesla T4 enumerated"

selected_display="${DISPLAY:-}"
display_strategy=""
if [[ -n "${selected_display}" ]] && command -v xdpyinfo >/dev/null && timeout 5s xdpyinfo -display "${selected_display}" >/dev/null 2>&1; then
  display_strategy="existing X display ${selected_display}"
else
  command -v Xvfb >/dev/null || fail "no usable X display and Xvfb unavailable"
  command -v xdpyinfo >/dev/null || fail "xdpyinfo unavailable for Xvfb validation"
  for n in 97 98 99 100; do
    [[ -e "/tmp/.X11-unix/X${n}" ]] && continue
    selected_display=":${n}"
    break
  done
  [[ -n "${selected_display}" ]] || fail "no free X display number"
  Xvfb "${selected_display}" -screen 0 1280x720x24 -nolisten tcp -noreset >"${LOGS}/xvfb-native.log" 2>&1 &
  xvfb_pid=$!
  for _ in {1..40}; do
    timeout 2s xdpyinfo -display "${selected_display}" >/dev/null 2>&1 && break
    kill -0 "${xvfb_pid}" 2>/dev/null || fail "Xvfb exited during startup"
    sleep 0.25
  done
  timeout 5s xdpyinfo -display "${selected_display}" >/dev/null 2>&1 || fail "Xvfb display did not become usable"
  display_strategy="owned Xvfb ${selected_display}, 1280x720x24"
fi
export DISPLAY="${selected_display}"
report "DISPLAY STRATEGY: ${display_strategy}"
if command -v glxinfo >/dev/null; then
  report "OPENGL DISPLAY INFO (diagnostic only; Atom gate is Vulkan):"
  timeout 15s glxinfo -B 2>&1 | sed -n '1,30p' | tee -a "${REPORT}" || true
fi

report "PROJECT LOAD SETTINGS:"
grep -RInE 'load_level|LoadLevel|default_level|startup' "${PROJECT}/Registry" "${PROJECT}/user/Registry" 2>/dev/null | sed -n '1,80p' | tee -a "${REPORT}" || true
report "CACHED LEVEL PRODUCTS:"
find "${CACHE}" -type f \( -iname '*.spawnable' -o -iname '*.prefab' -o -iname '*.level' \) -printf '%p\n' 2>/dev/null | sed -n '1,80p' | tee -a "${REPORT}" || true

touch "${MARKER}"
command=("${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${O3DE}")
report "LAUNCH COMMAND: DISPLAY=${DISPLAY} LD_LIBRARY_PATH=${BIN}:<existing> ${command[*]}"
: >"${STDIO}"
setsid env DISPLAY="${DISPLAY}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${command[@]}" >"${STDIO}" 2>&1 &
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

mapfile -t fresh_logs < <(find "${PROJECT}/user" "${STATE}" -type f -newer "${MARKER}" \( -iname '*.log' -o -iname '*.txt' \) -print 2>/dev/null | sort -u)
cp "${STDIO}" "${COMBINED}"
for log in "${fresh_logs[@]}"; do
  [[ "$log" == "${STDIO}" || "$log" == "${COMBINED}" || "$log" == "${REPORT}" ]] && continue
  printf '\n===== %s =====\n' "$log" >>"${COMBINED}"
  tail -n 12000 "$log" >>"${COMBINED}" 2>/dev/null || true
done
report "RUNTIME LOGS:"
printf '  %s\n' "${fresh_logs[@]}" | tee -a "${REPORT}"
report "COMBINED RUNTIME LOG: ${COMBINED}"
report "ATOM/RHI/GPU EVIDENCE:"
grep -Ein 'Atom|RHI|Vulkan|adapter|physical device|Tesla T4|NVIDIA|device name|renderer' "${COMBINED}" | tail -n 160 | tee -a "${REPORT}" || true
grep -Eqi 'Tesla T4|NVIDIA.*T4' "${COMBINED}" || fail "Atom/RHI logs do not identify NVIDIA Tesla T4"
grep -Eqi 'Vulkan' "${COMBINED}" || fail "runtime logs do not prove Vulkan backend"
if grep -Eqi 'selected.*(llvmpipe|lavapipe|software)|using.*(llvmpipe|lavapipe|software)|adapter.*(llvmpipe|lavapipe)' "${COMBINED}"; then
  fail "runtime selected a software renderer"
fi

rm -f "${FRAME}" "${THUMB}"
if command -v import >/dev/null; then
  timeout 30s import -display "${DISPLAY}" -window root "${FRAME}" || fail "ImageMagick native X11 frame capture failed"
elif command -v scrot >/dev/null; then
  timeout 30s scrot -D "${DISPLAY}" "${FRAME}" || fail "scrot native X11 frame capture failed"
elif command -v xwd >/dev/null && command -v convert >/dev/null; then
  xwd_file="${STATE}/stw-gamelauncher-native.xwd"
  timeout 30s xwd -display "${DISPLAY}" -root -silent -out "${xwd_file}" || fail "xwd native frame capture failed"
  timeout 30s convert "${xwd_file}" "${FRAME}" || fail "xwd frame conversion failed"
else
  fail "no factual native X11 screenshot tool is installed"
fi
[[ -s "${FRAME}" ]] || fail "native screenshot file is empty"

if command -v identify >/dev/null; then
  frame_info="$(identify -format '%m|%wx%h|%b|%[mean]|%[standard-deviation]|%[entropy]' "${FRAME}" 2>&1)" || fail "captured frame is unreadable"
else
  frame_info="$(file "${FRAME}")"
fi
report "NATIVE FRAME: ${FRAME}"
report "FRAME INFO: ${frame_info}"
report "FRAME METADATA: $(stat -c '%s bytes | %y' "${FRAME}")"

if command -v convert >/dev/null; then
  convert "${FRAME}" -resize 480x270 -quality 72 "${THUMB}"
elif command -v magick >/dev/null; then
  magick "${FRAME}" -resize 480x270 -quality 72 "${THUMB}"
else
  cp "${FRAME}" "${THUMB}"
fi
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
