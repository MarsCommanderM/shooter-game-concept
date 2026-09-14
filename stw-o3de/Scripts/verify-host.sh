#!/usr/bin/env bash

# Read-only preflight for the STW O3DE source-build and visual gate.
# It installs no packages and creates no files.

set -uo pipefail

failures=0

pass() {
  printf 'PASS: %s\n' "$1"
}

fail() {
  printf 'FAIL: %s\n' "$1" >&2
  failures=$((failures + 1))
}

version_at_least() {
  local actual="$1"
  local minimum="$2"
  [ "$(printf '%s\n%s\n' "$minimum" "$actual" | sort -V | head -n 1)" = "$minimum" ]
}

printf 'STW O3DE 26.05 HOST GATE\n'
printf 'host: %s\n' "$(hostname 2>/dev/null || printf unknown)"
printf 'kernel: %s\n' "$(uname -srmo 2>/dev/null || printf unknown)"

architecture="$(uname -m 2>/dev/null || true)"
if [ "$architecture" = "x86_64" ]; then
  pass "x86-64 host architecture"
else
  fail "x86-64 required; detected ${architecture:-unknown}"
fi

if command -v lscpu >/dev/null 2>&1 && lscpu | grep -q '\bsse4_1\b'; then
  pass "SSE 4.1 CPU capability"
else
  fail "SSE 4.1 CPU capability not detected"
fi

memory_kib="$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo 2>/dev/null || true)"
if [ -n "$memory_kib" ] && [ "$memory_kib" -ge 16777216 ]; then
  pass "at least 16 GiB RAM ($((memory_kib / 1024 / 1024)) GiB detected)"
else
  detected_memory="unknown"
  if [ -n "$memory_kib" ]; then
    detected_memory="$((memory_kib / 1024 / 1024)) GiB"
  fi
  fail "16 GiB RAM minimum; detected $detected_memory"
fi

available_kib="$(df -Pk . 2>/dev/null | awk 'NR == 2 { print $4 }')"
required_disk_kib=$((100 * 1024 * 1024))
if [ -n "$available_kib" ] && [ "$available_kib" -ge "$required_disk_kib" ]; then
  pass "at least 100 GiB free disk ($((available_kib / 1024 / 1024)) GiB detected)"
else
  detected_disk="unknown"
  if [ -n "$available_kib" ]; then
    detected_disk="$((available_kib / 1024 / 1024)) GiB"
  fi
  fail "100+ GiB free disk required for source build; detected $detected_disk"
fi

if command -v cmake >/dev/null 2>&1; then
  cmake_version="$(cmake --version | awk 'NR == 1 { print $3 }')"
  if version_at_least "$cmake_version" "3.30.0"; then
    pass "CMake $cmake_version"
  else
    fail "CMake 3.30.0 or later required; detected $cmake_version"
  fi
else
  fail "CMake 3.30.0 or later not found"
fi

if command -v clang++ >/dev/null 2>&1; then
  clang_version="$(clang++ --version | awk 'NR == 1 { for (i = 1; i <= NF; ++i) if ($i ~ /^[0-9]+([.][0-9]+)+/) { print $i; exit } }')"
  clang_major="${clang_version%%.*}"
  if [ -n "$clang_major" ] && [ "$clang_major" -ge 14 ]; then
    pass "Clang $clang_version"
  else
    fail "Clang 14 or later required; detected ${clang_version:-unknown}"
  fi
else
  fail "clang++ 14 or later not found"
fi

if command -v ninja >/dev/null 2>&1; then
  pass "Ninja $(ninja --version 2>/dev/null || printf unknown)"
else
  fail "Ninja not found"
fi

if command -v vulkaninfo >/dev/null 2>&1; then
  vulkan_summary="$(vulkaninfo --summary 2>&1)"
  if printf '%s\n' "$vulkan_summary" | grep -Eiq 'llvmpipe|lavapipe|deviceType[^[:alnum:]]*(CPU|VK_PHYSICAL_DEVICE_TYPE_CPU)'; then
    fail "only a software Vulkan device was detected; a real GPU is required"
  elif printf '%s\n' "$vulkan_summary" | grep -Eiq 'GPU|deviceName|deviceType'; then
    pass "Vulkan device enumerates without a software-device marker"
  else
    fail "vulkaninfo ran but no Vulkan device was identified"
  fi
else
  fail "vulkaninfo not found"
fi

if [ -d /dev/dri ] && find /dev/dri -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null | grep -q .; then
  pass "DRM device node present"
else
  fail "no DRM GPU device node detected under /dev/dri"
fi

if [ "$failures" -eq 0 ]; then
  printf 'RESULT: READY FOR O3DE PROJECT/BUILD/VISUAL GATE\n'
  exit 0
fi

printf 'RESULT: BLOCKED (%d failed checks)\n' "$failures" >&2
exit 1
