#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="${HOME}"
ICD="${ROOT}/stw-o3de-gate/nvidia_icd.json"

echo "VULKAN NATIVE SCREENSHOT LAYER READ-ONLY INVENTORY"
echo "NO BUILD / NO LAUNCH / NO ASSET PROCESSING / NO INSTALL / NO SOURCE CHANGE"
echo "VULKAN LAYER MANIFESTS:"
for dir in /usr/share/vulkan/explicit_layer.d /usr/share/vulkan/implicit_layer.d /usr/local/share/vulkan/explicit_layer.d /etc/vulkan/explicit_layer.d "${ROOT}/.local/share/vulkan/explicit_layer.d"; do
  echo "===== ${dir} ====="
  if [[ -d "${dir}" ]]; then
    find "${dir}" -maxdepth 1 -type f -printf '%p %s bytes\n' | sort
  else
    echo "MISSING"
  fi
done
echo "SCREENSHOT LAYER CANDIDATES:"
find /usr/share/vulkan /usr/local/share/vulkan /etc/vulkan "${ROOT}/.local/share/vulkan" \
  -maxdepth 4 -type f \( -iname '*screenshot*' -o -iname '*capture*' \) -print 2>/dev/null | sort || true
echo "SCREENSHOT/CAPTURE MANIFEST CONTENT:"
find /usr/share/vulkan /usr/local/share/vulkan /etc/vulkan "${ROOT}/.local/share/vulkan" \
  -maxdepth 4 -type f -name '*.json' -print0 2>/dev/null |
  xargs -0 grep -HnEi 'VK_LAYER.*screenshot|screenshot|capture' 2>/dev/null | sed -n '1,500p' || true
echo "SCREENSHOT LAYER LIBRARIES:"
find /usr/lib /usr/local/lib "${ROOT}/.local/lib" -maxdepth 5 -type f \
  \( -iname '*VkLayer*screenshot*' -o -iname '*VkLayer*capture*' \) -printf '%p %s bytes\n' 2>/dev/null | sort || true
echo "EXISTING CAPTURE TOOLS:"
for tool in vulkaninfo xwd import ffmpeg maim scrot grim gnome-screenshot; do
  printf '%s: ' "${tool}"
  command -v "${tool}" || echo MISSING
done
echo "NVIDIA VULKAN SUMMARY:"
VK_ICD_FILENAMES="${ICD}" vulkaninfo --summary 2>&1 | grep -E 'GPU|deviceName|driver|apiVersion|VK_LAYER' | sed -n '1,240p' || true
echo "COST INCURRED: \$0.00"
