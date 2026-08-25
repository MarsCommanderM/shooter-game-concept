#!/usr/bin/env bash

set -Eeuo pipefail

readonly EXPECTED_REPOSITORY="MarsCommanderM/shooter-game-concept"
readonly EXPECTED_BRANCH="brauny/stw-game-production"
readonly EXPECTED_HISTORY="8c624b682ada3df6a91c307ba4fd8dc6d46e624a"
readonly O3DE_VERSION="26.05.0"
readonly O3DE_TAG="2605.0"
readonly O3DE_COMMIT="3db6943249d8bd7960b9ed7e9aee310b7668586e"
readonly REQUIRED_FREE_GIB=180
readonly REQUIRED_MEMORY_KIB=$((16 * 1024 * 1024))
readonly PERSISTENT_ROOT="${HOME}"
readonly STATE_ROOT="${PERSISTENT_ROOT}/stw-o3de-gate"
readonly LOG_ROOT="${STATE_ROOT}/logs"
readonly REPORT_FILE="${STATE_ROOT}/host-qualification.txt"
readonly STW_ROOT="${PERSISTENT_ROOT}/stw-production"
readonly USER_BIN="${PERSISTENT_ROOT}/.local/bin"
readonly TOOLS_BIN="${STATE_ROOT}/bin"
readonly GIT_ASKPASS_HELPER="${STATE_ROOT}/git-askpass.sh"
readonly NVIDIA_ICD_FILE="${STATE_ROOT}/nvidia_icd.json"
readonly O3DE_ROOT="${PERSISTENT_ROOT}/o3de-2605"
readonly O3DE_PACKAGES="${PERSISTENT_ROOT}/o3de-packages"
readonly STW_GATE_ROOT="${PERSISTENT_ROOT}/stw-o3de-worktree"
readonly PROJECT_ROOT="${STW_GATE_ROOT}/stw-o3de/Project"
readonly STW_CORE_GEM="${STW_GATE_ROOT}/stw-o3de/Gems/STWCore"
readonly STW_GAMEPLAY_GEM="${STW_GATE_ROOT}/stw-o3de/Gems/STWGameplay"
readonly BUILD_ROOT="${PERSISTENT_ROOT}/stw-o3de-build/linux"

mkdir -p "${LOG_ROOT}"
: >"${REPORT_FILE}"

umask 077
cat >"${GIT_ASKPASS_HELPER}" <<'EOF'
#!/usr/bin/env bash
case "${1:-}" in
  *Username*) printf '%s\n' 'x-access-token' ;;
  *Password*) printf '%s\n' "${STW_GITHUB_TOKEN:?}" ;;
  *) exit 1 ;;
esac
EOF
chmod 700 "${GIT_ASKPASS_HELPER}"
trap 'rm -f "${GIT_ASKPASS_HELPER}"' EXIT

report() {
  printf '%s\n' "$*" | tee -a "${REPORT_FILE}"
}

fail() {
  report "BLOCKER: $*"
  report "COST INCURRED: \$0.00"
  exit 1
}

run_logged() {
  local name="$1"
  shift
  local log_file="${LOG_ROOT}/${name}.log"
  report "RUN: ${name}"
  if "$@" >"${log_file}" 2>&1; then
    report "PASS: ${name}"
    return 0
  fi
  report "FAIL: ${name}"
  tail -n 80 "${log_file}" || true
  return 1
}

git_remote() {
  if [[ -n "${STW_GITHUB_TOKEN:-}" ]]; then
    GIT_TERMINAL_PROMPT=0 GIT_ASKPASS_REQUIRE=force \
      GIT_ASKPASS="${GIT_ASKPASS_HELPER}" "$@"
    return
  fi
  GIT_TERMINAL_PROMPT=0 "$@"
}

command_version() {
  local command_name="$1"
  shift
  if command -v "${command_name}" >/dev/null 2>&1; then
    "$@" 2>&1 | head -n 1
  else
    printf 'NOT FOUND'
  fi
}

[[ "${GITHUB_REPOSITORY:-}" == "${EXPECTED_REPOSITORY}" ]] ||
  fail "Unexpected repository '${GITHUB_REPOSITORY:-unset}'."
[[ "${GITHUB_REF_NAME:-}" == "${EXPECTED_BRANCH}" ]] ||
  fail "Unexpected branch '${GITHUB_REF_NAME:-unset}'."

report "LIGHTNING O3DE HOST QUALIFICATION"
report "timestamp: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
report "repository: ${GITHUB_REPOSITORY}"
report "workflow commit: $(git rev-parse HEAD)"
report "host: $(hostname)"
report "user: $(id -un)"
report "kernel: $(uname -srmo)"
report "free/paid: billing metadata is not exposed to the shell; using only the already-running T4 and starting no resource"

if [[ -r /etc/os-release ]]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  report "os: ${PRETTY_NAME:-unknown}"
else
  report "os: unknown"
fi

report "cpu: $(lscpu 2>/dev/null | awk -F: '/Model name/ {sub(/^[ \t]+/, "", $2); print $2; exit}')"
report "cpu cores: $(getconf _NPROCESSORS_ONLN 2>/dev/null || printf unknown)"

memory_kib="$(awk '/^MemTotal:/ {print $2}' /proc/meminfo)"
memory_gib="$(awk -v kib="${memory_kib}" 'BEGIN {printf "%.2f", kib / 1024 / 1024}')"
report "ram: ${memory_gib} GiB"

report "persistent root: ${PERSISTENT_ROOT}"
report "persistent mount: $(findmnt -T "${PERSISTENT_ROOT}" -n -o TARGET,SOURCE,FSTYPE,OPTIONS 2>/dev/null || printf unknown)"
report "mount points:"
df -hT | tee -a "${REPORT_FILE}"

free_kib="$(df -Pk "${PERSISTENT_ROOT}" | awk 'NR == 2 {print $4}')"
free_gib="$(awk -v kib="${free_kib}" 'BEGIN {printf "%.2f", kib / 1024 / 1024}')"
report "persistent free disk before cleanup: ${free_gib} GiB"
report "storage estimate: 30 GiB source + 60 GiB dependencies + 70 GiB build/artifacts + 20 GiB safety = ${REQUIRED_FREE_GIB} GiB"

report "nvidia device nodes:"
if compgen -G '/dev/nvidia*' >/dev/null; then
  ls -l /dev/nvidia* | tee -a "${REPORT_FILE}"
else
  report "NONE"
fi

command -v nvidia-smi >/dev/null 2>&1 || fail "nvidia-smi is unavailable on the current machine."
gpu_line="$(nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader 2>/dev/null | head -n 1)"
report "nvidia-smi: ${gpu_line}"
printf '%s\n' "${gpu_line}" | grep -Eiq '(^|[[:space:]])(Tesla[[:space:]]+)?T4([,[:space:]]|$)' ||
  fail "The current physical GPU is not an NVIDIA T4."
[[ -e /dev/nvidia0 && -e /dev/nvidiactl ]] ||
  fail "Required NVIDIA device nodes /dev/nvidia0 and /dev/nvidiactl are missing."

report "cuda driver visibility: $(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -n 1)"
if command -v nvcc >/dev/null 2>&1; then
  report "cuda toolkit: $(nvcc --version 2>/dev/null | tail -n 1)"
else
  report "cuda toolkit: nvcc not installed (driver remains visible)"
fi

vulkan_loader="$(ldconfig -p 2>/dev/null | awk '/libvulkan\.so\.1/{print $NF; exit}')"
report "vulkan loader: ${vulkan_loader:-NOT FOUND}"
report "nvidia vulkan icd files:"
{ find /etc/vulkan /usr/share/vulkan -maxdepth 3 -type f \
  \( -iname '*nvidia*json' -o -iname 'nvidia_icd.json' \) -print 2>/dev/null || true; } |
  sort | tee -a "${REPORT_FILE}"

report "git: $(command_version git git --version)"
report "python: $(command_version python3 python3 --version)"
report "node: $(command_version node node --version)"
report "cmake before setup: $(command_version cmake cmake --version)"
report "clang before setup: $(command_version clang++ clang++ --version)"
report "ninja before setup: $(command_version ninja ninja --version)"
report "vulkaninfo before setup: $(command -v vulkaninfo 2>/dev/null || printf 'NOT FOUND')"

((memory_kib >= REQUIRED_MEMORY_KIB)) ||
  fail "O3DE requires at least 16 GiB RAM; only ${memory_gib} GiB is visible."

if ((free_kib < REQUIRED_FREE_GIB * 1024 * 1024)); then
  report "Storage is below the conservative gate; cleaning only safe package caches."
  if command -v python3 >/dev/null 2>&1; then
    python3 -m pip cache purge >"${LOG_ROOT}/pip-cache-purge.log" 2>&1 || true
  fi
  if command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
    sudo -n apt-get clean >"${LOG_ROOT}/apt-clean.log" 2>&1 || true
  fi
  free_kib="$(df -Pk "${PERSISTENT_ROOT}" | awk 'NR == 2 {print $4}')"
  free_gib="$(awk -v kib="${free_kib}" 'BEGIN {printf "%.2f", kib / 1024 / 1024}')"
  report "persistent free disk after safe cleanup: ${free_gib} GiB"
fi

if ((free_kib < REQUIRED_FREE_GIB * 1024 * 1024)); then
  missing_kib=$((REQUIRED_FREE_GIB * 1024 * 1024 - free_kib))
  missing_gib="$(awk -v kib="${missing_kib}" 'BEGIN {printf "%.2f", kib / 1024 / 1024}')"
  fail "Persistent storage is short by ${missing_gib} GiB; O3DE was not downloaded."
fi

if [[ -e "${STW_ROOT}" && ! -d "${STW_ROOT}/.git" ]]; then
  fail "${STW_ROOT} exists but is not the authorized Git repository."
fi

if [[ ! -d "${STW_ROOT}/.git" ]]; then
  run_logged clone-stw git_remote git clone \
    --branch "${EXPECTED_BRANCH}" \
    --single-branch \
    "https://github.com/${EXPECTED_REPOSITORY}.git" \
    "${STW_ROOT}" || fail "Unable to clone the authoritative STW repository."
fi

origin_url="$(git -C "${STW_ROOT}" remote get-url origin)"
case "${origin_url}" in
  "https://github.com/${EXPECTED_REPOSITORY}" | \
    "https://github.com/${EXPECTED_REPOSITORY}.git" | \
    "git@github.com:${EXPECTED_REPOSITORY}.git") ;;
  *) fail "Persistent checkout origin is not the authorized STW repository." ;;
esac

[[ -z "$(git -C "${STW_ROOT}" status --porcelain=v1 --untracked-files=all)" ]] ||
  fail "Persistent STW checkout contains uncommitted work; nothing was overwritten."

run_logged fetch-stw git_remote git -C "${STW_ROOT}" fetch origin "${EXPECTED_BRANCH}" ||
  fail "Unable to fetch the production branch."
git -C "${STW_ROOT}" switch "${EXPECTED_BRANCH}" >"${LOG_ROOT}/switch-stw.log" 2>&1 ||
  fail "Unable to select the production branch without overwriting work."
git -C "${STW_ROOT}" merge --ff-only "origin/${EXPECTED_BRANCH}" >"${LOG_ROOT}/ff-stw.log" 2>&1 ||
  fail "Persistent checkout cannot fast-forward cleanly to origin."

remote_head="$(git -C "${STW_ROOT}" rev-parse "origin/${EXPECTED_BRANCH}")"
local_head="$(git -C "${STW_ROOT}" rev-parse HEAD)"
[[ "${local_head}" == "${remote_head}" ]] || fail "Persistent checkout does not match the remote head."
git -C "${STW_ROOT}" merge-base --is-ancestor "${EXPECTED_HISTORY}" "${remote_head}" ||
  fail "Expected production history ${EXPECTED_HISTORY} is not an ancestor of ${remote_head}."
[[ -f "${STW_ROOT}/stw-o3de/MIGRATION.md" ]] || fail "MIGRATION.md is missing."
[[ -x "${STW_ROOT}/stw-o3de/Scripts/verify-host.sh" ]] || fail "verify-host.sh is missing or not executable."

report "stw repository: ${STW_ROOT}"
report "production branch: ${EXPECTED_BRANCH}"
report "remote head: ${remote_head}"
report "expected history: PASS (${EXPECTED_HISTORY})"
report "migration.md: ${STW_ROOT}/stw-o3de/MIGRATION.md"
report "verify-host script: ${STW_ROOT}/stw-o3de/Scripts/verify-host.sh"
report "o3de pin: ${O3DE_VERSION} tag ${O3DE_TAG} commit ${O3DE_COMMIT}"

set +e
(cd "${STW_ROOT}" && bash stw-o3de/Scripts/verify-host.sh) \
  >"${LOG_ROOT}/verify-host-before.log" 2>&1
verify_before_rc=$?
set -e
report "verify-host unchanged before dependency setup: exit ${verify_before_rc}"
tail -n 60 "${LOG_ROOT}/verify-host-before.log" | tee -a "${REPORT_FILE}"

command -v sudo >/dev/null 2>&1 || fail "sudo is unavailable; required free system packages cannot be installed."
sudo -n true >/dev/null 2>&1 || fail "Passwordless sudo is unavailable; required free system packages cannot be installed automatically."

run_logged apt-update sudo -n apt-get update || fail "apt package metadata update failed."
run_logged apt-install sudo -n env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  build-essential \
  clang \
  lld \
  libstdc++-12-dev \
  ninja-build \
  git-lfs \
  python3-pip \
  libvulkan-dev \
  vulkan-tools \
  libx11-dev \
  libx11-xcb-dev \
  libxcb1-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libxrandr-dev \
  libxi-dev \
  libxcursor-dev \
  libxinerama-dev \
  libglu1-mesa-dev \
  mesa-common-dev \
  libxcb-randr0-dev \
  libxcb-xinerama0 \
  libxcb-xinput0 \
  libxcb-xinput-dev \
  libxcb-xfixes0-dev \
  libxcb-xkb-dev \
  libxcb-image0-dev \
  libxcb-keysyms1-dev \
  libfontconfig1-dev \
  libfreetype6-dev \
  libcurl4-openssl-dev \
  libpcre2-16-0 \
  libudev-dev \
  libdbus-1-dev \
  libpulse-dev \
  libasound2-dev \
  libssl-dev \
  libunwind-dev \
  libzstd-dev \
  zlib1g-dev \
  xxd \
  xauth \
  xvfb || fail "Required O3DE host packages failed to install."

export PATH="${TOOLS_BIN}:${USER_BIN}:${PATH}"
cmake_ready=false
if command -v cmake >/dev/null 2>&1; then
  cmake_version="$(cmake --version | awk 'NR == 1 {print $3}')"
  if [[ "$(printf '%s\n%s\n' '3.30.0' "${cmake_version}" | sort -V | head -n 1)" == '3.30.0' ]]; then
    cmake_ready=true
  fi
fi
if [[ "${cmake_ready}" != true ]]; then
  run_logged install-cmake python3 -m pip install --user --disable-pip-version-check \
    --upgrade "cmake==3.30.5" ||
    fail "Unable to install pinned CMake into Lightning's existing default environment."
fi

cmake_native_dir="$(python3 -c 'import cmake, pathlib; print(pathlib.Path(cmake.__file__).parent / "data" / "bin")')"
for cmake_tool in cmake ctest cpack; do
  [[ -x "${cmake_native_dir}/${cmake_tool}" ]] ||
    fail "Pinned CMake package is missing native ${cmake_tool}."
  if [[ -e "${TOOLS_BIN}/${cmake_tool}" && ! -L "${TOOLS_BIN}/${cmake_tool}" ]]; then
    fail "Refusing to replace non-symlink tool ${TOOLS_BIN}/${cmake_tool}."
  fi
  mkdir -p "${TOOLS_BIN}"
  ln -sfn "${cmake_native_dir}/${cmake_tool}" "${TOOLS_BIN}/${cmake_tool}"
done
export PATH="${TOOLS_BIN}:${USER_BIN}:${PATH}"

git lfs install --skip-repo >"${LOG_ROOT}/git-lfs-install.log" 2>&1 ||
  fail "Git LFS setup failed."

nvidia_vulkan_library="$(ldconfig -p 2>/dev/null | awk '$1 == "libGLX_nvidia.so.0" {print $NF; exit}')"
[[ -n "${nvidia_vulkan_library}" && -r "${nvidia_vulkan_library}" ]] ||
  fail "The Lightning container does not expose the NVIDIA Vulkan ICD library."
cat >"${NVIDIA_ICD_FILE}" <<'EOF'
{
  "file_format_version": "1.0.0",
  "ICD": {
    "library_path": "libGLX_nvidia.so.0",
    "api_version": "1.3.280"
  }
}
EOF
chmod 600 "${NVIDIA_ICD_FILE}"
export VK_DRIVER_FILES="${NVIDIA_ICD_FILE}"
report "Lightning NVIDIA ICD manifest: ${NVIDIA_ICD_FILE} -> ${nvidia_vulkan_library}"

report "cmake after setup: $(cmake --version | head -n 1)"
report "clang after setup: $(clang++ --version | head -n 1)"
report "ninja after setup: $(ninja --version)"
report "git lfs after setup: $(git lfs version)"

set +e
vulkaninfo --summary >"${LOG_ROOT}/vulkaninfo-summary.log" 2>&1
vulkan_rc=$?
set -e
report "vulkaninfo exit: ${vulkan_rc}"
tail -n 100 "${LOG_ROOT}/vulkaninfo-summary.log" | tee -a "${REPORT_FILE}"
if ((vulkan_rc != 0)); then
  report "vulkan diagnostics:"
  for variable_name in \
    VK_DRIVER_FILES \
    VK_ICD_FILENAMES \
    NVIDIA_VISIBLE_DEVICES \
    NVIDIA_DRIVER_CAPABILITIES \
    LD_LIBRARY_PATH \
    DISPLAY; do
    report "${variable_name}=${!variable_name:-unset}"
  done
  report "vulkan icd directories:"
  for icd_directory in /etc/vulkan/icd.d /usr/share/vulkan/icd.d "${HOME}/.local/share/vulkan/icd.d"; do
    if [[ -d "${icd_directory}" ]]; then
      find "${icd_directory}" -maxdepth 1 -type f -printf '%p\n' 2>/dev/null | sort | tee -a "${REPORT_FILE}"
    else
      report "MISSING: ${icd_directory}"
    fi
  done
  report "NVIDIA/Vulkan dynamic libraries:"
  { ldconfig -p 2>/dev/null | grep -Ei 'nvidia|vulkan' || true; } | tee -a "${REPORT_FILE}"
  report "NVIDIA package candidates:"
  apt-cache policy libnvidia-gl-580 nvidia-driver-580 nvidia-utils-580 2>/dev/null | tee -a "${REPORT_FILE}"
  report "installed NVIDIA/Vulkan packages:"
  { dpkg-query -W -f='${binary:Package} ${Version}\n' 2>/dev/null | grep -Ei 'nvidia|vulkan' || true; } |
    sort | tee -a "${REPORT_FILE}"
  report "NVIDIA graphics library files:"
  { find /usr /lib /system -xdev -type f \
    \( -name 'libGLX_nvidia.so*' -o -name 'libnvidia-glvkspirv.so*' -o -name 'libnvidia-vulkan-producer.so*' \) \
    -print 2>/dev/null || true; } | sort | tee -a "${REPORT_FILE}"
  fail "vulkaninfo could not enumerate a physical Vulkan device. O3DE was not downloaded."
fi
grep -Eiq 'deviceName[[:space:]]*=[[:space:]]*(Tesla[[:space:]]+)?T4|deviceName.*NVIDIA.*T4' \
  "${LOG_ROOT}/vulkaninfo-summary.log" ||
  fail "Vulkan did not identify the physical NVIDIA T4."
grep -Eiq 'llvmpipe|lavapipe|deviceType[[:space:]]*=[[:space:]]*PHYSICAL_DEVICE_TYPE_CPU' \
  "${LOG_ROOT}/vulkaninfo-summary.log" &&
  fail "Software Vulkan was detected; the visual gate refuses fallback rendering."

set +e
(cd "${STW_ROOT}" && PATH="${TOOLS_BIN}:${USER_BIN}:${PATH}" bash stw-o3de/Scripts/verify-host.sh) \
  >"${LOG_ROOT}/verify-host-after.log" 2>&1
verify_after_rc=$?
set -e
report "verify-host unchanged after dependency setup: exit ${verify_after_rc}"
tail -n 80 "${LOG_ROOT}/verify-host-after.log" | tee -a "${REPORT_FILE}"

if ((verify_after_rc != 0)); then
  fail "The unchanged repository host gate still fails. O3DE was not downloaded."
fi

report "HOST QUALIFICATION: PASS"

free_kib="$(df -Pk "${PERSISTENT_ROOT}" | awk 'NR == 2 {print $4}')"
free_gib="$(awk -v kib="${free_kib}" 'BEGIN {printf "%.2f", kib / 1024 / 1024}')"
report "persistent free disk before O3DE clone: ${free_gib} GiB"
((free_kib >= REQUIRED_FREE_GIB * 1024 * 1024)) ||
  fail "Storage dropped below the 180 GiB O3DE download gate."

if [[ -e "${O3DE_ROOT}" && ! -d "${O3DE_ROOT}/.git" ]]; then
  fail "${O3DE_ROOT} exists but is not an official O3DE Git checkout."
fi
if [[ ! -d "${O3DE_ROOT}/.git" ]]; then
  run_logged clone-o3de git clone \
    --branch "${O3DE_TAG}" \
    --single-branch \
    --depth 1 \
    https://github.com/o3de/o3de.git \
    "${O3DE_ROOT}" || fail "Official O3DE clone failed."
fi

o3de_origin="$(git -C "${O3DE_ROOT}" remote get-url origin)"
case "${o3de_origin}" in
  https://github.com/o3de/o3de | https://github.com/o3de/o3de.git | git@github.com:o3de/o3de.git) ;;
  *) fail "O3DE checkout origin is not the official upstream repository." ;;
esac
[[ -z "$(git -C "${O3DE_ROOT}" status --porcelain=v1 --untracked-files=all)" ]] ||
  fail "Official O3DE source checkout contains uncommitted changes."
actual_o3de_commit="$(git -C "${O3DE_ROOT}" rev-parse HEAD)"
[[ "${actual_o3de_commit}" == "${O3DE_COMMIT}" ]] ||
  fail "O3DE tag resolved to ${actual_o3de_commit}, not pinned ${O3DE_COMMIT}."
git -C "${O3DE_ROOT}" cat-file -e "${O3DE_COMMIT}^{commit}" ||
  fail "Pinned O3DE object is unavailable after clone."
report "O3DE source: ${O3DE_ROOT}"
report "O3DE version: ${O3DE_VERSION}"
report "O3DE tag: ${O3DE_TAG}"
report "O3DE commit: ${actual_o3de_commit}"

run_logged o3de-get-python env PATH="${TOOLS_BIN}:${USER_BIN}:${PATH}" \
  bash "${O3DE_ROOT}/python/get_python.sh" ||
  fail "Official O3DE Python bootstrap failed."
run_logged o3de-register-engine env PATH="${TOOLS_BIN}:${USER_BIN}:${PATH}" \
  "${O3DE_ROOT}/scripts/o3de.sh" register --this-engine ||
  fail "Official O3DE engine registration failed."

if [[ -e "${STW_GATE_ROOT}" && ! -e "${STW_GATE_ROOT}/.git" ]]; then
  fail "${STW_GATE_ROOT} exists but is not the isolated STW Git worktree."
fi
if [[ ! -e "${STW_GATE_ROOT}/.git" ]]; then
  run_logged create-stw-gate-worktree git -C "${STW_ROOT}" worktree add \
    --detach "${STW_GATE_ROOT}" "${remote_head}" ||
    fail "Unable to create the isolated STW O3DE worktree."
fi
[[ "$(git -C "${STW_GATE_ROOT}" rev-parse --show-toplevel)" == "${STW_GATE_ROOT}" ]] ||
  fail "The isolated STW gate worktree identity is invalid."

mkdir -p "${STW_GATE_ROOT}/stw-o3de/Gems"
if [[ ! -f "${PROJECT_ROOT}/project.json" ]]; then
  run_logged create-stw-project "${O3DE_ROOT}/scripts/o3de.sh" create-project \
    --project-path "${PROJECT_ROOT}" \
    --project-name STW || fail "Official O3DE STW project generation failed."
fi
if [[ ! -f "${STW_CORE_GEM}/gem.json" ]]; then
  run_logged create-stw-core-gem "${O3DE_ROOT}/scripts/o3de.sh" create-gem \
    --gem-path "${STW_CORE_GEM}" \
    --gem-name STWCore || fail "Official STWCore Gem generation failed."
fi
if [[ ! -f "${STW_GAMEPLAY_GEM}/gem.json" ]]; then
  run_logged create-stw-gameplay-gem "${O3DE_ROOT}/scripts/o3de.sh" create-gem \
    --gem-path "${STW_GAMEPLAY_GEM}" \
    --gem-name STWGameplay || fail "Official STWGameplay Gem generation failed."
fi

run_logged register-stw-core "${O3DE_ROOT}/scripts/o3de.sh" register \
  --external-subdirectory "${STW_CORE_GEM}" \
  --external-subdirectory-project-path "${PROJECT_ROOT}" ||
  fail "STWCore project registration failed."
run_logged register-stw-gameplay "${O3DE_ROOT}/scripts/o3de.sh" register \
  --external-subdirectory "${STW_GAMEPLAY_GEM}" \
  --external-subdirectory-project-path "${PROJECT_ROOT}" ||
  fail "STWGameplay project registration failed."
run_logged enable-stw-core "${O3DE_ROOT}/scripts/o3de.sh" enable-gem \
  --gem-path "${STW_CORE_GEM}" \
  --project-path "${PROJECT_ROOT}" || fail "STWCore enablement failed."
run_logged enable-stw-gameplay "${O3DE_ROOT}/scripts/o3de.sh" enable-gem \
  --gem-path "${STW_GAMEPLAY_GEM}" \
  --project-path "${PROJECT_ROOT}" || fail "STWGameplay enablement failed."

mkdir -p "${O3DE_PACKAGES}" "${BUILD_ROOT}"
run_logged configure-stw env \
  PATH="${TOOLS_BIN}:${USER_BIN}:${PATH}" \
  CC=clang \
  CXX=clang++ \
  VK_DRIVER_FILES="${NVIDIA_ICD_FILE}" \
  cmake -S "${PROJECT_ROOT}" -B "${BUILD_ROOT}" \
    -G "Ninja Multi-Config" \
    -DLY_3RDPARTY_PATH="${O3DE_PACKAGES}" ||
  fail "STW O3DE CMake configure failed."

free_after_kib="$(df -Pk "${PERSISTENT_ROOT}" | awk 'NR == 2 {print $4}')"
free_after_gib="$(awk -v kib="${free_after_kib}" 'BEGIN {printf "%.2f", kib / 1024 / 1024}')"
report "persistent free disk after configure: ${free_after_gib} GiB"
report "O3DE BOOTSTRAP: PASS"
report "STW PROJECT: PASS (${PROJECT_ROOT})"
report "STW GEMS: PASS (STWCore, STWGameplay)"
report "CONFIGURE: PASS (${BUILD_ROOT})"
report "BUILD: NOT STARTED IN SETUP PHASE"
report "CANONICAL STW CHECKOUT MODIFIED: NO"
report "COST INCURRED: \$0.00"
report "NEXT PHASE: build STW.GameLauncher and required Atom tools"
