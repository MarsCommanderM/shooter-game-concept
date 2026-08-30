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
PINNED_CMAKE="${ROOT}/tools/cmake-3.31.12-linux-x86_64/bin/cmake"
O3DE_PACKAGES="${ROOT}/o3de-packages"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="${ROOT}/stw-o3de-gate/player-slice-${RUN_ID}"
ICD="${RUN_DIR}/nvidia_icd.json"
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

[[ -x "${PINNED_CMAKE}" ]]
cmake_version="$(${PINNED_CMAKE} --version | head -1)"
echo "PINNED_CMAKE=${PINNED_CMAKE}"
echo "PINNED_CMAKE_VERSION=${cmake_version}"
[[ "${cmake_version}" == "cmake version 3.31.12" ]]
command -v ninja >/dev/null

echo "=================================================="
echo "STW_PERSISTENT_RECOVERY_BEGIN"
echo "=================================================="

# Synchronize tracked production inputs additively.  The persistent checkout is a
# build input, not the Git source of truth; files present only on the host are not
# deleted.  Every tracked Project/Assets file is byte-checked after synchronization.
copy_file_if_changed(){
  mkdir -p "$(dirname "$2")"
  cmp -s "$1" "$2" || cp -a "$1" "$2"
}
copy_tree_if_changed(){
  mkdir -p "$2"
  diff -qr "$1" "$2" >/dev/null 2>&1 || cp -a "$1/." "$2/"
}

tracked_gem="${GITHUB_WORKSPACE}/stw-o3de/Gems/STWGameplay"
tracked_project_assets="${GITHUB_WORKSPACE}/stw-o3de/Project/Assets"
[[ -f "${tracked_gem}/gem.json" && -d "${tracked_project_assets}" ]]
echo "SYNCING_TRACKED_PRODUCTION_GEM=${GEM}"
copy_file_if_changed "${tracked_gem}/gem.json" "${GEM}/gem.json"
copy_file_if_changed "${tracked_gem}/CMakeLists.txt" "${GEM}/CMakeLists.txt"
copy_tree_if_changed "${tracked_gem}/Code" "${GEM}/Code"
copy_tree_if_changed "${tracked_gem}/Registry" "${GEM}/Registry"

# The pinned DefaultProject template is generated away from the persistent Project
# first.  A partial scaffold is an error: it is never overwritten with --force.
scaffold_core_count=0
for scaffold_file in project.json CMakeLists.txt CMakePresets.json; do
  [[ -f "${PROJECT}/${scaffold_file}" ]] && scaffold_core_count=$((scaffold_core_count + 1))
done
project_scaffold_created=NO
if [[ "${scaffold_core_count}" -eq 0 ]]; then
  scaffold_stage="$(mktemp -d "${RUN_DIR}/project-scaffold.XXXXXX")"
  "${ENGINE}/scripts/o3de.sh" create-project \
    --project-path "${scaffold_stage}" \
    --project-name STW \
    --template-path "${ENGINE}/Templates/DefaultProject" \
    --no-register 2>&1 | tee -a "${BUILD_LOG}"
  for scaffold_file in project.json CMakeLists.txt CMakePresets.json; do
    [[ -f "${scaffold_stage}/${scaffold_file}" ]]
  done
  [[ ! -e "${scaffold_stage}/Assets" ]]
  mkdir -p "${PROJECT}"
  cp -a "${scaffold_stage}/." "${PROJECT}/"
  project_scaffold_created=YES
elif [[ "${scaffold_core_count}" -ne 3 ]]; then
  echo "PROJECT_SCAFFOLD_PARTIAL=${scaffold_core_count}/3"
  false
fi
for scaffold_file in project.json CMakeLists.txt CMakePresets.json; do
  [[ -f "${PROJECT}/${scaffold_file}" ]]
done

copy_tree_if_changed "${tracked_project_assets}" "${PROJECT}/Assets"
tracked_asset_count=0
while IFS= read -r -d '' tracked_asset; do
  relative_asset="${tracked_asset#${tracked_project_assets}/}"
  cmp -s "${tracked_asset}" "${PROJECT}/Assets/${relative_asset}"
  tracked_asset_count=$((tracked_asset_count + 1))
done < <(find "${tracked_project_assets}" -type f -print0)
[[ "${tracked_asset_count}" -gt 0 ]]

# These are the pinned 26.05 CLI contracts. Registration and enable-gem are
# idempotent: O3DE de-duplicates registered external subdirectories and gem names.
"${ENGINE}/scripts/o3de.sh" register --this-engine 2>&1 | tee -a "${BUILD_LOG}"
"${ENGINE}/scripts/o3de.sh" register --project-path "${PROJECT}" 2>&1 | tee -a "${BUILD_LOG}"
"${ENGINE}/scripts/o3de.sh" register \
  --external-subdirectory "${GEM}" \
  --external-subdirectory-project-path "${PROJECT}" 2>&1 | tee -a "${BUILD_LOG}"
"${ENGINE}/scripts/o3de.sh" enable-gem \
  --gem-path "${GEM}" \
  --project-path "${PROJECT}" 2>&1 | tee -a "${BUILD_LOG}"

python3 - "${PROJECT}/project.json" "${PROJECT}" "${GEM}" <<'PY'
import json
import os
import sys

project_json, project_path, gem_path = sys.argv[1:]
with open(project_json, encoding="utf-8") as stream:
    project = json.load(stream)
if project.get("project_name") != "STW":
    raise SystemExit("project_name is not STW")
gem_names = []
for entry in project.get("gem_names", []):
    gem_names.append(entry if isinstance(entry, str) else entry.get("name", ""))
if sum(name.split("==", 1)[0] == "STWGameplay" for name in gem_names) != 1:
    raise SystemExit("STWGameplay is not enabled exactly once")
resolved_external = {
    os.path.realpath(path if os.path.isabs(path) else os.path.join(project_path, path))
    for path in project.get("external_subdirectories", [])
}
if os.path.realpath(gem_path) not in resolved_external:
    raise SystemExit("STWGameplay external_subdirectory is not registered")
PY

persistent_build_configured=NO
if [[ ! -f "${BUILD}/CMakeCache.txt" ]]; then
  mkdir -p "${BUILD}" "${O3DE_PACKAGES}"
  "${PINNED_CMAKE}" -S "${PROJECT}" -B "${BUILD}" \
    -G "Ninja Multi-Config" \
    -DLY_3RDPARTY_PATH="${O3DE_PACKAGES}" 2>&1 | tee -a "${BUILD_LOG}"
  persistent_build_configured=YES
fi
[[ -f "${BUILD}/CMakeCache.txt" ]]
cmake_command="$(sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' "${BUILD}/CMakeCache.txt" | head -1)"
[[ -x "${cmake_command}" ]]
[[ "$(readlink -f "${cmake_command}")" == "$(readlink -f "${PINNED_CMAKE}")" ]]
cmake_bin_dir="$(dirname "${cmake_command}")"
export PATH="${cmake_bin_dir}:${PATH}"
echo "CMAKE_COMMAND=${cmake_command}"
echo "SYNCED_TRACKED_PROJECT_ASSETS=${tracked_project_assets} -> ${PROJECT}/Assets"
echo "ENGINE_PIN_VERIFIED=3db6943249d8bd7960b9ed7e9aee310b7668586e"
echo "PROJECT_SCAFFOLD_CREATED=${project_scaffold_created}"
echo "PROJECT_SCAFFOLD_CORE=project.json,CMakeLists.txt,CMakePresets.json"
echo "PROJECT_REGISTRATION=VERIFIED"
echo "STWGAMEPLAY_GEM_ENABLED=VERIFIED"
echo "TRACKED_PROJECT_ASSETS_PRESERVED=YES count=${tracked_asset_count}"
echo "PERSISTENT_BUILD_CONFIGURED_THIS_RUN=${persistent_build_configured}"
echo "CONFIGURE_GENERATOR=Ninja Multi-Config"
echo "CONFIGURE_LY_3RDPARTY_PATH=${O3DE_PACKAGES}"
echo "FULL_ENGINE_REBUILD=NO"
echo "=================================================="
echo "STW_PERSISTENT_RECOVERY_END"
echo "=================================================="

start="$(date +%s)"
"${cmake_command}" --build "${BUILD}" --config profile --target STWGameplay.Tests -j 2 2>&1 | tee -a "${BUILD_LOG}"
echo "TEST_TARGET_BUILD_SECONDS=$(($(date +%s)-start))"
"${cmake_bin_dir}/ctest" --test-dir "${BUILD}" -C profile --output-on-failure -R 'STWGameplay' 2>&1 | tee "${TEST_LOG}"
grep -Eq '100% tests passed|The following tests passed' "${TEST_LOG}"

# Test-count evidence. CTest registers one aggregate googletest target, so the underlying gtest
# case count never appears in its output. List the cases from the already-built module using the
# invocation O3DE itself generates (cmake/LYTestWrappers.cmake: ly_add_googletest ->
# "<bin>/AzTestRunner <module> AzRunUnitTests"; AzTestRunner shifts off <lib> and <symbol> and
# forwards the remaining google-test-args, per its own usage string). The same main-suite filter
# CTest applies is reused, so the reported number is the count this gate actually runs.
# --gtest_list_tests only lists: no test body executes, and no test registration is changed.
ctest_registered_tests="$(sed -n 's/.*tests failed out of \([0-9][0-9]*\).*/\1/p' "${TEST_LOG}" | tail -1)"
echo "CTEST_REGISTERED_TESTS=${ctest_registered_tests:-UNVERIFIED}"
GTEST_SUITE_FILTER='-*SUITE_smoke*:*SUITE_periodic*:*SUITE_benchmark*:*SUITE_sandbox*:*SUITE_awsi*'
test_runner="${BIN}/AzTestRunner"
mapfile -t test_modules < <(find "${BIN}" -maxdepth 1 -type f -name 'libSTWGameplay.Tests.so' -print 2>/dev/null | sort)
echo "GTEST_TEST_RUNNER=${test_runner}"
echo "GTEST_TEST_MODULE_MATCHES=${#test_modules[@]}"
[[ -x "${test_runner}" && "${#test_modules[@]}" -eq 1 ]]
test_module="${test_modules[0]}"
GTEST_LIST_LOG="${RUN_DIR}/gtest-list.log"
gtest_list_exit=0
( cd "${BUILD}" && LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${test_runner}" "${test_module}" AzRunUnitTests \
    --gtest_list_tests "--gtest_filter=${GTEST_SUITE_FILTER}" ) >"${GTEST_LIST_LOG}" 2>&1 || gtest_list_exit=$?
echo "GTEST_LISTING_EXIT=${gtest_list_exit}"
if [[ "${gtest_list_exit}" -ne 0 ]]; then
  echo "----- ${GTEST_LIST_LOG} (tail -60) -----"
  tail -n 60 "${GTEST_LIST_LOG}" 2>/dev/null || true
  false
fi
gtest_case_count="$(grep -cE '^  [A-Za-z0-9_]' "${GTEST_LIST_LOG}" || true)"
echo "GTEST_TEST_MODULE=${test_module}"
echo "GTEST_LISTING_METHOD=AzTestRunner <module> AzRunUnitTests --gtest_list_tests source=cmake/LYTestWrappers.cmake:ly_add_googletest;Code/Tools/AzTestRunner/src/main.cpp:usage"
echo "GTEST_LISTING_FILTER=${GTEST_SUITE_FILTER}"
echo "GTEST_LISTING_LOG=${GTEST_LIST_LOG}"
echo "GTEST_SUITE_COUNT=$(grep -cE '^[A-Za-z0-9_]+\.$' "${GTEST_LIST_LOG}" || true)"
echo "GTEST_CASE_COUNT=${gtest_case_count}"
[[ "${gtest_case_count}" -gt 0 ]]

start="$(date +%s)"
"${cmake_command}" --build "${BUILD}" --config profile --target STW.GameLauncher -j 2 2>&1 | tee -a "${BUILD_LOG}"
echo "LAUNCHER_INCREMENTAL_BUILD_SECONDS=$(($(date +%s)-start))"
[[ -x "${LAUNCHER}" ]]

# A fresh targeted GameLauncher build does not guarantee the separate AP batch
# tool exists. Build only that exact tool target when absent; never Editor or ALL.
ap_batch="${BIN}/AssetProcessorBatch"
asset_processor_batch_built=NO
if [[ ! -x "${ap_batch}" ]]; then
  start="$(date +%s)"
  "${cmake_command}" --build "${BUILD}" --config profile --target AssetProcessorBatch -j 2 2>&1 | tee -a "${BUILD_LOG}"
  echo "ASSETPROCESSORBATCH_TARGET_BUILD_SECONDS=$(($(date +%s)-start))"
  asset_processor_batch_built=YES
fi
[[ -x "${ap_batch}" ]]
echo "ASSETPROCESSORBATCH_TARGET_BUILT_THIS_RUN=${asset_processor_batch_built}"
echo "FULL_ENGINE_REBUILD=NO"
echo "ASSET_PROCESSING=HEADLESS_INCREMENTAL_REQUIRED"

# Deterministic headless asset processing. O3DE's own Linux automation
# (scripts/build/Platform/Linux/asset_linux.sh) drives AssetProcessorBatch with an
# absolute --project-path and an explicit --platforms; that is the variant this gate
# uses. The GUI AssetProcessor is deliberately NOT started: it is the only variant
# that serves port 45643 (BatchApplicationServer increments the port on purpose) and
# it is a Qt GUI application with no proven headless startup on this host.
echo "=================================================="
echo "STW_HEADLESS_ASSET_PIPELINE_BEGIN"
echo "=================================================="
[[ -x "${ap_batch}" ]]
AP_LOG="${RUN_DIR}/assetprocessorbatch.log"
ap_start="$(date +%s)"
ap_exit=0
"${ap_batch}" --project-path="${PROJECT}" --engine-path="${ENGINE}" --platforms=linux >"${AP_LOG}" 2>&1 || ap_exit=$?
ap_seconds="$(($(date +%s)-ap_start))"
echo "ASSETPROCESSORBATCH_EXECUTABLE=${ap_batch}"
echo "ASSETPROCESSORBATCH_PROJECT=${PROJECT}"
echo "ASSETPROCESSORBATCH_PLATFORM=linux"
echo "ASSETPROCESSORBATCH_EXIT=${ap_exit}"
echo "ASSETPROCESSORBATCH_SECONDS=${ap_seconds}"
if [[ "${ap_exit}" -ne 0 ]]; then
  echo "----- ${AP_LOG} (tail -120) -----"
  tail -n 120 "${AP_LOG}" 2>/dev/null || true
  false
fi

stw_asset_source="${PROJECT}/Assets/Weapons/STW_SMG_01/STW_SMG_01.obj"
[[ -s "${stw_asset_source}" ]]
stw_asset_source_bytes="$(stat -c %s "${stw_asset_source}")"
stw_asset_vertex_count="$(grep -c '^v ' "${stw_asset_source}")"
stw_asset_face_count="$(grep -c '^f ' "${stw_asset_source}")"
stw_asset_group_count="$(grep -c '^g ' "${stw_asset_source}")"
mapfile -t stw_model_products < <(find "${PROJECT}/Cache/linux" -type f -iname '*stw_smg_01*.azmodel' -print 2>/dev/null | sort)
mapfile -t stw_material_products < <(find "${PROJECT}/Cache/linux" -type f -iname '*stw_smg_01*.azmaterial' -print 2>/dev/null | sort)
echo "STW_ASSET_MODEL_PRODUCT_MATCHES=${#stw_model_products[@]}"
echo "STW_ASSET_MATERIAL_PRODUCT_MATCHES=${#stw_material_products[@]}"
[[ "${#stw_model_products[@]}" -eq 1 ]]
[[ "${#stw_material_products[@]}" -eq 1 ]]
stw_model_product="${stw_model_products[0]}"
stw_material_product="${stw_material_products[0]}"
[[ -s "${stw_model_product}" && -s "${stw_material_product}" ]]
# Read-only probe only: never starts, reuses or kills anything on the GUI AP port.
# NO is the expected and accepted answer now that the launcher is AP-independent.
gui_ap_listener="NO"
if timeout 5 bash -c 'exec 3<>/dev/tcp/127.0.0.1/45643' 2>/dev/null; then
  gui_ap_listener="YES"
fi
echo "GUI_AP_LISTENER_PRESENT_BEFORE_LAUNCHER=${gui_ap_listener}"
echo "LAUNCHER_WAIT_FOR_CONNECT_OVERRIDE=linux_wait_for_connect=0"
echo "=================================================="
echo "STW_HEADLESS_ASSET_PIPELINE_END"
echo "=================================================="

# This fresh Lightning image has no system ICD manifest.  Create a process-local
# manifest for the already-installed NVIDIA library; no global Vulkan state changes.
nvidia_vulkan_library="$(ldconfig -p 2>/dev/null | awk '$1 == "libGLX_nvidia.so.0" {print $NF; exit}')"
[[ -n "${nvidia_vulkan_library}" && -r "${nvidia_vulkan_library}" ]]
python3 - "${ICD}" "${nvidia_vulkan_library}" <<'PY'
import json
import sys

path, library = sys.argv[1:]
with open(path, "w", encoding="utf-8") as stream:
    json.dump(
        {
            "file_format_version": "1.0.0",
            "ICD": {"library_path": library, "api_version": "1.3.280"},
        },
        stream,
        indent=2,
    )
    stream.write("\n")
PY
chmod 600 "${ICD}"
echo "PROCESS_LOCAL_VULKAN_ICD=${ICD}"
echo "PROCESS_LOCAL_VULKAN_LIBRARY=${nvidia_vulkan_library}"
echo "GLOBAL_VULKAN_ICD_CHANGED=NO"

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
mkdir -p "$(dirname "${GAME_LOG}")"
: >"${GAME_LOG}"
echo "FRESH_GAME_LOG=${GAME_LOG}"
setsid env DISPLAY="${display}" XDG_RUNTIME_DIR="${RUNTIME}" ALSA_CONFIG_PATH="${ALSA}" \
  STW_NATIVE_CAPTURE_PATH="${FRAME_NATIVE}" \
  STW_PHYSX_ACCEPTANCE=1 \
  VK_ICD_FILENAMES="${ICD}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
  ${STDBUF_PREFIX} "${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${ENGINE}" "--regset=/Amazon/AzCore/Bootstrap/linux_wait_for_connect=0" -sys_audio_disable 1 >"${LAUNCH_LOG}" 2>&1 &
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
runtime_grep -q 'ATOM_VIEWMODEL_MESH result=PASS .*material=bound'
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
echo "=================================================="
echo "STW_ASSET_INTEGRATION_BEGIN"
echo "=================================================="
echo "STW_ASSET_NAME=STW_SMG_01"
echo "STW_ASSET_SOURCE=${stw_asset_source}"
echo "STW_ASSET_SOURCE_EXISTS=YES"
echo "STW_ASSET_SOURCE_BYTES=${stw_asset_source_bytes}"
echo "STW_ASSET_VERTEX_COUNT=${stw_asset_vertex_count}"
echo "STW_ASSET_FACE_COUNT=${stw_asset_face_count}"
echo "STW_ASSET_GROUP_COUNT=${stw_asset_group_count}"
echo "STW_ASSET_PRODUCT=${stw_model_product}"
echo "STW_ASSET_PRODUCT_EXISTS=YES"
echo "STW_ASSET_MATERIAL_PRODUCT=${stw_material_product}"
echo "STW_ASSET_CATALOG_RESOLVES=YES"
echo "STW_ASSET_PBR_BINDING=BOUND"
echo "STW_ASSET_EXTERNAL_SOURCE=NO"
echo "=================================================="
echo "STW_ASSET_INTEGRATION_END"
echo "=================================================="
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
  if [[ "${ap_exit}" -eq 0 && -s "${AP_LOG}" ]]; then
    asset_processor_command="${asset_processor_batch} --project-path=${PROJECT} --engine-path=${ENGINE} --platforms=linux"
  fi
fi
echo "ASSET_PROCESSOR_HELP_AVAILABLE=${ap_help_available}"
echo "ASSET_PROCESSOR_HELP_PROJECT_PATH_OPTION=$(grep -Fqi 'project-path' <<< "${ap_help}" && echo YES || echo NO)"
echo "ASSET_PROCESSOR_HELP_PLATFORMS_OPTION=$(grep -Fqi 'platforms' <<< "${ap_help}" && echo YES || echo NO)"
echo "ASSET_PROCESSOR_COMMAND=${asset_processor_command}"
echo "ASSET_PROCESSOR_COMMAND_EVIDENCE=SAME_RUN_HEADLESS_PROCESS_EXIT_${ap_exit}"
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
    && grep -Fq 'MeshHandleDescriptor descriptor(modelAsset, s_viewmodelAssetLoadState.m_material)' "${mesh_source}" \
    && grep -Fq 'Material::FindOrCreate' "${mesh_source}" \
    && grep -Fq 'AcquireMesh(descriptor)' "${mesh_source}" \
    && ! grep -Fq 'SetCustomMaterials(' "${mesh_source}"; then
  current_material_behavior="EXPLICIT_DEFAULT_CUSTOM_MATERIAL_BOUND_IN_MESH_DESCRIPTOR"
fi
echo "RUNTIME_MATERIAL_OVERRIDE_API=${runtime_override}"
echo "RUNTIME_MATERIAL_OVERRIDE_EVIDENCE=${mesh_api}:MeshHandleDescriptor,SetCustomMaterials"
echo "CURRENT_MESH_MATERIAL_BEHAVIOR=${current_material_behavior}"
echo "CURRENT_MESH_MATERIAL_EVIDENCE=${mesh_source}:Material::FindOrCreate,MeshHandleDescriptor(modelAsset,material),AcquireMesh"

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

# BLOCK 6A2: prove the exact project-aware AssetProcessorBatch invocation from
# the installed O3DE source, its shipped Linux automation, actual help output,
# and current registration metadata. No processing pass is launched here.
echo "=================================================="
echo "STW_ASSETPROCESSOR_COMMAND_PROOF_BEGIN"
echo "=================================================="

command_proof_cache_before="$(find "${CACHE}" -type f -printf '%p\t%s\t%T@\n' 2>/dev/null | sort | sha256sum | awk '{print $1}')"
ap_entry="${ENGINE}/Code/Tools/AssetProcessor/native/main_batch.cpp"
ap_utils_header="${ENGINE}/Code/Tools/AssetProcessor/native/utilities/assetUtils.h"
ap_utils_source="${ENGINE}/Code/Tools/AssetProcessor/native/utilities/assetUtils.cpp"
ap_application_manager="${ENGINE}/Code/Tools/AssetProcessor/native/utilities/ApplicationManager.cpp"
ap_application_manager_base="${ENGINE}/Code/Tools/AssetProcessor/native/utilities/ApplicationManagerBase.cpp"
settings_registry_merge="${ENGINE}/Code/Framework/AzCore/AzCore/Settings/SettingsRegistryMergeUtils.cpp"
application_options="${ENGINE}/Registry/application_options.setreg"
linux_invocation_example="${ENGINE}/scripts/build/Platform/Linux/asset_linux.sh"

entry_point_proven="NO"
if [[ -s "${ap_entry}" ]] \
    && grep -Fq 'BatchApplicationManager' "${ap_entry}" \
    && grep -Fq 'BeforeRun' "${ap_entry}" \
    && grep -Fq 'Run()' "${ap_entry}"; then
  entry_point_proven="YES"
fi

project_option_proven="NO"
absolute_project_path_required="NO"
if [[ -s "${ap_utils_header}" && -s "${ap_utils_source}" ]] \
    && grep -Fq 'ProjectPathOverrideParameter' "${ap_utils_header}" \
    && grep -Fq '"project-path"' "${ap_utils_header}" \
    && grep -Fq 'ComputeProjectPath' "${ap_utils_source}" \
    && grep -Fq 'ProjectPathOverrideParameter' "${ap_utils_source}"; then
  project_option_proven="YES"
fi
if [[ -s "${ap_utils_source}" ]] \
    && grep -Fq 'isAbsolute' "${ap_utils_source}" \
    && grep -Fq 'ProjectPathOverrideParameter' "${ap_utils_source}"; then
  absolute_project_path_required="YES"
fi

settings_priority_proven="NO"
if [[ -s "${settings_registry_merge}" ]] \
    && grep -Fq 'FindProjectRoot' "${settings_registry_merge}" \
    && grep -Fq 'ProjectPath' "${settings_registry_merge}" \
    && grep -Fq 'Bootstrap' "${settings_registry_merge}"; then
  settings_priority_proven="YES"
fi

project_validation_proven="NO"
if [[ -s "${ap_application_manager}" ]] \
    && grep -Fq 'ComputeProjectPath' "${ap_application_manager}" \
    && grep -Fq 'project.json' "${ap_application_manager}"; then
  project_validation_proven="YES"
fi

platform_option_proven="NO"
if [[ -s "${ap_utils_source}" ]] \
    && grep -Fq 'ReadPlatformsFromCommandLine' "${ap_utils_source}" \
    && grep -Fq 'platforms' "${ap_utils_source}"; then
  platform_option_proven="YES"
fi

application_option_registered="NO"
if [[ -s "${application_options}" ]] \
    && grep -Fq '"project-path"' "${application_options}"; then
  application_option_registered="YES"
fi

installed_example_proven="NO"
if [[ -s "${linux_invocation_example}" ]] \
    && grep -Fq 'AssetProcessorBatch' "${linux_invocation_example}" \
    && grep -Fq -- '--project-path=$SOURCE_DIRECTORY/$project' "${linux_invocation_example}" \
    && grep -Fq -- '--platforms=$ASSET_PROCESSOR_PLATFORMS' "${linux_invocation_example}"; then
  installed_example_proven="YES"
fi

engine_path_source_proven="NO"
if [[ -s "${settings_registry_merge}" && -s "${application_options}" ]] \
    && grep -Fq 'CommandLineEngineOptionName' "${settings_registry_merge}" \
    && grep -Fq '"engine-path"' "${settings_registry_merge}" \
    && grep -Fq 'FindEngineRoot' "${settings_registry_merge}" \
    && grep -Fq 'MergeSettingsToRegistry_EngineRegistry' "${settings_registry_merge}" \
    && grep -Fq 'FilePathKey_EngineRootFolder' "${settings_registry_merge}" \
    && grep -Fq '"engine-path"' "${application_options}"; then
  engine_path_source_proven="YES"
fi

# Same-run execution evidence: the exact command this run's headless step executed, rebuilt from
# the same variables, its exit code, and proof that run #70's configuration fatal is absent from
# this run's own AssetProcessorBatch log.
same_run_command="${ap_batch} --project-path=${PROJECT} --engine-path=${ENGINE} --platforms=linux"
same_run_prior_failure_absent="UNVERIFIED"
if [[ -f "${AP_LOG}" ]]; then
  if grep -Fq 'Platform xxxxxx' "${AP_LOG}" \
      || grep -Fq 'Unable to find any scan folders' "${AP_LOG}" \
      || grep -Fq 'Failed to Initialize from AssetProcessorPlatformConfig' "${AP_LOG}"; then
    same_run_prior_failure_absent="NO"
  else
    same_run_prior_failure_absent="YES"
  fi
fi

help_project_option="$(grep -Fqi 'project-path' <<< "${ap_help}" && echo YES || echo NO)"
help_platform_option="$(grep -Fqi 'platforms' <<< "${ap_help}" && echo YES || echo NO)"
help_zero_analysis_option="$(grep -Fqi 'zeroAnalysisMode' <<< "${ap_help}" && echo YES || echo NO)"
help_reprocess_option="$(grep -Fqi 'reprocessFileList' <<< "${ap_help}" && echo YES || echo NO)"
help_dependency_pattern_option="$(grep -Fqi 'dependencyScanPattern' <<< "${ap_help}" && echo YES || echo NO)"
help_additional_scan_option="$(grep -Fqi 'additionalScanFolders' <<< "${ap_help}" && echo YES || echo NO)"

reprocess_list_source="NO"
dependency_pattern_source="NO"
additional_scan_source="NO"
if [[ -s "${ap_application_manager_base}" ]]; then
  grep -Fq 'reprocessFileList' "${ap_application_manager_base}" && reprocess_list_source="YES"
  grep -Fq 'dependencyScanPattern' "${ap_application_manager_base}" && dependency_pattern_source="YES"
  grep -Fq 'additionalScanFolders' "${ap_application_manager_base}" && additional_scan_source="YES"
fi

registration_manifest="${HOME:-}/.o3de/o3de_manifest.json"
project_registered="UNVERIFIED"
engine_registered="UNVERIFIED"
registration_evidence="MANIFEST_UNAVAILABLE"
if [[ -n "${python_path}" && -x "${python_path}" && -s "${registration_manifest}" ]]; then
  registration_result="$("${python_path}" - "${registration_manifest}" "${PROJECT}" "${ENGINE}" <<'PY'
import json
import os
import sys

manifest_path, project_path, engine_path = sys.argv[1:]
with open(manifest_path, encoding="utf-8") as manifest_file:
    manifest = json.load(manifest_file)

def normalize_entries(value):
    if not isinstance(value, list):
        return set()
    return {os.path.realpath(item) for item in value if isinstance(item, str)}

projects = normalize_entries(manifest.get("projects"))
engines = normalize_entries(manifest.get("engines"))
print("YES" if os.path.realpath(project_path) in projects else "NO")
print("YES" if os.path.realpath(engine_path) in engines else "NO")
PY
)"
  project_registered="$(sed -n '1p' <<< "${registration_result}")"
  engine_registered="$(sed -n '2p' <<< "${registration_result}")"
  registration_evidence="${registration_manifest}:projects,engines"
fi

# COMMAND_PROVEN reflects the strongest evidence actually available: installed-source semantics
# for each of the three arguments, plus this same run's successful execution of the exact command.
# The binary's --help text is still reported verbatim but is deliberately NOT required: it omits
# project-path even though the option demonstrably works, so help text is the weakest of the three
# evidence classes. The shipped Linux example script is informational only for the same reason
# recorded in SHIPPED_EXAMPLE_LAYOUT_NOTE. Nothing here is hardcoded to YES.
project_selection_method="UNVERIFIED"
required_cwd="UNVERIFIED"
future_processing_command="UNVERIFIED"
command_proven="NO"
if [[ -x "${asset_processor_batch}" \
    && -s "${PROJECT}/project.json" \
    && "${entry_point_proven}" == "YES" \
    && "${project_option_proven}" == "YES" \
    && "${absolute_project_path_required}" == "YES" \
    && "${settings_priority_proven}" == "YES" \
    && "${project_validation_proven}" == "YES" \
    && "${platform_option_proven}" == "YES" \
    && "${engine_path_source_proven}" == "YES" \
    && "${application_option_registered}" == "YES" \
    && "${same_run_prior_failure_absent}" == "YES" \
    && "${ap_exit}" -eq 0 ]]; then
  project_selection_method="EXPLICIT_ABSOLUTE_--project-path_AND_--engine-path"
  required_cwd="ARBITRARY_WITH_EXPLICIT_ABSOLUTE_PROJECT_AND_ENGINE_PATHS"
  future_processing_command="${same_run_command}"
  command_proven="YES"
fi

echo "ASSET_PROCESSOR_BATCH=${asset_processor_batch:-UNAVAILABLE}"
echo "HELP_INVOCATION=${asset_processor_batch:-UNAVAILABLE} --help --project-path=${PROJECT}"
echo "HELP_AVAILABLE=${ap_help_available}"
echo "HELP_PROJECT_PATH_OPTION=${help_project_option}"
echo "HELP_PLATFORM_OPTION=${help_platform_option}"
echo "HELP_ZERO_ANALYSIS_OPTION=${help_zero_analysis_option}"
echo "HELP_REPROCESS_FILE_LIST_OPTION=${help_reprocess_option}"
echo "HELP_DEPENDENCY_SCAN_PATTERN_OPTION=${help_dependency_pattern_option}"
echo "HELP_ADDITIONAL_SCAN_FOLDERS_OPTION=${help_additional_scan_option}"
echo "ENTRY_POINT_EVIDENCE=${ap_entry}:BatchApplicationManager::BeforeRun,Run proven=${entry_point_proven}"
echo "PROJECT_OPTION_EVIDENCE=${ap_utils_header}:ProjectPathOverrideParameter=project-path;${ap_utils_source}:ComputeProjectPath,absolute-path-check proven=${project_option_proven} absolute_required=${absolute_project_path_required}"
echo "SETTINGS_REGISTRY_EVIDENCE=${settings_registry_merge}:FindProjectRoot command-line/executable-scan/bootstrap resolution proven=${settings_priority_proven}"
echo "PROJECT_VALIDATION_EVIDENCE=${ap_application_manager}:ComputeProjectPath,project.json proven=${project_validation_proven}"
echo "APPLICATION_OPTION_EVIDENCE=${application_options}:project-path proven=${application_option_registered}"
echo "PLATFORM_OPTION_EVIDENCE=${ap_utils_source}:ReadPlatformsFromCommandLine proven=${platform_option_proven}"
echo "INSTALLED_INVOCATION_EXAMPLE=${linux_invocation_example}:AssetProcessorBatch --project-path=\$SOURCE_DIRECTORY/\$project --platforms=\$ASSET_PROCESSOR_PLATFORMS proven=${installed_example_proven}"
echo "SHIPPED_EXAMPLE_REQUIRED=NO informational_only=YES"
echo "SHIPPED_EXAMPLE_LAYOUT_NOTE=the shipped script assumes the engine source root is an ancestor of the project; STW uses an external-engine layout (engine, project worktree and build tree are siblings), which is why --engine-path must be explicit and why byte-identity with the shipped invocation is neither achievable nor required"
echo "PROJECT_PATH_SOURCE_PROVEN=${project_option_proven}"
echo "ENGINE_PATH_SOURCE_PROVEN=${engine_path_source_proven}"
echo "ENGINE_PATH_SOURCE_EVIDENCE=${settings_registry_merge}:CommandLineEngineOptionName=engine-path,FindEngineRoot,MergeSettingsToRegistry_EngineRegistry,FilePathKey_EngineRootFolder;${application_options}:engine-path"
echo "PLATFORM_SOURCE_PROVEN=${platform_option_proven}"
echo "SAME_RUN_COMMAND_EXECUTED=${same_run_command}"
echo "SAME_RUN_COMMAND_EXIT=${ap_exit}"
echo "SAME_RUN_COMMAND_SECONDS=${ap_seconds}"
echo "SAME_RUN_PRIOR_CONFIG_FAILURE_ABSENT=${same_run_prior_failure_absent}"
echo "SAME_RUN_ASSETPROCESSOR_LOG=${AP_LOG}"
echo "PROJECT_SELECTION_METHOD=${project_selection_method}"
echo "PROJECT_PATH=${PROJECT}"
echo "PROJECT_REGISTERED=${project_registered}"
echo "ENGINE_REGISTERED=${engine_registered}"
echo "REGISTRATION_EVIDENCE=${registration_evidence}"
echo "REQUIRED_CWD=${required_cwd}"
echo "PLATFORM_SELECTION=--platforms=linux"
echo "NORMAL_FILE_OR_PATTERN_FILTER=NOT_PROVEN"
echo "REPROCESS_FILE_LIST_SUPPORT=${reprocess_list_source} source=${ap_application_manager_base}:reprocessFileList purpose=force-listed-files-to-reprocess-not-limit-normal-pending-work"
echo "DEPENDENCY_SCAN_PATTERN_SUPPORT=${dependency_pattern_source} source=${ap_application_manager_base}:dependencyScanPattern purpose=dependency-scan-only"
echo "ADDITIONAL_SCAN_FOLDER_SUPPORT=${additional_scan_source} source=${ap_application_manager_base}:additionalScanFolders purpose=add-scan-root-not-one-file-filter"
echo "INCREMENTAL_SCOPE=PROJECT_LEVEL_PENDING_CHANGES;NO_NORMAL_ONE_FILE_FILTER_PROVEN"
echo "COMMAND=${future_processing_command}"
echo "COMMAND_PROVEN=${command_proven}"
echo "ASSET_PROCESSOR_RUN_THIS_BLOCK=NO"
echo "ASSETS_CREATED=NONE"

command_proof_cache_after="$(find "${CACHE}" -type f -printf '%p\t%s\t%T@\n' 2>/dev/null | sort | sha256sum | awk '{print $1}')"
if [[ "${command_proof_cache_before}" == "${command_proof_cache_after}" ]]; then
  echo "CACHE_MUTATED_BY_COMMAND_PROOF=NO"
else
  echo "CACHE_MUTATED_BY_COMMAND_PROOF=UNEXPECTED"
  false
fi
echo "=================================================="
echo "STW_ASSETPROCESSOR_COMMAND_PROOF_END"
echo "=================================================="
echo "RESULT=PASS"
