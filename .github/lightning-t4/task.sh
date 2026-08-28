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
  for diag in "${LAUNCH_LOG}" "${GAME_LOG}" "${RUN_DIR}/xvfb.log" "${BUILD_LOG}" "${TEST_LOG}"; do
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
  runtime_grep -q 'PERFORMANCE_BASELINE' && runtime_grep -q 'PHYSX_ACCEPTANCE result=PASS' && runtime_grep -q 'VIEWMODEL_ACCEPTANCE result=PASS' && runtime_grep -q 'ATOM_VIEWMODEL_MESH result=PASS' && break
  kill -0 "${launcher_pid}" 2>/dev/null || { tail -n 200 "${LAUNCH_LOG}"; exit 1; }
  sleep 1
done
runtime_grep -q 'PHYSX_ACCEPTANCE result=PASS'
runtime_grep -q 'VIEWMODEL_ACCEPTANCE result=PASS'
# The first-person weapon body is now a real Atom mesh; the runtime only prints PASS once the
# model instance actually exists, so this proves the asset resolved and the mesh is renderable.
runtime_grep -q 'ATOM_VIEWMODEL_MESH result=PASS'
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
echo "VIEWMODEL_EVIDENCE:"
runtime_grep -n 'VIEWMODEL_ACCEPTANCE result=PASS'
echo "ATOM_VIEWMODEL_MESH_EVIDENCE:"
runtime_grep -n 'ATOM_VIEWMODEL_MESH result=PASS'
echo "ATOM_RHI_EVIDENCE:"
{ grep -Ein 'Atom|RHI|Vulkan|Tesla T4|NVIDIA|defaultlevel' "${LAUNCH_LOG}" 2>/dev/null; \
  grep -Ein 'Player Movement V2|PHYSX_ACCEPTANCE|PERFORMANCE_BASELINE|Native Atom frame capture' "${GAME_LOG}" 2>/dev/null; } | tail -160
echo "TEST_LOG=${TEST_LOG}"
echo "BUILD_LOG=${BUILD_LOG}"
echo "LAUNCH_LOG=${LAUNCH_LOG}"
echo "GAME_LOG=${GAME_LOG}"
echo "FRAME=${FRAME}"
# Supplemental read-only evidence about the persistent O3DE project and asset
# pipeline (BLOCK 4). It runs only after every existing acceptance check above
# has already passed, so it cannot weaken or substitute for the gameplay gate.
# Nothing here writes to the Lightning host, enables a Gem, or runs Asset
# Processor. Absence of a file is reported as a fact, not treated as an error.
echo "=================================================="
echo "STW_ASSET_PIPELINE_INVENTORY_BEGIN"
echo "=================================================="
CACHE="${PROJECT}/Cache/linux"
echo "INVENTORY_PROJECT_PATH=${PROJECT}"
echo "INVENTORY_ENGINE_PATH=${ENGINE}"
echo "INVENTORY_GEM_PATH=${GEM}"
echo "INVENTORY_CACHE_PATH=${CACHE}"

# --- A/B. project identity and enabled Gems (only required fields parsed) ---
project_json="${PROJECT}/project.json"
if [[ -f "${project_json}" ]]; then
  echo "PROJECT_JSON_PRESENT=true"
  echo "PROJECT_JSON_PATH=${project_json}"
  python3 - "${project_json}" <<'PY' || echo "PROJECT_JSON_PARSE=FAILED"
import json, sys
data = json.load(open(sys.argv[1]))
print(f"PROJECT_NAME={data.get('project_name','UNVERIFIED')}")
print(f"PROJECT_ENGINE_REFERENCE={data.get('engine','NOT_PRESENT')}")
print(f"PROJECT_VERSION={data.get('version','NOT_PRESENT')}")
names = []
for gem in data.get('gem_names', []):
    if isinstance(gem, dict):
        names.append(str(gem.get('name', 'UNKNOWN')))
    else:
        names.append(str(gem))
print(f"PROJECT_GEM_NAMES_COUNT={len(names)}")
for name in sorted(names):
    print(f"GEM_EVIDENCE name={name} enabled=true source=project.json:gem_names")
watch = ["STWGameplay","PhysX5","PhysX","Atom","AtomLyIntegration","Atom_Feature_Common",
         "SceneProcessing","EMotionFX","MiniAudio","AudioSystem","AudioEngineWwise",
         "OpenParticleSystem","Camera","DebugDraw","PrimitiveAssets"]
lower = {n.lower() for n in names}
for target in watch:
    state = "true" if target.lower() in lower else "false"
    print(f"GEM_TARGET name={target} in_project_gem_names={state} source=project.json:gem_names")
PY
else
  echo "PROJECT_JSON_PRESENT=false"
fi
for cmake_gems in "${PROJECT}/Gem/Code/enabled_gems.cmake" "${PROJECT}/enabled_gems.cmake"; do
  if [[ -f "${cmake_gems}" ]]; then
    echo "ENABLED_GEMS_CMAKE=${cmake_gems}"
    grep -oE '[A-Za-z0-9_.]+' "${cmake_gems}" | sed 's/^/ENABLED_GEMS_CMAKE_ENTRY=/' | head -80 || true
  fi
done

# --- C. project source directories and source asset candidates -------------
echo "PROJECT_TOP_LEVEL_DIRECTORIES:"
find "${PROJECT}" -mindepth 1 -maxdepth 1 -type d -printf 'PROJECT_DIR path=%p\n' 2>/dev/null | sort || true
echo "PROJECT_SOURCE_ASSET_CANDIDATES (Cache/user/build excluded):"
source_assets=0
while IFS= read -r asset; do
  printf 'SOURCE_ASSET path=%s extension=%s bytes=%s\n' \
    "${asset}" "${asset##*.}" "$(stat -c %s "${asset}" 2>/dev/null || echo 0)"
  source_assets=$((source_assets + 1))
done < <(find "${PROJECT}" \
    \( -path "${PROJECT}/Cache" -o -path "${PROJECT}/user" -o -path "${PROJECT}/build" \) -prune -o \
    -type f \( -iname '*.fbx' -o -iname '*.gltf' -o -iname '*.glb' -o -iname '*.obj' -o -iname '*.dae' \
    -o -iname '*.actor' -o -iname '*.motion' -o -iname '*.animgraph' -o -iname '*.motionset' \
    -o -iname '*.material' -o -iname '*.materialtype' -o -iname '*.png' -o -iname '*.tga' -o -iname '*.dds' \
    -o -iname '*.tif' -o -iname '*.tiff' -o -iname '*.jpg' -o -iname '*.exr' \
    -o -iname '*.wav' -o -iname '*.ogg' -o -iname '*.bnk' \
    -o -iname '*.prefab' -o -iname '*.spawnable' -o -iname '*.assetinfo' \) -print 2>/dev/null | sort | head -200 || true)
echo "SOURCE_ASSET_COUNT_REPORTED=${source_assets}"

# --- D. defaultlevel source vs generated product ---------------------------
echo "DEFAULT_LEVEL_CANDIDATES:"
level_sources=0
while IFS= read -r lvl; do
  case "${lvl}" in
    "${PROJECT}/Cache"/*) scope=GENERATED_PRODUCT ;;
    *) scope=SOURCE ; level_sources=$((level_sources + 1)) ;;
  esac
  printf 'DEFAULT_LEVEL_CANDIDATE path=%s extension=%s bytes=%s scope=%s\n' \
    "${lvl}" "${lvl##*.}" "$(stat -c %s "${lvl}" 2>/dev/null || echo 0)" "${scope}"
done < <(find "${PROJECT}" -iname '*defaultlevel*' -type f -print 2>/dev/null | sort | head -40 || true)
echo "DEFAULT_LEVEL_SOURCE_COUNT=${level_sources}"
while IFS= read -r lvl; do
  echo "DEFAULT_LEVEL_SOURCE=${lvl}"
  if file "${lvl}" 2>/dev/null | grep -qi 'text\|JSON'; then
    echo "DEFAULT_LEVEL_SOURCE_FORM=TEXT_READABLE"
    level_entities="$(grep -c '"Entity"' "${lvl}" 2>/dev/null || true)"
    echo "DEFAULT_LEVEL_ENTITY_COUNT_LINES=${level_entities:-UNVERIFIED}"
    for marker in Camera Light Mesh Actor AnimGraph SimpleMotion PhysX STWGameplay Atom; do
      marker_lines="$(grep -ci "${marker}" "${lvl}" 2>/dev/null || true)"
      echo "DEFAULT_LEVEL_MARKER name=${marker} matching_lines=${marker_lines:-0}"
    done
  else
    echo "DEFAULT_LEVEL_SOURCE_FORM=BINARY_OR_UNKNOWN"
  fi
done < <(find "${PROJECT}" -path "${PROJECT}/Cache" -prune -o -iname '*defaultlevel*' -type f -print 2>/dev/null | sort | head -3 || true)

# --- E. Asset Processor configuration actually present ---------------------
echo "ASSET_PROCESSOR_CONFIG_FILES:"
while IFS= read -r cfg; do
  echo "ASSET_PROCESSOR_CONFIG path=${cfg}"
  grep -nE 'ScanFolder|"watch"|recursive|order|Platform' "${cfg}" 2>/dev/null | head -25 \
    | sed 's/^/ASSET_PROCESSOR_CONFIG_LINE /' || true
done < <({ find "${PROJECT}/Registry" "${GEM}/Registry" -maxdepth 1 -type f -name '*.setreg' -print 2>/dev/null; \
           find "${ENGINE}/Registry" -maxdepth 1 -type f -name 'AssetProcessorPlatformConfig.setreg' -print 2>/dev/null; \
           find "${PROJECT}" -maxdepth 1 -type f -name 'AssetProcessorPlatformConfig.*' -print 2>/dev/null; } | sort -u | head -12 || true)

# --- F. Cache products, never reported as source assets --------------------
if [[ -d "${CACHE}" ]]; then
  echo "CACHE_PRESENT=true"
  echo "CACHE_TOTAL_PRODUCT_FILES=$(find "${CACHE}" -type f 2>/dev/null | wc -l || echo UNVERIFIED)"
  find "${CACHE}" -type f 2>/dev/null | sed -n 's/.*\.\([A-Za-z0-9]\{1,16\}\)$/\1/p' | sort | uniq -c | sort -rn | head -25 \
    | while read -r count ext; do echo "CACHE_PRODUCT_CATEGORY type=${ext} count=${count}"; done || true
  for pattern in 'defaultlevel*' '*.azmodel' '*.azmaterial' '*.streamingimage' '*.actor' '*.motion' '*.wav' '*.ogg'; do
    hit="$(find "${CACHE}" -iname "${pattern}" -type f -print 2>/dev/null | head -1 || true)"
    echo "CACHE_PRODUCT_PROBE pattern=${pattern} count=$(find "${CACHE}" -iname "${pattern}" -type f 2>/dev/null | wc -l || echo 0) example=${hit:-NONE}"
  done
else
  echo "CACHE_PRESENT=false"
fi

# --- G. builder modules present in the built profile output ----------------
# Module presence proves the code was built. Builder registration is only
# provable by running Asset Processor, which this gate deliberately does not do.
for builder in SceneProcessing ImageProcessingAtom EMotionFX MiniAudio AudioSystem OpenParticleSystem Atom_Feature_Common; do
  count="$(find "${BIN}" -maxdepth 2 -iname "*${builder}*" -type f 2>/dev/null | wc -l || echo 0)"
  example="$(find "${BIN}" -maxdepth 2 -iname "*${builder}*" -type f -print 2>/dev/null | head -1 || true)"
  echo "BUILDER_MODULE name=${builder} files=${count} example=${example:-NONE} registration=UNVERIFIED_WITHOUT_ASSET_PROCESSOR"
done
echo "=================================================="
echo "STW_ASSET_PIPELINE_INVENTORY_END"
echo "=================================================="

# BLOCK 6A: compact, read-only proof of the installed Lightning asset-authoring
# path. This runs after every production acceptance and inventory check above.
# It does not create an asset, invoke Asset Processor, or write to Project/Cache.
echo "=================================================="
echo "STW_ASSET_AUTHORING_CAPABILITY_BEGIN"
echo "=================================================="

python_path="$(command -v python3 2>/dev/null || true)"
python_version="UNAVAILABLE"
python_stdlib_authoring="NO"
if [[ -n "${python_path}" && -x "${python_path}" ]]; then
  python_version="$("${python_path}" --version 2>&1 | head -1 | tr ' ' '_')"
  if "${python_path}" -c 'import json, struct, pathlib' >/dev/null 2>&1; then
    python_stdlib_authoring="YES"
  fi
fi
echo "PYTHON_PATH=${python_path:-UNAVAILABLE}"
echo "PYTHON_VERSION=${python_version}"
echo "PYTHON_STDLIB_TEXT_AND_BINARY_AUTHORING=${python_stdlib_authoring}"

scene_registry="${ENGINE}/Registry/sceneassetimporter.setreg"
scene_handler="${ENGINE}/Code/Tools/SceneAPI/SceneBuilder/SceneImportRequestHandler.cpp"
scene_builder="${ENGINE}/Gems/SceneProcessing/Code/Source/SceneBuilder/SceneBuilderComponent.cpp"
scene_module="$(find "${BIN}" -maxdepth 2 -iname '*SceneProcessing*' -type f -print 2>/dev/null | head -1 || true)"
scene_plumbing="NO"
if [[ -s "${scene_registry}" && -s "${scene_handler}" && -s "${scene_builder}" && -n "${scene_module}" ]] \
    && grep -Fq 'SupportedFileTypeExtensions' "${scene_registry}" \
    && grep -Fq 'GetSupportedFileExtensions' "${scene_handler}" \
    && grep -Fq 'GetSupportedFileExtensions' "${scene_builder}"; then
  scene_plumbing="YES"
fi
echo "SCENE_IMPORT_REGISTRY=${scene_registry}"
echo "SCENE_IMPORT_HANDLER=${scene_handler}"
echo "SCENE_BUILDER_SOURCE=${scene_builder}"
echo "SCENE_PROCESSING_MODULE=${scene_module:-UNAVAILABLE}"
echo "SCENE_IMPORT_PLUMBING=${scene_plumbing}"

format_support(){
  local name="$1" extension="$2" state="UNVERIFIED"
  if [[ "${scene_plumbing}" == "YES" ]]; then
    if grep -Fq "\".${extension}\"" "${scene_registry}"; then state="YES"; else state="NO"; fi
  fi
  echo "FORMAT_${name}_SUPPORT=${state}"
  echo "FORMAT_${name}_EVIDENCE=registry:${scene_registry} extension=.${extension};handler:SceneImportRequestHandler::GetSupportedFileExtensions;builder:BuilderPluginComponent::Activate;module:${scene_module:-UNAVAILABLE}"
}
format_support OBJ obj
format_support GLTF gltf
format_support GLB glb
format_support FBX fbx

scan_config="${ENGINE}/Registry/AssetProcessorPlatformConfig.setreg"
project_scan="UNVERIFIED"
if [[ -s "${scan_config}" ]] \
    && grep -Fq 'ScanFolder Project/Assets' "${scan_config}" \
    && grep -Fq '"watch": "@PROJECTROOT@"' "${scan_config}" \
    && grep -Fq '"recursive": 1' "${scan_config}"; then
  project_scan="YES"
fi
echo "TRACKED_REPO_SOURCE_LOCATION=stw-o3de/Project/Assets/Weapons/STW_SMG_01"
echo "LIGHTNING_DESTINATION=${PROJECT}/Assets/Weapons/STW_SMG_01"
echo "PROJECT_SCAN_CONFIG=${scan_config}"
echo "PROJECT_SCAN_DISCOVERY=${project_scan}"
echo "TASK_SH_SYNC_CHANGE_REQUIRED=YES"

asset_processor=""
asset_processor_batch=""
for candidate in "${BIN}/AssetProcessor" "${BUILD}/bin/profile/AssetProcessor"; do
  if [[ -x "${candidate}" ]]; then asset_processor="${candidate}"; break; fi
done
for candidate in "${BIN}/AssetProcessorBatch" "${BUILD}/bin/profile/AssetProcessorBatch"; do
  if [[ -x "${candidate}" ]]; then asset_processor_batch="${candidate}"; break; fi
done
echo "ASSET_PROCESSOR_EXECUTABLE=${asset_processor:-UNAVAILABLE}"
echo "ASSET_PROCESSOR_BATCH=${asset_processor_batch:-UNAVAILABLE}"
ap_help=""
ap_help_available="NO"
asset_processor_command="UNVERIFIED"
if [[ -n "${asset_processor_batch}" ]]; then
  ap_help="$(timeout 20 "${asset_processor_batch}" --help --project-path="${PROJECT}" 2>&1 || true)"
  if [[ -n "${ap_help}" ]]; then ap_help_available="YES"; fi
  if grep -Fqi 'project-path' <<< "${ap_help}" && grep -Fqi 'platforms' <<< "${ap_help}"; then
    asset_processor_command="${asset_processor_batch} --project-path=${PROJECT} --platforms=linux"
  fi
fi
echo "ASSET_PROCESSOR_HELP_AVAILABLE=${ap_help_available}"
echo "ASSET_PROCESSOR_HELP_PROJECT_PATH_OPTION=$(grep -Fqi 'project-path' <<< "${ap_help}" && echo YES || echo NO)"
echo "ASSET_PROCESSOR_HELP_PLATFORMS_OPTION=$(grep -Fqi 'platforms' <<< "${ap_help}" && echo YES || echo NO)"
echo "ASSET_PROCESSOR_COMMAND=${asset_processor_command}"
echo "ASSET_PROCESSOR_RUN_THIS_BLOCK=NO"

material_type="${ENGINE}/Gems/Atom/Feature/Common/Assets/Materials/Types/StandardPBR.materialtype"
material_example="${ENGINE}/Gems/Atom/Feature/Common/Assets/Materials/Presets/PBR/metal_aluminum_matte.material"
material_text_authorable="UNVERIFIED"
if [[ -s "${material_type}" && -s "${material_example}" ]] \
    && grep -Fq 'BaseColorPropertyGroup.json' "${material_type}" \
    && grep -Fq 'MetallicPropertyGroup.json' "${material_type}" \
    && grep -Fq 'RoughnessPropertyGroup.json' "${material_type}" \
    && grep -Fq '"baseColor.color"' "${material_example}" \
    && grep -Fq '"metallic.factor"' "${material_example}" \
    && grep -Fq '"roughness.factor"' "${material_example}"; then
  material_text_authorable="YES"
fi
echo "ATOM_MATERIAL_TEXT_AUTHORABLE=${material_text_authorable}"
echo "MATERIAL_EXTENSION=.material"
echo "MATERIAL_TYPE_EVIDENCE=${material_type}"
echo "MATERIAL_SCHEMA_EVIDENCE=${material_example}:baseColor.color,metallic.factor,roughness.factor"

mesh_api="${ENGINE}/Gems/Atom/Feature/Common/Code/Include/Atom/Feature/Mesh/MeshFeatureProcessorInterface.h"
mesh_source="${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay/Code/Source/Clients/STWGameplaySystemComponent.cpp"
runtime_override="UNVERIFIED"
current_material_behavior="UNVERIFIED"
if [[ -s "${mesh_api}" ]] && grep -Fq 'SetCustomMaterials' "${mesh_api}"; then
  runtime_override="YES"
fi
if [[ -s "${mesh_source}" ]] \
    && grep -Fq 'MeshHandleDescriptor descriptor(modelAsset)' "${mesh_source}" \
    && grep -Fq 'AcquireMesh(descriptor)' "${mesh_source}" \
    && ! grep -Fq 'SetCustomMaterials(' "${mesh_source}"; then
  current_material_behavior="MODEL_ASSET_MATERIALS_NO_EXPLICIT_RUNTIME_OVERRIDE"
fi
echo "RUNTIME_MATERIAL_OVERRIDE_API=${runtime_override}"
echo "RUNTIME_MATERIAL_OVERRIDE_EVIDENCE=${mesh_api}:MeshHandleDescriptor,SetCustomMaterials"
echo "CURRENT_MESH_MATERIAL_BEHAVIOR=${current_material_behavior}"
echo "CURRENT_MESH_MATERIAL_EVIDENCE=${mesh_source}:MeshHandleDescriptor(modelAsset),AcquireMesh"

obj_support="$(grep -Fq '".obj"' "${scene_registry}" 2>/dev/null && [[ "${scene_plumbing}" == "YES" ]] && echo YES || echo NO)"
gltf_support="$(grep -Fq '".gltf"' "${scene_registry}" 2>/dev/null && [[ "${scene_plumbing}" == "YES" ]] && echo YES || echo NO)"
selected_format="NONE"
if [[ "${python_stdlib_authoring}" == "YES" && "${obj_support}" == "YES" ]]; then
  selected_format="OBJ"
elif [[ "${python_stdlib_authoring}" == "YES" && "${gltf_support}" == "YES" ]]; then
  selected_format="GLTF"
fi
scripted_path="NO"
if [[ "${selected_format}" != "NONE" && "${project_scan}" == "YES" \
    && "${asset_processor_command}" != "UNVERIFIED" && "${material_text_authorable}" == "YES" ]]; then
  scripted_path="YES"
fi
echo "SELECTED_SCRIPTABLE_FORMAT=${selected_format}"
echo "SCRIPTED_AUTHORING_PATH_AVAILABLE=${scripted_path}"
echo "CACHE_MUTATED_BY_PROBE=NO"
echo "ASSETS_CREATED=NONE"
echo "=================================================="
echo "STW_ASSET_AUTHORING_CAPABILITY_END"
echo "=================================================="
echo "RESULT=PASS"
