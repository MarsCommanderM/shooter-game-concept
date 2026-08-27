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
# O3DE routes AZ_Printf runtime output to the project runtime log once CrySystem's
# logger is active, so the STWGameplay acceptance markers land here, not in launcher
# stdout. Resolved from the known project path (not a historical run directory).
GAME_LOG="${PROJECT}/user/log/Game.log"
FRAME_NATIVE="${RUN_DIR}/stw-player-slice.ppm"
FRAME="${RUN_DIR}/stw-player-slice.png"
THUMB="${RUN_DIR}/stw-player-slice-thumb.jpg"
xvfb_pid=""; launcher_pid=""
cleanup(){
  if [[ -n "${launcher_pid}" ]] && kill -0 "${launcher_pid}" 2>/dev/null; then kill -TERM -- "-${launcher_pid}" 2>/dev/null || true; fi
  if [[ -n "${xvfb_pid}" ]] && kill -0 "${xvfb_pid}" 2>/dev/null; then kill -TERM "${xvfb_pid}" 2>/dev/null || true; fi
}
# On failure, surface the host-local diagnostic logs into the job output so the
# native launcher's PhysX/Atom errors are visible without Studio shell access.
dump_diagnostics(){
  echo "===== STW DIAGNOSTIC DUMP BEGIN ====="
  echo "RUN_DIR=${RUN_DIR}"
  for diag in "${LAUNCH_LOG}" "${RUN_DIR}/xvfb.log" "${BUILD_LOG}" "${TEST_LOG}"; do
    if [[ -f "${diag}" ]]; then
      echo "----- ${diag} (tail -200) -----"
      tail -n 200 "${diag}" 2>/dev/null || true
    else
      echo "----- ${diag} : MISSING -----"
    fi
  done
  echo "===== STW DIAGNOSTIC DUMP END ====="
}
# Live capture taken while the launcher is STILL RUNNING (before timeout cleanup),
# so a hung/quiet launcher is observable: process liveness/state/CPU, mapped
# modules, and every fresh O3DE log file discovered from the project/user roots.
timeout_diagnostics(){
  echo "===== STW TIMEOUT DIAGNOSTICS BEGIN ====="
  echo "MARKER_SOUGHT=Native Player Movement V2 PhysX active"
  if kill -0 "${launcher_pid}" 2>/dev/null; then
    echo "LAUNCHER_ALIVE=YES pid=${launcher_pid}"
  else
    echo "LAUNCHER_ALIVE=NO pid=${launcher_pid}"
  fi
  echo "--- ps (launcher) ---"
  ps -o pid,ppid,stat,etimes,cputime,pcpu,rss,wchan:32,comm -p "${launcher_pid}" 2>/dev/null || true
  echo "--- ps (children) ---"
  ps --ppid "${launcher_pid}" -o pid,ppid,stat,etimes,cputime,pcpu,comm 2>/dev/null || true
  echo "--- /proc/${launcher_pid}/status (selected) ---"
  grep -E '^(State|Threads|VmRSS|voluntary_ctxt_switches|nonvoluntary_ctxt_switches):' "/proc/${launcher_pid}/status" 2>/dev/null || true
  echo "--- /proc/${launcher_pid}/wchan ---"
  { cat "/proc/${launcher_pid}/wchan" 2>/dev/null; echo; } || true
  echo "--- mapped .so count ---"
  grep -Eo '/[^[:space:]]+\.so[^[:space:]]*' "/proc/${launcher_pid}/maps" 2>/dev/null | sort -u | wc -l || true
  echo "--- mapped STW/PhysX/Atom/Physics modules ---"
  grep -Eo '/[^[:space:]]+\.so[^[:space:]]*' "/proc/${launcher_pid}/maps" 2>/dev/null | sort -u | grep -Ei 'STW|PhysX|Atom|Physics' || true
  echo "--- launcher.log size/mtime ---"
  stat -c 'size=%s mtime=%y' "${LAUNCH_LOG}" 2>/dev/null || true
  echo "--- discover fresh O3DE logs (*.log modified < 6 min) ---"
  local found=""
  local root f
  for root in "${PROJECT}" "${PROJECT}/user" "${PROJECT}/Cache/linux" "${HOME}/.o3de" "${RUNTIME}"; do
    [[ -d "${root}" ]] || continue
    while IFS= read -r f; do
      [[ -n "${f}" ]] && found+="${f}"$'\n'
    done < <(find "${root}" -maxdepth 6 -type f -name '*.log' -newermt '-6 minutes' 2>/dev/null || true)
  done
  found="$(printf '%s' "${found}" | sort -u | grep -v -F "${RUN_DIR}" || true)"
  if [[ -z "${found}" ]]; then
    echo "O3DE_LOG_FILES_FOUND=NONE (outside RUN_DIR)"
  else
    echo "O3DE_LOG_FILES_FOUND:"
    printf '%s\n' "${found}"
    while IFS= read -r f; do
      [[ -n "${f}" && -f "${f}" ]] || continue
      echo "----- ${f} (size $(stat -c %s "${f}" 2>/dev/null), tail -160) -----"
      tail -n 160 "${f}" 2>/dev/null || true
      echo "----- ${f} : factual markers -----"
      grep -Eina 'default scene|PhysX|Character Controller|STWGameplay|spawnable|level|Asset Processor|CriticalAssets|Atom|RHI|Vulkan|Tesla T4|error|warning|assert|fatal' "${f}" 2>/dev/null | tail -80 || true
    done <<< "${found}"
  fi
  echo "===== STW TIMEOUT DIAGNOSTICS END ====="
}
# Runtime gameplay markers (AZ_Printf) may be written to launcher stdout OR, once
# CrySystem's logger is active, to the project runtime log (Game.log). Search both so
# a real success is detected wherever O3DE wrote it. Non-zero if found in neither.
runtime_grep(){
  grep "$@" "${LAUNCH_LOG}" 2>/dev/null || grep "$@" "${GAME_LOG}" 2>/dev/null
}
on_exit(){
  local status=$?
  if [[ "${status}" -ne 0 ]]; then dump_diagnostics; fi
  cleanup
}
trap on_exit EXIT
trap cleanup INT TERM
mkdir -p "${RUN_DIR}" "${RUNTIME}"; chmod 700 "${RUNTIME}"
exec > >(tee "${RUN_DIR}/report.log") 2>&1

echo "STW O3DE PRODUCTION PLAYER MOVEMENT V2"
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
copy_file_if_changed(){ cmp -s "$1" "$2" || cp -a "$1" "$2"; }
copy_tree_if_changed(){ diff -qr "$1" "$2" >/dev/null 2>&1 || cp -a "$1/." "$2/"; }
copy_file_if_changed "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/gem.json" "${GEM}/gem.json"
copy_file_if_changed "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/CMakeLists.txt" "${GEM}/CMakeLists.txt"
copy_tree_if_changed "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/Code" "${GEM}/Code"
mkdir -p "${GEM}/Registry"
copy_tree_if_changed "${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/Registry" "${GEM}/Registry"

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
# Force line-buffered stdout/stderr for the launcher so its native trace output is
# flushed to launcher.log continuously instead of sitting in libc block buffers until
# exit. stdbuf only changes stdio buffering via libstdbuf; it does not alter launcher
# behavior. Fall back to default buffering if stdbuf is unavailable.
STDBUF_PREFIX=""
if command -v stdbuf >/dev/null 2>&1; then
  STDBUF_PREFIX="stdbuf -oL -eL"
  echo "LAUNCHER_STDOUT_BUFFERING=line (stdbuf -oL -eL)"
else
  echo "LAUNCHER_STDOUT_BUFFERING=default (stdbuf unavailable)"
fi
setsid env DISPLAY="${display}" XDG_RUNTIME_DIR="${RUNTIME}" ALSA_CONFIG_PATH="${ALSA}" \
  STW_NATIVE_CAPTURE_PATH="${FRAME_NATIVE}" \
  STW_PHYSX_ACCEPTANCE=1 \
  VK_ICD_FILENAMES="${ICD}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
  ${STDBUF_PREFIX} "${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${ENGINE}" -sys_audio_disable 1 >"${LAUNCH_LOG}" 2>&1 &
launcher_pid=$!; echo "LAUNCHER_PID=${launcher_pid}"
for _ in $(seq 1 75); do
  kill -0 "${launcher_pid}" 2>/dev/null || { tail -n 200 "${LAUNCH_LOG}"; exit 1; }
  if runtime_grep -q 'Native Player Movement V2 PhysX active' && grep -Eqi 'Tesla T4|NVIDIA.*T4' "${LAUNCH_LOG}" && grep -Eqi 'Vulkan' "${LAUNCH_LOG}"; then break; fi
  sleep 1
done
# Diagnosis: if the activation marker is still absent after the wait window, capture
# live launcher/process/log state BEFORE the exit trap kills the launcher.
if ! runtime_grep -q 'Native Player Movement V2 PhysX active'; then
  timeout_diagnostics
fi
# Runtime acceptance marker: launcher.log OR Game.log. Hardware/RHI evidence stays on
# launcher stdout, where O3DE's early-boot RHI selection is written.
runtime_grep -q 'Native Player Movement V2 PhysX active'
grep -Eqi 'Tesla T4|NVIDIA.*T4' "${LAUNCH_LOG}"
grep -Eqi 'Vulkan' "${LAUNCH_LOG}"
! grep -Eqi 'selected.*(llvmpipe|lavapipe|software)|adapter.*(llvmpipe|lavapipe)' "${LAUNCH_LOG}"

last_frame_size=0
for _ in $(seq 1 45); do
  frame_size="$(stat -c %s "${FRAME_NATIVE}" 2>/dev/null || echo 0)"
  [[ "${frame_size}" -gt 0 && "${frame_size}" -eq "${last_frame_size}" ]] && break
  last_frame_size="${frame_size}"
  kill -0 "${launcher_pid}" 2>/dev/null || { tail -n 200 "${LAUNCH_LOG}"; exit 1; }
  sleep 1
done
[[ -s "${FRAME_NATIVE}" ]]
for _ in $(seq 1 20); do
  runtime_grep -q 'PERFORMANCE_BASELINE' && runtime_grep -q 'PHYSX_ACCEPTANCE result=PASS' && break
  kill -0 "${launcher_pid}" 2>/dev/null || { tail -n 200 "${LAUNCH_LOG}"; exit 1; }
  sleep 1
done
runtime_grep -q 'PHYSX_ACCEPTANCE result=PASS'
runtime_grep -q 'PERFORMANCE_BASELINE'
if runtime_grep -q 'Native Atom frame capture submitted'; then
  echo "FRAME_CAPTURE_SUBMISSION_LOG=CONFIRMED"
else
  echo "FRAME_CAPTURE_SUBMISSION_LOG=BUFFERED; native output file is authoritative"
fi

python3 - "${FRAME_NATIVE}" "${FRAME}" <<'PY'
import os, sys
from PIL import Image
with Image.open(sys.argv[1]) as source:
    img=source.convert("RGB")
img.save(sys.argv[2], "PNG")
w,h=img.size; pixels=list(img.getdata()); unique=len(set(pixels)); lums=[.2126*r+.7152*g+.0722*b for r,g,b in pixels]; mean=sum(lums)/len(lums); var=sum((v-mean)**2 for v in lums)/len(lums); near=sum(v<8.0 for v in lums)*100.0/len(lums)
print(f"CAPTURE_METHOD=O3DE_FRAMECAPTURE_RHI_READBACK\nNATIVE_FRAME={sys.argv[1]}\nFRAME={sys.argv[2]}\nRESOLUTION={w}x{h}\nNATIVE_SIZE={os.path.getsize(sys.argv[1])}\nSIZE={os.path.getsize(sys.argv[2])}\nUNIQUE_COLORS={unique}\nPIXEL_VARIANCE={var:.4f}\nMEAN_LUMINANCE={mean:.4f}\nNEAR_BLACK_PERCENT={near:.4f}")
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
{ grep -Ein 'Atom|RHI|Vulkan|Tesla T4|NVIDIA|defaultlevel' "${LAUNCH_LOG}" 2>/dev/null; \
  grep -Ein 'Player Movement V2|PHYSX_ACCEPTANCE|PERFORMANCE_BASELINE|Native Atom frame capture' "${GAME_LOG}" 2>/dev/null; } | tail -160
echo "TEST_LOG=${TEST_LOG}"
echo "BUILD_LOG=${BUILD_LOG}"
echo "LAUNCH_LOG=${LAUNCH_LOG}"
echo "GAME_LOG=${GAME_LOG}"
echo "FRAME=${FRAME}"
echo "RESULT=PASS"
