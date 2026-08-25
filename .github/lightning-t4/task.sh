#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly REPORT="${STATE}/registry-provenance-audit.txt"
readonly O3DE_EXPECTED="${ROOT}/o3de-2605"
readonly PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
readonly BUILD="${ROOT}/stw-o3de-build/linux"
readonly BIN="${BUILD}/bin/profile"
readonly AP="${BIN}/AssetProcessorBatch"
readonly MANIFEST="${ROOT}/.o3de/o3de_manifest.json"

mkdir -p "${LOGS}"
: >"${REPORT}"
report() { printf '%s\n' "$*" | tee -a "${REPORT}"; }
section() { report ""; report "===== $* ====="; }

section "GUARDS"
report "HOME=${ROOT}"
report "PROJECT=${PROJECT}"
report "BUILD=${BUILD}"
[[ -f "${BUILD}/CMakeCache.txt" ]] || { report "BLOCKER: CMakeCache missing"; exit 1; }
[[ -x "${AP}" ]] || { report "BLOCKER: AssetProcessorBatch missing"; exit 1; }
report "BUILD TREE PRESERVED=YES"
report "NO BUILD OR ASSET PROCESSOR INVOCATION PERFORMED=YES"

section "CMAKE ENGINE PROVENANCE"
grep -E '^(CMAKE_HOME_DIRECTORY|LY_ROOT_FOLDER|LY_3RDPARTY_PATH|O3DE|.*ENGINE.*|.*PROJECT.*):' "${BUILD}/CMakeCache.txt" 2>/dev/null | sed -n '1,160p' | tee -a "${REPORT}" || true
report "CMAKE CACHE SOURCE PATH REFERENCES:"
grep -oE '/[^ ;"]+(o3de-2605|stw-o3de-worktree)[^ ;"]*' "${BUILD}/CMakeCache.txt" | sort -u | sed -n '1,120p' | tee -a "${REPORT}" || true

section "ENGINE"
engine_root="${O3DE_EXPECTED}"
report "ENGINE ROOT CANDIDATE=${engine_root}"
report "ENGINE ROOT REALPATH=$(realpath -e "${engine_root}" 2>/dev/null || echo MISSING)"
report "ENGINE GIT HEAD=$(git -C "${engine_root}" rev-parse HEAD 2>/dev/null || echo MISSING)"
report "ENGINE GIT DESCRIBE=$(git -C "${engine_root}" describe --tags --exact-match 2>/dev/null || echo NO_EXACT_TAG)"
for f in "${engine_root}/engine.json" "${engine_root}/Registry/AssetProcessorPlatformConfig.setreg"; do
  if [[ -r "$f" && -s "$f" ]]; then
    report "READABLE NONEMPTY: $f ($(stat -c '%s bytes; %y' "$f"))"
  else
    report "MISSING/UNREADABLE/EMPTY: $f"
  fi
done

section "JSON ASSOCIATIONS"
python3 - "${engine_root}/engine.json" "${PROJECT}/project.json" "${PROJECT}/user/project.json" "${MANIFEST}" <<'PY' | tee -a "${REPORT}"
import json, os, sys
for path in sys.argv[1:]:
    print(f"FILE: {path}")
    if not os.path.isfile(path):
        print("  PRESENT: NO")
        continue
    print("  PRESENT: YES")
    try:
        with open(path, encoding="utf-8") as fh:
            data=json.load(fh)
    except Exception as exc:
        print(f"  JSON: INVALID ({type(exc).__name__})")
        continue
    print("  JSON: VALID")
    for key in ("engine_name","display_name","version","O3DEVersion","project_name","project_id","engine","engine_path"):
        if key in data:
            print(f"  {key}: {data[key]}")
    for key in ("engines","projects","external_subdirectories","gem_names","gems"):
        value=data.get(key)
        if value is not None:
            print(f"  {key}:")
            if isinstance(value,list):
                for item in value:
                    if isinstance(item,dict):
                        safe={k:v for k,v in item.items() if k in ("path","name","engine_name","project_name")}
                        print(f"    - {safe}")
                    else:
                        print(f"    - {item}")
            else:
                print(f"    {value}")
PY

section "REGISTERED PATH EXISTENCE"
python3 - "${MANIFEST}" <<'PY' | tee -a "${REPORT}"
import json, os, sys
p=sys.argv[1]
if not os.path.isfile(p):
    print("MANIFEST PATH CHECK: MANIFEST MISSING")
    raise SystemExit
with open(p,encoding="utf-8") as f: d=json.load(f)
for key in ("engines","projects","external_subdirectories"):
    for item in d.get(key,[]) or []:
        path=item.get("path") if isinstance(item,dict) else item
        if isinstance(path,str):
            print(f"{key}: {path} -> {'EXISTS' if os.path.exists(os.path.expanduser(path)) else 'MISSING'}")
PY

section "REGISTRY INVENTORY"
registry_dirs=("${engine_root}/Registry" "${PROJECT}/Registry" "${PROJECT}/user/Registry" "${ROOT}/.o3de/Registry" "${BIN}/Registry" "${BUILD}/runtime_dependencies/profile")
for dir in "${registry_dirs[@]}"; do
  report "DIRECTORY: $dir"
  if [[ -d "$dir" ]]; then
    find "$dir" -maxdepth 3 -type f \( -name '*.setreg' -o -name '*.json' -o -name '*.cmake' \) -printf '  %p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort | sed -n '1,240p' | tee -a "${REPORT}"
  else
    report "  MISSING"
  fi
done

section "ASSET PROCESSOR PLATFORM CONFIG CONTENT"
platform_cfg="${engine_root}/Registry/AssetProcessorPlatformConfig.setreg"
if [[ -r "${platform_cfg}" ]]; then
  report "PLATFORM KEY MATCHES=$(grep -c 'Platform ' "${platform_cfg}" || true)"
  report "SCAN FOLDER KEY MATCHES=$(grep -c 'ScanFolder' "${platform_cfg}" || true)"
  grep -nE 'AssetProcessor|Platform |ScanFolder' "${platform_cfg}" | sed -n '1,220p' | tee -a "${REPORT}" || true
fi

section "GENERATED REGISTRY AND GEM PATHS"
find "${BUILD}" "${BIN}" -type f \( -name 'cmake_dependencies*.setreg' -o -name '*gem*.setreg' -o -name '*Gem*.setreg' -o -name 'bootstrap*.setreg' -o -name '*AssetProcessor*.setreg' \) -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort | sed -n '1,320p' | tee -a "${REPORT}"
report "GENERATED PATH REFERENCES:"
find "${BUILD}" "${BIN}" -type f -name '*.setreg' -print0 2>/dev/null | xargs -0 grep -hE 'o3de-2605|stw-o3de-worktree|/Gems/' 2>/dev/null | sed -n '1,240p' | tee -a "${REPORT}" || true

section "ACTUAL PREVIOUS INVOCATION"
prior="${LOGS}/assetprocessorbatch.stdout-stderr.log"
report "PRIOR LOG=${prior}"
if [[ -f "${STATE}/asset-and-launch-gate.txt" ]]; then
  grep -F 'ASSET PROCESSOR COMMAND:' "${STATE}/asset-and-launch-gate.txt" | tail -n1 | tee -a "${REPORT}" || true
fi
report "ASCII OPTION EXPECTED=--project-path="

section "SUMMARY"
report "AUDIT LOG=${REPORT}"
report "FILES CHANGED=diagnostic log only; no project/gameplay/build changes"
report "COST INCURRED=\$0.00"
