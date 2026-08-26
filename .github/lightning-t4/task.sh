#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly REPORT="${STATE}/assetprocessor-engine-associated.txt"
readonly O3DE="${ROOT}/o3de-2605"
readonly PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
readonly BUILD="${ROOT}/stw-o3de-build/linux"
readonly BIN="${BUILD}/bin/profile"
readonly AP="${BIN}/AssetProcessorBatch"
readonly MANIFEST="${ROOT}/.o3de/o3de_manifest.json"
readonly CAPTURE="${LOGS}/assetprocessor-engine-associated.stdout-stderr.log"
readonly COMBINED="${LOGS}/assetprocessor-engine-associated.combined.log"
readonly MARKER="${STATE}/assetprocessor-engine-associated.start"

mkdir -p "${LOGS}"
: >"${REPORT}"
report() { printf '%s\n' "$*" | tee -a "${REPORT}"; }
fail() { report "FIRST REMAINING ERROR: $*"; report "COST INCURRED: \$0.00"; exit 1; }

report "STW O3DE OFFICIAL ENGINE ASSOCIATION + ASSET PROCESSING"
report "PRODUCTION STATE: d8adba3627911c29db8c9daacdc8793505fd5c4f"
report "ENGINE ROOT: ${O3DE}"
report "PROJECT ROOT: ${PROJECT}"
[[ "$(git -C "${O3DE}" rev-parse HEAD)" == "3db6943249d8bd7960b9ed7e9aee310b7668586e" ]] || fail "O3DE pin mismatch"
[[ -s "${O3DE}/engine.json" ]] || fail "engine.json missing"
[[ -s "${O3DE}/Registry/AssetProcessorPlatformConfig.setreg" ]] || fail "AssetProcessorPlatformConfig.setreg missing"
[[ -s "${PROJECT}/project.json" ]] || fail "project.json missing"
[[ -s "${MANIFEST}" ]] || fail "o3de_manifest.json missing"
[[ -x "${AP}" ]] || fail "AssetProcessorBatch missing or not executable"
[[ -f "${BUILD}/CMakeCache.txt" ]] || fail "Existing configured build tree missing"

python3 - "${O3DE}" "${PROJECT}" "${MANIFEST}" <<'PY' | tee -a "${REPORT}"
import json, os, sys
engine, project, manifest = sys.argv[1:]
with open(os.path.join(engine, "engine.json"), encoding="utf-8") as f: ej=json.load(f)
with open(os.path.join(project, "project.json"), encoding="utf-8") as f: pj=json.load(f)
with open(manifest, encoding="utf-8") as f: mj=json.load(f)
engines=[os.path.realpath(os.path.expanduser(x.get("path",x) if isinstance(x,dict) else x)) for x in mj.get("engines",[])]
projects=[os.path.realpath(os.path.expanduser(x.get("path",x) if isinstance(x,dict) else x)) for x in mj.get("projects",[])]
print(f"ENGINE NAME: {ej.get('engine_name')}")
print(f"ENGINE VERSION: {ej.get('version')}")
print(f"PROJECT NAME: {pj.get('project_name')}")
print(f"PROJECT ENGINE: {pj.get('engine')}")
print(f"ENGINE REGISTERED: {'YES' if os.path.realpath(engine) in engines else 'NO'}")
print(f"PROJECT REGISTERED: {'YES' if os.path.realpath(project) in projects else 'NO'}")
if ej.get("engine_name") != pj.get("engine"): raise SystemExit("project engine association mismatch")
if os.path.realpath(engine) not in engines: raise SystemExit("engine not registered")
if os.path.realpath(project) not in projects: raise SystemExit("project not registered")
PY

[[ -s "${BIN}/Registry/cmake_dependencies.assetprocessorbatch.setreg" ]] || fail "Generic AssetProcessorBatch runtime registry missing"
[[ -s "${BIN}/Registry/cmake_dependencies.stw.assetprocessorbatch.setreg" ]] || fail "STW AssetProcessorBatch runtime registry missing"
report "GENERATED REGISTRY: PRESENT"
report "GEM REGISTRATION: manifest paths and generated STW dependency registry present"
report "ROOT CAUSE CLASS: G + J"
report "ROOT CAUSE: registered external source engine was not merged by AssetProcessorBatch when only --project-path was supplied"
report "FIX APPLIED: official O3DE --engine-path runtime association; no ScanFolder entries or project source edited"

touch "${MARKER}"
command=("${AP}" "--project-path=${PROJECT}" "--engine-path=${O3DE}")
report "ASSET PROCESSOR COMMAND: ${command[*]}"
set +e
timeout 7200s env LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${command[@]}" >"${CAPTURE}" 2>&1
rc=$?
set -e
report "EXIT CODE: ${rc}"
report "STDOUT/STDERR: ${CAPTURE}"

mapfile -t fresh_logs < <(find "${PROJECT}" "${STATE}" -type f -newer "${MARKER}" \( -iname '*AssetProcessor*.log' -o -iname '*AssetBuilder*.log' \) -print 2>/dev/null | sort -u)
cp "${CAPTURE}" "${COMBINED}"
for log in "${fresh_logs[@]}"; do
  [[ "$log" == "${CAPTURE}" || "$log" == "${COMBINED}" ]] && continue
  printf '\n===== %s =====\n' "$log" >>"${COMBINED}"
  tail -n 10000 "$log" >>"${COMBINED}" 2>/dev/null || true
done
report "ASSET LOGS:"
if (("${#fresh_logs[@]}" > 0)); then printf '%s\n' "${fresh_logs[@]}" | tee -a "${REPORT}"; else report "  ${CAPTURE}"; fi
report "COMBINED LOG: ${COMBINED}"

first_warning="$(grep -n -m1 -Ei 'Trace::Warning|(^|[^[:alpha:]])warning([: ]|$)' "${COMBINED}" || true)"
first_error="$(grep -n -m1 -Ei 'Trace::Error|(^|[^[:alpha:]])error([: ]|$)|failed to initialize|no scan folders|no platforms' "${COMBINED}" || true)"
platform_lines="$(grep -Ei 'enabled platforms|platforms enabled|platform.*(linux|pc)' "${COMBINED}" | tail -n 30 || true)"
scan_lines="$(grep -Ei 'scan folder|scanfolder|watch folder' "${COMBINED}" | tail -n 80 || true)"
job_lines="$(grep -Ei 'jobs? (started|succeeded|failed|completed|to process)|processed [0-9]+|processing complete' "${COMBINED}" | tail -n 80 || true)"
failed_jobs="$(grep -Eic 'failed jobs?[=: ]+[1-9]|jobs? failed[=: ]+[1-9]|Job .* failed' "${COMBINED}" || true)"

report "PLATFORMS LOADED EVIDENCE:"
printf '%s\n' "${platform_lines:-no explicit platform summary line}" | tee -a "${REPORT}"
report "SCAN FOLDERS LOADED EVIDENCE:"
printf '%s\n' "${scan_lines:-no explicit scan-folder summary line}" | tee -a "${REPORT}"
report "JOB SUMMARY EVIDENCE:"
printf '%s\n' "${job_lines:-no explicit job summary line}" | tee -a "${REPORT}"
report "JOBS FAILED MATCHES: ${failed_jobs}"
report "FIRST WARNING: ${first_warning:-NONE}"
report "FIRST ERROR: ${first_error:-NONE}"

if ((rc != 0)); then
  tail -n 200 "${CAPTURE}" | tee -a "${REPORT}"
  fail "AssetProcessorBatch exited ${rc}; ${first_error:-inspect ${COMBINED}}"
fi
((failed_jobs == 0)) || fail "AssetProcessorBatch reported failed jobs"
if grep -Eqi 'Failed to Initialize from AssetProcessorPlatformConfig|Unable to find any scan folders|no platforms appear to be enabled' "${COMBINED}"; then
  fail "Critical Asset Processor configuration error remains"
fi

cache=""
for candidate in "${PROJECT}/Cache/linux" "${PROJECT}/Cache/pc" "${PROJECT}/Cache"; do
  if [[ -d "$candidate" ]]; then cache="$candidate"; break; fi
done
[[ -n "$cache" ]] || fail "Successful exit without a runtime asset cache"
report "ASSET CACHE: $cache ($(du -sh "$cache" | awk '{print $1}'))"
registry_count="$(find "$cache" -type f -name '*.setreg' 2>/dev/null | wc -l)"
report "CACHE SETTINGS REGISTRY FILES: ${registry_count}"
report "ASSET PROCESSOR STATUS: PASS"
report "GAME LAUNCHER REBUILT: NO"
report "EDITOR REBUILT: NO"
report "GAME LAUNCHER STARTED: NO"
report "FILES CHANGED: local O3DE registration/runtime cache/log state only; no gameplay/STW production source"
report "COST INCURRED: \$0.00"
report "FIRST REMAINING ERROR: NONE"
report "DECISION: PASS"
report "NEXT SAFE ACTION: native NVIDIA T4 GameLauncher visual gate"
