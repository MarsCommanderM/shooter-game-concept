#!/usr/bin/env bash

set -Eeuo pipefail

readonly STW_REPOSITORY="MarsCommanderM/shooter-game-concept"
readonly RUNNER_VERSION="2.336.0"
readonly RUNNER_SHA256="04cf0be1aff4c3ec3554466c39124ca250e3effd8873bb7e8d68535aa9505d5d"
readonly RUNNER_ARCHIVE="actions-runner-linux-x64-${RUNNER_VERSION}.tar.gz"
readonly RUNNER_URL="https://github.com/actions/runner/releases/download/v${RUNNER_VERSION}/${RUNNER_ARCHIVE}"
readonly RUNNER_NAME="lightning-t4"
readonly RUNNER_LABEL="stw-lightning-t4"
readonly RUNNER_ROOT="${STW_RUNNER_HOME:-${HOME}/.stw-github-runner}"
readonly PID_FILE="${RUNNER_ROOT}/runner.pid"
readonly LOG_FILE="${RUNNER_ROOT}/runner.log"
readonly VERSION_FILE="${RUNNER_ROOT}/.stw-runner-version"
readonly LOCK_DIR="${RUNNER_ROOT}.lock"

command_name="${1:-start}"

say() {
  printf '%s\n' "$*"
}

fail() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || fail "Required command is missing: $1"
}

read_pid() {
  [[ -f "${PID_FILE}" ]] || return 1

  local pid
  IFS= read -r pid <"${PID_FILE}"
  [[ "${pid}" =~ ^[1-9][0-9]*$ ]] || return 1
  printf '%s\n' "${pid}"
}

is_owned_runner_process() {
  local pid="$1"
  [[ -d "/proc/${pid}" ]] || return 1

  local process_cwd process_args
  process_cwd="$(readlink -f "/proc/${pid}/cwd" 2>/dev/null || true)"
  process_args="$(tr '\0' ' ' <"/proc/${pid}/cmdline" 2>/dev/null || true)"

  [[ "${process_cwd}" == "$(readlink -f "${RUNNER_ROOT}")" ]] || return 1
  [[ "${process_args}" == *"Runner.Listener"* || "${process_args}" == *"run.sh"* ]]
}

runner_pid() {
  local pid
  pid="$(read_pid 2>/dev/null || true)"
  [[ -n "${pid}" ]] || return 1
  is_owned_runner_process "${pid}" || return 1
  printf '%s\n' "${pid}"
}

show_status() {
  local pid
  if pid="$(runner_pid)"; then
    say "STW Lightning runner: ONLINE (PID ${pid})"
    say "Log: ${LOG_FILE}"
    return 0
  fi

  say "STW Lightning runner: OFFLINE"
  if [[ -f "${LOG_FILE}" ]]; then
    say "Last log lines:"
    tail -n 8 "${LOG_FILE}" || true
  fi
  return 1
}

stop_runner() {
  local pid
  pid="$(runner_pid)" || fail "No owned STW Lightning runner process is active."

  say "Stopping owned STW Lightning runner (PID ${pid})..."
  kill -TERM "${pid}"

  local attempt
  for ((attempt = 1; attempt <= 20; attempt++)); do
    if ! kill -0 "${pid}" 2>/dev/null; then
      rm -f "${PID_FILE}"
      say "STW Lightning runner stopped."
      return 0
    fi
    sleep 1
  done

  if is_owned_runner_process "${pid}"; then
    say "Runner did not stop after 20 seconds; terminating the same verified process."
    kill -KILL "${pid}"
    rm -f "${PID_FILE}"
    say "STW Lightning runner stopped."
    return 0
  fi

  fail "PID ${pid} changed ownership while stopping; no further signal was sent."
}

verify_repository() {
  require_command gh
  require_command git

  git rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
    fail "Run this script from the cloned shooter-game-concept repository."

  local origin_url
  origin_url="$(git remote get-url origin 2>/dev/null || true)"
  case "${origin_url}" in
    "https://github.com/${STW_REPOSITORY}" | \
      "https://github.com/${STW_REPOSITORY}.git" | \
      "git@github.com:${STW_REPOSITORY}.git") ;;
    *) fail "The origin remote is not the authorized STW repository." ;;
  esac

  local current_commit production_commit
  current_commit="$(git rev-parse HEAD)"
  production_commit="$(git rev-parse refs/remotes/origin/brauny/stw-game-production 2>/dev/null || true)"
  [[ -n "${production_commit}" && "${current_commit}" == "${production_commit}" ]] ||
    fail "Checkout is not the fetched production head. Run: git fetch, then git checkout origin/brauny/stw-game-production"
}

install_runner() {
  require_command curl
  require_command sha256sum
  require_command tar

  case "$(uname -m)" in
    x86_64 | amd64) ;;
    *) fail "This pinned bootstrap supports Linux x86_64 only." ;;
  esac

  if [[ -f "${VERSION_FILE}" ]]; then
    local installed_version
    IFS= read -r installed_version <"${VERSION_FILE}"
    [[ "${installed_version}" == "${RUNNER_VERSION}" ]] ||
      fail "Runner ${installed_version} is installed at ${RUNNER_ROOT}; remove it explicitly before changing versions."
  fi

  if [[ -x "${RUNNER_ROOT}/config.sh" && -x "${RUNNER_ROOT}/run.sh" ]]; then
    return 0
  fi

  [[ ! -e "${RUNNER_ROOT}" ]] ||
    fail "Partial runner directory exists at ${RUNNER_ROOT}; inspect it before retrying."

  local archive temporary_directory
  temporary_directory="$(mktemp -d)"
  archive="${temporary_directory}/${RUNNER_ARCHIVE}"

  say "Downloading pinned GitHub Actions runner v${RUNNER_VERSION}..."
  curl --fail --location --proto '=https' --tlsv1.2 --output "${archive}" "${RUNNER_URL}"
  printf '%s  %s\n' "${RUNNER_SHA256}" "${archive}" | sha256sum --check --status ||
    fail "GitHub Actions runner checksum verification failed."

  mkdir -p "${RUNNER_ROOT}"
  tar -xzf "${archive}" -C "${RUNNER_ROOT}"
  printf '%s\n' "${RUNNER_VERSION}" >"${VERSION_FILE}"
  rm -f -- "${archive}"
  rmdir -- "${temporary_directory}"
  say "Verified runner installed at ${RUNNER_ROOT}."
}

configure_runner() {
  [[ -f "${RUNNER_ROOT}/.runner" ]] && return 0

  local registration_token=""
  registration_token="$(
    gh api --method POST \
      "repos/${STW_REPOSITORY}/actions/runners/registration-token" \
      --jq '.token'
  )" || fail "GitHub did not issue a runner registration token."
  [[ -n "${registration_token}" ]] || fail "GitHub returned an empty runner registration token."

  say "Registering '${RUNNER_NAME}' for ${STW_REPOSITORY}..."
  (
    cd "${RUNNER_ROOT}"
    ./config.sh \
      --unattended \
      --replace \
      --disableupdate \
      --url "https://github.com/${STW_REPOSITORY}" \
      --token "${registration_token}" \
      --name "${RUNNER_NAME}" \
      --labels "${RUNNER_LABEL}" \
      --work "_work"
  )
  registration_token=""
}

start_runner() {
  local current_pid
  if current_pid="$(runner_pid)"; then
    say "STW Lightning runner is already online (PID ${current_pid})."
    return 0
  fi

  rm -f "${PID_FILE}"
  : >"${LOG_FILE}"

  say "Starting STW Lightning runner..."
  (
    cd "${RUNNER_ROOT}"
    nohup ./run.sh </dev/null >>"${LOG_FILE}" 2>&1 &
    printf '%s\n' "$!" >"${PID_FILE}"
  )

  local attempt pid
  for ((attempt = 1; attempt <= 30; attempt++)); do
    pid="$(runner_pid 2>/dev/null || true)"
    if [[ -n "${pid}" ]] && grep -q "Listening for Jobs" "${LOG_FILE}" 2>/dev/null; then
      say "STW Lightning runner: ONLINE (PID ${pid})"
      say "GitHub can now dispatch the fixed STW workflow to this Studio."
      say "Log: ${LOG_FILE}"
      return 0
    fi
    sleep 1
  done

  tail -n 30 "${LOG_FILE}" >&2 || true
  fail "Runner did not become ready within 30 seconds."
}

remove_runner() {
  if runner_pid >/dev/null 2>&1; then
    stop_runner
  fi

  if [[ -f "${RUNNER_ROOT}/.runner" ]]; then
    verify_repository
    local removal_token=""
    removal_token="$(
      gh api --method POST \
        "repos/${STW_REPOSITORY}/actions/runners/remove-token" \
        --jq '.token'
    )" || fail "GitHub did not issue a runner removal token."

    (
      cd "${RUNNER_ROOT}"
      ./config.sh remove --unattended --token "${removal_token}"
    )
    removal_token=""
  fi

  say "Runner registration removed. Files remain at ${RUNNER_ROOT}."
}

if [[ "${EUID}" -eq 0 && "${command_name}" == "start" ]]; then
  fail "Do not run the GitHub Actions runner as root."
fi

case "${command_name}" in
  start | stop | remove)
    mkdir "${LOCK_DIR}" 2>/dev/null || fail "Another runner operation is active: ${LOCK_DIR}"
    trap 'rmdir "${LOCK_DIR}" 2>/dev/null || true' EXIT
    ;;
  status) ;;
  *) fail "Usage: bash runner.sh [start|status|stop|remove]" ;;
esac

case "${command_name}" in
  start)
    verify_repository
    install_runner
    configure_runner
    start_runner
    ;;
  status)
    show_status
    ;;
  stop)
    stop_runner
    ;;
  remove)
    remove_runner
    ;;
esac
