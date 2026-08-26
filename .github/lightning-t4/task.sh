#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
readonly CACHE="${PROJECT}/Cache/linux"
readonly LOG="${PROJECT}/user/log/Game.log"
readonly PREFAB="${PROJECT}/Levels/DefaultLevel/DefaultLevel.prefab"
readonly BOOTSTRAP="${CACHE}/bootstrap.client.profile.setreg"
readonly STDIO="${ROOT}/stw-o3de-gate/logs/game-launcher-native.stdout-stderr.log"

echo "STW O3DE LEVEL-LOAD READ-ONLY ROOT-CAUSE AUDIT"
echo "NO BUILD / NO LAUNCH / NO ASSET PROCESSING / NO INSTALL / NO SOURCE CHANGE"

echo "GAME LOG:"
stat -c '%n | %s bytes | %y' "${LOG}"
nl -ba "${LOG}" | sed -n '1,500p'

echo "LAUNCH STDIO END:"
stat -c '%n | %s bytes | %y' "${STDIO}"
nl -ba "${STDIO}" | tail -n 220

echo "CLIENT PROFILE BOOTSTRAP:"
stat -c '%n | %s bytes | %y' "${BOOTSTRAP}"
grep -nEi -C 6 'LoadLevel|defaultlevel|project_path|project_name|asset|cache|bootstrap' "${BOOTSTRAP}" | sed -n '1,500p' || true

echo "LOAD LEVEL PRODUCT:"
find "${CACHE}" -maxdepth 3 -type f -iname '*load_level*' -o -iname 'load_level.setreg' 2>/dev/null | while IFS= read -r file; do
  stat -c '%n | %s bytes | %y' "${file}"
  sed -n '1,160p' "${file}" 2>/dev/null || true
done

echo "DEFAULTLEVEL PREFAB:"
stat -c '%n | %s bytes | %y' "${PREFAB}"
nl -ba "${PREFAB}" | sed -n '1,520p'

echo "DEFAULTLEVEL COMPONENT SUMMARY:"
grep -nE '"Name"|\$type|Active|Fov|Near|Far|Position|Translation|Rotation|ModelAsset|Material|Intensity|Color|Camera' "${PREFAB}" | sed -n '1,800p' || true

echo "CACHED LEVEL:"
stat -c '%n | %s bytes | %y' "${CACHE}/levels/defaultlevel/defaultlevel.spawnable"

echo "PAK MISSING ASSETS:"
if [[ -s "${PROJECT}/user/log/PakMissingAssets.log" ]]; then
  nl -ba "${PROJECT}/user/log/PakMissingAssets.log" | sed -n '1,300p'
else
  echo "none"
fi

echo "COST INCURRED: \$0.00"
