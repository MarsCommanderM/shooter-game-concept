#!/usr/bin/env bash
set -Eeuo pipefail

readonly EXPECTED_REPOSITORY="MarsCommanderM/shooter-game-concept"
readonly EXPECTED_BRANCH="brauny/stw-game-production"
readonly BUILT_FROM_COMMIT="d8adba3627911c29db8c9daacdc8793505fd5c4f"
readonly O3DE_COMMIT="3db6943249d8bd7960b9ed7e9aee310b7668586e"
readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly REPORT="${STATE}/asset-and-launch-gate.txt"
readonly STW="${ROOT}/stw-production"
readonly O3DE="${ROOT}/o3de-2605"
readonly GATE="${ROOT}/stw-o3de-worktree"
readonly PROJECT="${GATE}/stw-o3de/Project"
readonly BUILD="${ROOT}/stw-o3de-build/linux"
readonly BIN="${BUILD}/bin/profile"
readonly LAUNCHER="${BIN}/STW.GameLauncher"
readonly AP="${BIN}/AssetProcessorBatch"
readonly AP_CAPTURE="${LOGS}/assetprocessorbatch.stdout-stderr.log"
readonly LAUNCH_CAPTURE="${LOGS}/game-launcher.stdout-stderr.log"
readonly FRAME="${STATE}/stw-native-frame.png"

mkdir -p "${LOGS}"
: >"${REPORT}"
report() { printf '%s\n' "$*" | tee -a "${REPORT}"; }
fail() {
  report "FIRST REAL ERROR: $*"
  report "COST INCURRED: \$0.00"
  exit 1
}
require_executable() {
  local label="$1" path="$2" dep_log
  [[ -f "${path}" ]] || fail "${label} missing: ${path}"
  [[ -x "${path}" ]] || fail "${label} is not executable: ${path}"
  dep_log="${LOGS}/$(printf '%s' "${label}" | tr '[:upper:] ' '[:lower:]-')-ldd.log"
  ldd "${path}" >"${dep_log}" 2>&1 || fail "ldd failed for ${label}; see ${dep_log}"
  ! grep -Fq 'not found' "${dep_log}" || fail "${label} has unresolved dependencies; see ${dep_log}"
  report "${label}: ${path} ($(stat -c '%s bytes; %y' "${path}")); executable; dependencies resolved"
}

[[ "${GITHUB_REPOSITORY:-}" == "${EXPECTED_REPOSITORY}" ]] || fail "Unexpected repository"
[[ "${GITHUB_REF_NAME:-}" == "${EXPECTED_BRANCH}" ]] || fail "Unexpected branch"
[[ -d "${STW}/.git" ]] || fail "Canonical STW checkout missing"
git -C "${STW}" cat-file -e "${BUILT_FROM_COMMIT}^{commit}" || fail "Production commit missing locally"
[[ -z "$(git -C "${STW}" status --porcelain=v1 --untracked-files=all)" ]] || fail "Canonical STW checkout is dirty"
[[ "$(git -C "${O3DE}" rev-parse HEAD)" == "${O3DE_COMMIT}" ]] || fail "O3DE pin mismatch"
[[ -f "${PROJECT}/project.json" ]] || fail "Project not recognized: ${PROJECT}/project.json missing"
[[ -f "${BUILD}/CMakeCache.txt" ]] || fail "Existing configured build tree missing"

report "STW O3DE ASSET + NATIVE LAUNCH GATE"
report "TIMESTAMP: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
report "PRODUCTION STATE: ${BUILT_FROM_COMMIT}"
report "O3DE PIN: ${O3DE_COMMIT}"
report "PROJECT: ${PROJECT}"
require_executable "ASSET PROCESSOR" "${AP}"
require_executable "GAME LAUNCHER" "${LAUNCHER}"

asset_start_epoch="$(date +%s)"
asset_command=("${AP}" "--project-path=${PROJECT}")
report "ASSET PROCESSOR COMMAND: ${asset_command[*]}"
set +e
timeout 7200s env LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
  "${asset_command[@]}" >"${AP_CAPTURE}" 2>&1
asset_rc=$?
set -e
report "ASSET PROCESSOR EXIT CODE: ${asset_rc}"
report "ASSET PROCESSOR STDOUT/STDERR: ${AP_CAPTURE}"

mapfile -t ap_logs < <(find "${PROJECT}" "${STATE}" -type f \
  \( -iname '*AssetProcessor*.log' -o -iname '*AssetBuilder*.log' \) \
  -newermt "@${asset_start_epoch}" -print 2>/dev/null | sort -u)
if ((${#ap_logs[@]})); then
  report "ASSET PROCESSOR LOGS:"
  printf '%s\n' "${ap_logs[@]}" | tee -a "${REPORT}"
else
  report "ASSET PROCESSOR LOGS: none discovered beyond ${AP_CAPTURE}"
fi

combined_ap="${LOGS}/assetprocessor-combined.log"
cp "${AP_CAPTURE}" "${combined_ap}"
for log in "${ap_logs[@]}"; do
  [[ "${log}" == "${AP_CAPTURE}" || "${log}" == "${combined_ap}" ]] && continue
  printf '\n===== %s =====\n' "${log}" >>"${combined_ap}"
  tail -n 5000 "${log}" >>"${combined_ap}" 2>/dev/null || true
done
ap_errors="$(grep -Eic 'Trace::Error|(^|[^[:alpha:]])error([: ]|$)|failed job' "${combined_ap}" || true)"
ap_warnings="$(grep -Eic 'Trace::Warning|(^|[^[:alpha:]])warning([: ]|$)' "${combined_ap}" || true)"
failed_jobs="$(grep -Eic 'failed jobs?[=: ]+[1-9]|jobs? failed[=: ]+[1-9]|Job .* failed' "${combined_ap}" || true)"
processed_summary="$(grep -Ei 'processed|assets? succeeded|jobs? succeeded|assets? remaining|processing complete' "${combined_ap}" | tail -n 20 || true)"
report "ASSET ERROR MATCHES: ${ap_errors}"
report "ASSET WARNING MATCHES: ${ap_warnings}"
report "FAILED JOB MATCHES: ${failed_jobs}"
report "ASSET PROCESSING SUMMARY:"
printf '%s\n' "${processed_summary:-no explicit summary line found}" | tee -a "${REPORT}"

if ((asset_rc != 0)); then
  first="$(grep -n -m1 -Ei 'Trace::Error|(^|[^[:alpha:]])error([: ]|$)|failed job|not found|invalid project' "${combined_ap}" || true)"
  tail -n 160 "${AP_CAPTURE}" | tee -a "${REPORT}"
  fail "AssetProcessorBatch exited ${asset_rc}; ${first:-inspect ${combined_ap}}"
fi
if ((failed_jobs > 0)); then
  first="$(grep -n -m1 -Ei 'failed jobs?[=: ]+[1-9]|jobs? failed[=: ]+[1-9]|Job .* failed' "${combined_ap}" || true)"
  fail "Asset Processor reported failed jobs; ${first:-inspect ${combined_ap}}"
fi

cache_candidates=()
for candidate in "${PROJECT}/Cache" "${PROJECT}/Cache/linux" "${PROJECT}/Cache/pc"; do
  [[ -d "${candidate}" ]] && cache_candidates+=("${candidate}")
done
((${#cache_candidates[@]})) || fail "Asset Processor returned success but no project cache exists"
report "ASSET CACHE:"
for cache in "${cache_candidates[@]}"; do
  du -sh "${cache}" | tee -a "${REPORT}"
done
report "ASSET PROCESSOR STATUS: PASS"

gpu_line="$(nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>&1)" || fail "nvidia-smi failed before launch"
report "GPU RECHECK: ${gpu_line}"
grep -Fqi 'Tesla T4' <<<"${gpu_line}" || fail "Real NVIDIA Tesla T4 not detected before launch"
vulkan_summary="${LOGS}/vulkan-summary.log"
vulkaninfo --summary >"${vulkan_summary}" 2>&1 || fail "vulkaninfo --summary failed"
grep -Fqi 'Tesla T4' "${vulkan_summary}" || fail "Vulkan does not expose NVIDIA Tesla T4"
report "VULKAN RECHECK: NVIDIA Tesla T4 present (${vulkan_summary})"

display_value="${DISPLAY:-}"
if [[ -z "${display_value}" ]]; then
  xvfb_line="$(pgrep -a Xvfb | head -n1 || true)"
  display_value="$(grep -oE ':[0-9]+' <<<"${xvfb_line}" | head -n1 || true)"
fi
[[ -n "${display_value}" ]] || fail "No existing DISPLAY/Xvfb is available"
if command -v xdpyinfo >/dev/null 2>&1; then
  DISPLAY="${display_value}" xdpyinfo >/dev/null 2>&1 || fail "DISPLAY ${display_value} is not reachable"
fi
report "DISPLAY: ${display_value} (existing display; no X server started)"

launch_start_epoch="$(date +%s)"
launch_command=("${LAUNCHER}" "--project-path=${PROJECT}")
report "GAME LAUNCH COMMAND: DISPLAY=${display_value} LD_LIBRARY_PATH=${BIN} ${launch_command[*]}"
env DISPLAY="${display_value}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
  "${launch_command[@]}" >"${LAUNCH_CAPTURE}" 2>&1 &
launcher_pid=$!
report "GAME LAUNCHER PID: ${launcher_pid}"
cleanup_launcher() {
  if kill -0 "${launcher_pid}" 2>/dev/null; then
    kill -TERM "${launcher_pid}" 2>/dev/null || true
    for _ in {1..20}; do
      kill -0 "${launcher_pid}" 2>/dev/null || break
      sleep 0.25
    done
  fi
  wait "${launcher_pid}" 2>/dev/null || true
}
trap cleanup_launcher EXIT

for _ in {1..60}; do
  kill -0 "${launcher_pid}" 2>/dev/null || break
  sleep 1
done
if ! kill -0 "${launcher_pid}" 2>/dev/null; then
  wait "${launcher_pid}" || launch_rc=$?
  launch_rc="${launch_rc:-0}"
  tail -n 180 "${LAUNCH_CAPTURE}" | tee -a "${REPORT}"
  fail "GameLauncher exited before capture with code ${launch_rc}"
fi

mapfile -t launcher_logs < <(find "${PROJECT}" "${STATE}" -type f \
  \( -iname '*.log' -o -iname '*GameLauncher*' \) \
  -newermt "@${launch_start_epoch}" -print 2>/dev/null | sort -u)
runtime_combined="${LOGS}/launcher-combined.log"
cp "${LAUNCH_CAPTURE}" "${runtime_combined}"
for log in "${launcher_logs[@]}"; do
  [[ "${log}" == "${LAUNCH_CAPTURE}" || "${log}" == "${runtime_combined}" ]] && continue
  printf '\n===== %s =====\n' "${log}" >>"${runtime_combined}"
  tail -n 8000 "${log}" >>"${runtime_combined}" 2>/dev/null || true
done
report "LAUNCHER LOGS:"
printf '%s\n' "${launcher_logs[@]:-none discovered beyond ${LAUNCH_CAPTURE}}" | tee -a "${REPORT}"
adapter_evidence="$(grep -Ei 'Tesla T4|NVIDIA.*T4|physical device|adapter|Vulkan|RHI' "${runtime_combined}" | tail -n 80 || true)"
printf '%s\n' "${adapter_evidence:-NO ADAPTER EVIDENCE}" >"${LOGS}/atom-rhi-evidence.log"
grep -Fqi 'Tesla T4' "${LOGS}/atom-rhi-evidence.log" || fail "Launcher logs do not prove Atom/RHI selected the NVIDIA Tesla T4"
grep -Eqi 'Vulkan|RHI.*Vulkan' "${LOGS}/atom-rhi-evidence.log" || fail "Launcher logs do not prove the Vulkan renderer backend"

rm -f "${FRAME}"
if command -v scrot >/dev/null 2>&1; then
  DISPLAY="${display_value}" scrot "${FRAME}"
elif command -v import >/dev/null 2>&1; then
  DISPLAY="${display_value}" import -window root "${FRAME}"
elif command -v ffmpeg >/dev/null 2>&1; then
  dimensions="$(DISPLAY="${display_value}" xdpyinfo 2>/dev/null | awk '/dimensions:/{print $2; exit}')"
  [[ -n "${dimensions}" ]] || fail "Could not determine X11 dimensions for ffmpeg capture"
  DISPLAY="${display_value}" ffmpeg -hide_banner -loglevel error -y -f x11grab \
    -video_size "${dimensions}" -i "${display_value}" -frames:v 1 "${FRAME}" \
    >"${LOGS}/frame-capture.log" 2>&1 || fail "ffmpeg native frame capture failed"
else
  fail "No existing native X11 screenshot tool is available"
fi
[[ -s "${FRAME}" ]] || fail "Native frame file is missing or empty"
frame_meta="$(python3 - "${FRAME}" <<'PY'
import struct, sys
p = sys.argv[1]
with open(p, 'rb') as f:
    h = f.read(24)
if len(h) != 24 or h[:8] != b'\x89PNG\r\n\x1a\n':
    raise SystemExit('not a PNG')
w, hgt = struct.unpack('>II', h[16:24])
print(f'{w}x{hgt}')
PY
)" || fail "Captured frame is not a valid PNG"
report "GAME LAUNCHER STATUS: RUNNING"
report "ATOM/RHI STATUS: PASS; NVIDIA Tesla T4 + Vulkan evidenced in ${LOGS}/atom-rhi-evidence.log"
report "NATIVE FRAME: PASS"
report "FRAME PATH: ${FRAME}"
report "FRAME DIMENSIONS: ${frame_meta}"
report "FRAME TIMESTAMP: $(stat -c '%y' "${FRAME}")"
report "FILES CHANGED: generated asset cache/logs and native frame only; no gameplay/STW source changes"
report "COST INCURRED: \$0.00"
report "FIRST REAL ERROR: NONE"
report "NEXT SAFE ACTION: inspect the captured native frame and runtime logs; do not migrate gameplay yet"
report "DECISION: PASS"

cleanup_launcher
trap - EXIT
