#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly WORKTREE="${ROOT}/stw-o3de-worktree"
readonly PROJECT="${WORKTREE}/stw-o3de/Project"
readonly CACHE="${PROJECT}/Cache/linux"
readonly STDIO="${LOGS}/game-launcher-native.stdout-stderr.log"

echo "STW O3DE BLACK-FRAME LEVEL/CAMERA READ-ONLY AUDIT"
echo "NO BUILD / NO LAUNCH / NO ASSET PROCESSING / NO INSTALL / NO SOURCE CHANGE"

echo "AUTHORITATIVE SOURCE:"
git -C "${WORKTREE}" rev-parse HEAD
git -C "${WORKTREE}" status --short

echo "LAUNCHER LOG TAIL:"
stat -c '%n | %s bytes | %y' "${STDIO}"
tail -n 160 "${STDIO}"

echo "PROJECT MANIFEST:"
sed -n '1,260p' "${PROJECT}/project.json"

echo "PROJECT REGISTRY FILES:"
find "${PROJECT}/Registry" "${PROJECT}/user/Registry" -maxdepth 4 -type f -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort
while IFS= read -r file; do
  echo "===== ${file} ====="
  sed -n '1,260p' "${file}" 2>/dev/null || true
done < <(find "${PROJECT}/Registry" "${PROJECT}/user/Registry" -maxdepth 4 -type f \( -name '*.setreg' -o -name '*.json' \) -print 2>/dev/null | sort)

echo "CACHED REGISTRY/BOOTSTRAP INPUTS:"
find "${CACHE}" -type f \( -iname '*.setreg' -o -iname '*bootstrap*' -o -iname '*defaultlevel*' \) -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort | sed -n '1,600p'
grep -RInE 'LoadLevel|defaultlevel|Camera|Viewport' "${CACHE}/registry" "${CACHE}/config" 2>/dev/null | sed -n '1,600p' || true

echo "DEFAULTLEVEL SOURCE:"
if [[ -d "${PROJECT}/Levels/defaultlevel" ]]; then
  find "${PROJECT}/Levels/defaultlevel" -maxdepth 5 -type f -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' | sort
  while IFS= read -r file; do
    case "${file}" in
      *.prefab|*.json|*.setreg|*.xml|*.txt)
        echo "===== ${file} ====="
        sed -n '1,500p' "${file}" ;;
    esac
  done < <(find "${PROJECT}/Levels/defaultlevel" -maxdepth 5 -type f -print | sort)
else
  echo "DEFAULTLEVEL SOURCE DIRECTORY: MISSING"
fi

echo "DEFAULTLEVEL CACHE:"
find "${CACHE}/levels/defaultlevel" -maxdepth 5 -type f -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort
file "${CACHE}/levels/defaultlevel/"* 2>/dev/null || true

echo "MIGRATION GATE DOCUMENT:"
sed -n '1,420p' "${WORKTREE}/stw-o3de/MIGRATION.md" 2>/dev/null || echo "MIGRATION.md missing"

echo "PROJECT/GEM VISUAL BOOT REFERENCES:"
grep -RInE 'LoadLevel|defaultlevel|Camera|Viewport|RenderPipeline|MeshComponent|DirectionalLight|SkyBox|CreateEntity|TransformBus'   "${WORKTREE}/stw-o3de/Project" "${WORKTREE}/stw-o3de/Gems"   --exclude-dir=Cache --exclude-dir=user --exclude='*.png' --exclude='*.jpg' --exclude='*.dds'   2>/dev/null | sed -n '1,1000p' || true

echo "RECENT GAME-SPECIFIC LOG FILES:"
find "${PROJECT}/user/log" -maxdepth 2 -type f -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort | tail -n 120

echo "COST INCURRED: \$0.00"
