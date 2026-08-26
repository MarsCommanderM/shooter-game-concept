#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly BIN="${ROOT}/stw-o3de-build/linux/bin/profile"
readonly REGISTRY="${BIN}/Registry"
readonly LAUNCHER="${BIN}/STW.GameLauncher"

echo "STW GAMELAUNCHER REGISTRY READ-ONLY INVENTORY"
[[ -x "${LAUNCHER}" ]] || { echo "LAUNCHER MISSING"; exit 1; }
echo "LAUNCHER=$(stat -c '%n | %s bytes | %y' "${LAUNCHER}")"
echo "REGISTRY DIRECTORY=${REGISTRY}"
[[ -d "${REGISTRY}" ]] || { echo "REGISTRY DIRECTORY MISSING"; exit 1; }
find "${REGISTRY}" -maxdepth 1 -type f -printf '%f | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' | sort
echo "LAUNCHER MATCHES:"
find "${BIN}" -maxdepth 3 -type f \( -iname '*launcher*.setreg' -o -iname '*game*.setreg' -o -iname '*stw*.setreg' \) -printf '%p | %s bytes\n' | sort
echo "DEPENDENCY REGISTRY REFERENCES:"
grep -RIlE 'STW\.GameLauncher|GameLauncher|STW' "${REGISTRY}" 2>/dev/null | sort
echo "NO BUILD"
echo "NO LAUNCH"
echo "COST INCURRED: \$0.00"
