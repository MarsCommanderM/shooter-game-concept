#!/usr/bin/env bash
set -Eeuo pipefail

echo "LIGHTNING VULKAN ICD READ-ONLY INVENTORY"
echo "VULKANINFO=$(command -v vulkaninfo || true)"
echo "LOADER LIBRARIES:"
ldconfig -p 2>/dev/null | grep -Ei 'vulkan|nvidia' | sed -n '1,160p' || true
echo "ICD PATHS:"
find -L /usr/share/vulkan /etc/vulkan /usr/local/share/vulkan "${HOME}" -maxdepth 6 \( -type f -o -type l \) \( -iname '*icd*.json' -o -iname '*nvidia*.json' \) -printf '%p | %y | %s bytes\n' 2>/dev/null | sort -u
echo "VULKAN ENV PRESENCE:"
env | cut -d= -f1 | grep -E '^VK_|^VULKAN' | sort || true
echo "VULKAN SUMMARY:"
timeout 30s vulkaninfo --summary 2>&1 | sed -n '1,240p'
echo "NO BUILD"
echo "NO LAUNCH"
echo "COST INCURRED: \$0.00"
