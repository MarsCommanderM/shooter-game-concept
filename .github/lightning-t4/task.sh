#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="${HOME}"
O3DE="${ROOT}/o3de-2605"
BUILD="${ROOT}/stw-o3de-build/linux"
PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
BIN="${BUILD}/bin/profile"

echo "O3DE 26.05 SCRIPTAUTOMATION FRAME-CAPTURE READ-ONLY CHECK"
echo "NO BUILD / NO LAUNCH / NO ASSET PROCESSING / NO INSTALL / NO SOURCE CHANGE"
echo "PROJECT JSON:"
if [[ -r "${PROJECT}/project.json" ]]; then
  sed -n '1,260p' "${PROJECT}/project.json"
else
  echo "MISSING: ${PROJECT}/project.json"
fi
echo "SCRIPTAUTOMATION BUILT ARTIFACTS:"
find "${BIN}" -maxdepth 2 -type f -iname '*ScriptAutomation*' -printf '%p %s bytes\n' 2>/dev/null | sort || true
echo "SCRIPTAUTOMATION PROJECT/CACHE REGISTRY:"
find "${PROJECT}/Registry" "${PROJECT}/Cache/linux" -maxdepth 4 -type f \( -name '*.setreg' -o -name '*.json' \) -print0 2>/dev/null |
  xargs -0 grep -HnEi 'ScriptAutomation|AutomationScripts|\.sa\.lua|run.?script' 2>/dev/null | sed -n '1,800p' || true
echo "OFFICIAL AUTOMATION SCRIPT ENTRY POINTS:"
grep -RInE 'AZ_CVAR|AZ_CONSOLEFUNC|CommandLine|GetSwitchValue|run.?script|script.?run|\.sa\.lua|FrameCaptureRequestBus|CaptureScreenshot' \
  "${O3DE}/Gems/ScriptAutomation" "${O3DE}/AutomatedTesting/Assets/AutomatedTestScripts" \
  --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.lua' --include='*.setreg' --include='*.json' \
  2>/dev/null | sed -n '1,1200p' || true
echo "OFFICIAL SCREENSHOT SCRIPTS:"
for script in \
  "${O3DE}/AutomatedTesting/Assets/AutomatedTestScripts/screenshot.sa.lua" \
  "${O3DE}/Gems/ScriptAutomation/Assets/AutomationScripts/GenericRenderScreenshotTest.lua"; do
  echo "===== ${script} ====="
  if [[ -r "${script}" ]]; then sed -n '1,320p' "${script}"; else echo "MISSING"; fi
done
echo "LAUNCHER SCRIPT/CAPTURE STRINGS:"
strings "${BIN}/STW.GameLauncher" 2>/dev/null |
  grep -Ei 'ScriptAutomation|AutomationScripts|\.sa\.lua|run.?script|CaptureScreenshot|FrameCaptureRequestBus' |
  sort -u | sed -n '1,800p' || true
echo "COST INCURRED: \$0.00"
