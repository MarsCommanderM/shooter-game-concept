#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="${HOME}"
O3DE="${ROOT}/o3de-2605"
BUILD="${ROOT}/stw-o3de-build/linux"
PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"

echo "O3DE 26.05 NATIVE FRAME-CAPTURE API READ-ONLY DISCOVERY"
echo "NO BUILD / NO LAUNCH / NO ASSET PROCESSING / NO INSTALL / NO SOURCE CHANGE"
if git -C "${O3DE}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git -C "${O3DE}" rev-parse HEAD
else
  echo "O3DE GIT METADATA: unavailable; continuing read-only source inspection"
  test -r "${O3DE}/engine.json"
fi
echo "FRAME CAPTURE SOURCE FILES:"
find "${O3DE}" -type f \( -iname '*FrameCapture*' -o -iname '*ScreenShot*' -o -iname '*Screenshot*' \) -print | sort | sed -n '1,500p'
echo "FRAME CAPTURE SYMBOLS / CONSOLE COMMANDS:"
grep -RInE 'AZ_CVAR|AZ_CONSOLEFUNC|FrameCaptureRequestBus|CaptureScreenshot|CapturePassAttachment|Screen[Ss]hot|screen[Ss]hot|captureFrame|capture_frame|r_capture'   "${O3DE}/Gems/Atom" "${O3DE}/Code"   --include='*.cpp' --include='*.h' --include='*.hpp' --include='*.inl'   2>/dev/null | sed -n '1,1600p' || true
echo "BUILT FRAME CAPTURE STRINGS:"
for binary in "${BUILD}/bin/profile/libAtom_Feature_Common.so" "${BUILD}/bin/profile/libAtom_RPI.Private.so" "${BUILD}/bin/profile/STW.GameLauncher"; do
  echo "===== ${binary} ====="
  strings "${binary}" 2>/dev/null | grep -Ei 'screenshot|frame.?capture|capture.?frame|capturepass|capture.?attachment' | sort -u | sed -n '1,500p' || true
done
echo "PROJECT AUTOEXEC:"
find "${PROJECT}" "${PROJECT}/Cache/linux" -maxdepth 3 -type f \( -name '*.cfg' -o -name '*.setreg' \) -print0 2>/dev/null |
  xargs -0 grep -HnEi 'screenshot|frame.?capture|capture.?frame|LoadLevel' 2>/dev/null | sed -n '1,500p' || true
echo "COST INCURRED: \$0.00"
