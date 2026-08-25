#!/usr/bin/env bash

set -Eeuo pipefail

readonly EXPECTED_REPOSITORY="MarsCommanderM/shooter-game-concept"
readonly EXPECTED_BRANCH="brauny/stw-game-production"

[[ "${GITHUB_REPOSITORY:-}" == "${EXPECTED_REPOSITORY}" ]] || {
  printf 'Refusing unexpected repository: %s\n' "${GITHUB_REPOSITORY:-unset}" >&2
  exit 1
}

[[ "${GITHUB_REF_NAME:-}" == "${EXPECTED_BRANCH}" ]] || {
  printf 'Refusing unexpected branch: %s\n' "${GITHUB_REF_NAME:-unset}" >&2
  exit 1
}

[[ -f stw-engine/CMakeLists.txt ]] || {
  printf 'Missing canonical STW source tree.\n' >&2
  exit 1
}

printf 'STW Lightning T4 handshake\n'
printf 'repository: %s\n' "${GITHUB_REPOSITORY}"
printf 'branch: %s\n' "${GITHUB_REF_NAME}"
printf 'commit: %s\n' "$(git rev-parse HEAD)"
printf 'host: %s\n' "$(hostname)"
printf 'kernel: %s\n' "$(uname -srmo)"
printf 'cpu cores: %s\n' "$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf 'unknown')"

if command -v nvidia-smi >/dev/null 2>&1; then
  nvidia-smi \
    --query-gpu=name,driver_version,memory.total \
    --format=csv,noheader
else
  printf 'gpu: nvidia-smi unavailable\n'
fi

printf 'STW Lightning T4 handshake complete.\n'
