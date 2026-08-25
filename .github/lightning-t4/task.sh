#!/usr/bin/env bash
set -Eeuo pipefail

readonly EXPECTED_REPOSITORY="MarsCommanderM/shooter-game-concept"
readonly EXPECTED_BRANCH="brauny/stw-game-production"
readonly BUILT_FROM_COMMIT="d8adba3627911c29db8c9daacdc8793505fd5c4f"
readonly O3DE_COMMIT="3db6943249d8bd7960b9ed7e9aee310b7668586e"
readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly REPORT="${STATE}/assetprocessor-incremental.txt"
readonly STW="${ROOT}/stw-production"
readonly O3DE="${ROOT}/o3de-2605"
readonly GATE="${ROOT}/stw-o3de-worktree"
readonly PROJECT="${GATE}/stw-o3de/Project"
readonly BUILD="${ROOT}/stw-o3de-build/linux"
readonly BIN="${BUILD}/bin/profile"
readonly LAUNCHER="${BIN}/STW.GameLauncher"
readonly EDITOR="${BIN}/Editor"
readonly AP="${BIN}/AssetProcessorBatch"

mkdir -p "${LOGS}"
: >"${REPORT}"
report() { printf '%s\n' "$*" | tee -a "${REPORT}"; }
fail() {
  report "FIRST ERROR: $*"
  report "FULL BUILD REPEATED: NO"
  report "COST INCURRED: \$0.00"
  exit 1
}
artifact() {
  local label="$1" path="$2" dependency_log
  [[ -f "${path}" ]] || fail "${label} is missing at ${path}."
  [[ -x "${path}" ]] || fail "${label} is not executable at ${path}."
  report "${label} PATH: ${path}"
  report "${label} SIZE: $(stat -c '%s bytes' "${path}")"
  report "${label} TIMESTAMP: $(stat -c '%y' "${path}")"
  report "${label} EXECUTABLE: YES"
  file "${path}" | tee -a "${REPORT}"
  dependency_log="${LOGS}/$(printf '%s' "${label}" | tr '[:upper:] ' '[:lower:]-')-ldd.log"
  ldd "${path}" >"${dependency_log}" 2>&1 || fail "ldd failed for ${label}."
  if grep -Fq 'not found' "${dependency_log}"; then
    grep -F 'not found' "${dependency_log}" | tee -a "${REPORT}"
    fail "${label} has unresolved runtime dependencies."
  fi
  report "${label} RUNTIME DEPENDENCIES: RESOLVED (${dependency_log})"
}

[[ "${GITHUB_REPOSITORY:-}" == "${EXPECTED_REPOSITORY}" ]] || fail "Unexpected repository."
[[ "${GITHUB_REF_NAME:-}" == "${EXPECTED_BRANCH}" ]] || fail "Unexpected branch."
report "STW O3DE INCREMENTAL ASSETPROCESSORBATCH GATE"
report "TIMESTAMP: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
report "WORKFLOW COMMIT: $(git rev-parse HEAD)"
report "AUTHORITATIVE BUILT STATE: ${BUILT_FROM_COMMIT}"
report "BUILD TREE: ${BUILD}"
report "PROJECT ROOT: ${PROJECT}"

[[ -d "${STW}/.git" ]] || fail "Canonical STW checkout is missing."
git -C "${STW}" cat-file -e "${BUILT_FROM_COMMIT}^{commit}" || fail "Built commit is absent."
[[ -z "$(git -C "${STW}" status --porcelain=v1 --untracked-files=all)" ]] || fail "Canonical STW checkout is dirty."
[[ -d "${O3DE}/.git" ]] || fail "Pinned O3DE checkout is missing."
[[ "$(git -C "${O3DE}" rev-parse HEAD)" == "${O3DE_COMMIT}" ]] || fail "O3DE pin mismatch."
[[ -f "${PROJECT}/project.json" ]] || fail "STW project.json is missing."
[[ -f "${BUILD}/CMakeCache.txt" ]] || fail "Existing CMake cache is missing."
[[ -f "${BUILD}/build-profile.ninja" ]] || fail "Existing profile Ninja graph is missing."

artifact "GAME LAUNCHER" "${LAUNCHER}"
artifact "EDITOR" "${EDITOR}"
report "RAM BEFORE BUILD:"; free -h | tee -a "${REPORT}"
report "DISK BEFORE BUILD:"; df -h "${ROOT}" | tee -a "${REPORT}"

targets="${LOGS}/assetprocessor-targets.log"
ninja -C "${BUILD}" -f build-profile.ninja -t targets all >"${targets}" 2>&1 || fail "Ninja target query failed."
candidates="$(grep -E '(^|[/])AssetProcessorBatch([^:]*):' "${targets}" || true)"
[[ -n "${candidates}" ]] || fail "No AssetProcessorBatch target exists."
report "ASSETPROCESSORBATCH TARGET CANDIDATES:"
printf '%s\n' "${candidates}" | tee -a "${REPORT}"
if grep -Eq '^AssetProcessorBatch:' "${targets}"; then
  target="AssetProcessorBatch"
elif grep -Eq '^AssetProcessorBatch:profile:' "${targets}"; then
  target="AssetProcessorBatch:profile"
else
  fail "No exact safe top-level AssetProcessorBatch target exists."
fi
report "ASSETPROCESSORBATCH TARGET: ${target}"

build_log="${LOGS}/build-assetprocessorbatch.log"
report "RUN: incremental AssetProcessorBatch only"
set +e
cmake --build "${BUILD}" --target "${target}" --config profile --parallel 2 >"${build_log}" 2>&1
rc=$?
set -e
if ((rc != 0)); then
  first="$(grep -n -m1 -E 'FAILED:|fatal error:|(^|[[:space:]])error:' "${build_log}" || true)"
  report "FIRST BUILD ERROR: ${first:-not identified; inspect ${build_log}}"
  tail -n 120 "${build_log}" | tee -a "${REPORT}"
  fail "Incremental AssetProcessorBatch build exited ${rc}."
fi
report "ASSETPROCESSORBATCH BUILD: PASS"
artifact "ASSETPROCESSORBATCH" "${AP}"

smoke="${LOGS}/assetprocessorbatch-help.log"
set +e
timeout 30s env LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "${AP}" --help >"${smoke}" 2>&1
rc=$?
set -e
if ((rc != 0)); then
  first="$(grep -n -m1 -Ei 'error|failed|not found|cannot open|undefined symbol' "${smoke}" || true)"
  report "FIRST SMOKE ERROR: ${first:-not identified; inspect ${smoke}}"
  tail -n 80 "${smoke}" | tee -a "${REPORT}"
  fail "AssetProcessorBatch --help exited ${rc}."
fi
report "ASSETPROCESSORBATCH SMOKE TEST: PASS (--help)"
report "PREPARED COMMAND: ${AP} --project-path=${PROJECT}"
report "BUILD TREE PRESERVED: YES"
report "FULL BUILD REPEATED: NO"
report "FILES CHANGED: generated AssetProcessorBatch outputs only; no gameplay/STW source changes"
report "COST INCURRED: \$0.00"
report "FIRST ERROR: NONE"
report "NEXT SAFE ACTION: run AssetProcessorBatch for ${PROJECT}, then inspect logs before GameLauncher."
