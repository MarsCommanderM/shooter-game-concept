#!/usr/bin/env bash
set -Eeuo pipefail

readonly ROOT="${HOME}"
readonly STATE="${ROOT}/stw-o3de-gate"
readonly LOGS="${STATE}/logs"
readonly CAPTURE="${LOGS}/assetprocessor-engine-associated.stdout-stderr.log"
readonly COMBINED="${LOGS}/assetprocessor-engine-associated.combined.log"
readonly REPORT="${STATE}/assetprocessor-final-evidence.txt"
readonly PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
readonly CACHE="${PROJECT}/Cache/linux"

mkdir -p "${LOGS}"
: >"${REPORT}"
report() { printf '%s\n' "$*" | tee -a "${REPORT}"; }

report "STW O3DE ASSET PROCESSOR FINAL READ-ONLY EVIDENCE"
[[ -s "${CAPTURE}" ]] || { report "FIRST ERROR: prior AssetProcessor capture missing"; exit 1; }
[[ -s "${COMBINED}" ]] || { report "FIRST ERROR: prior combined log missing"; exit 1; }
[[ -d "${CACHE}" ]] || { report "FIRST ERROR: Linux asset cache missing"; exit 1; }

report "ASSET PROCESSOR CAPTURE: ${CAPTURE}"
report "CAPTURE METADATA: $(stat -c '%s bytes | %y' "${CAPTURE}")"
report "COMBINED LOG: ${COMBINED}"
report "COMBINED METADATA: $(stat -c '%s bytes | %y' "${COMBINED}")"

report "PLATFORMS:"
grep -oE '\("[[:alnum:]_-]+"\)' "${CAPTURE}" | tr -d '()"' | sort -u | sed 's/^/  /' | tee -a "${REPORT}" || true

processing_count="$(grep -c '^AssetProcessor: Processing ' "${CAPTURE}" || true)"
processed_count="$(grep -c '^AssetProcessor: Processed ' "${CAPTURE}" || true)"
failed_count="$(grep -Eic 'failed jobs?[=: ]+[1-9]|jobs? failed[=: ]+[1-9]|Job .* failed' "${CAPTURE}" || true)"
report "PROCESSING LINES: ${processing_count}"
report "PROCESSED LINES: ${processed_count}"
report "FAILED JOB MATCHES: ${failed_count}"

report "SCAN FOLDER EVIDENCE:"
grep -niE 'scan folder|scanfolder|watch folder' "${CAPTURE}" | sed -n '1,120p' | tee -a "${REPORT}" || true
report "PROCESSING ROOTS:"
grep '^AssetProcessor: Processing ' "${CAPTURE}" | sed -E 's#^AssetProcessor: Processing ##; s# \([^)]*\)\.\.\.$##' | sed -E 's#^"?([^/]+).*#  \1#' | sort -u | tee -a "${REPORT}" || true

report "FIRST WARNING WITH CONTEXT:"
warning_line="$(grep -n -m1 -Ei 'Trace::Warning|(^|[^[:alpha:]])warning([: ]|$)' "${COMBINED}" | cut -d: -f1 || true)"
if [[ -n "${warning_line}" ]]; then
  start=$((warning_line > 3 ? warning_line - 3 : 1))
  end=$((warning_line + 6))
  sed -n "${start},${end}p" "${COMBINED}" | tee -a "${REPORT}"
else
  report "  NONE"
fi

report "ERROR-LIKE TEXT WITH CONTEXT:"
error_line="$(grep -n -m1 -Ei 'Trace::Error|(^|[^[:alpha:]])error([: ]|$)|failed to initialize|no scan folders|no platforms' "${COMBINED}" | cut -d: -f1 || true)"
if [[ -n "${error_line}" ]]; then
  start=$((error_line > 3 ? error_line - 3 : 1))
  end=$((error_line + 10))
  sed -n "${start},${end}p" "${COMBINED}" | tee -a "${REPORT}"
else
  report "  NONE"
fi

report "CRITICAL CONFIGURATION ERRORS:"
critical="$(grep -Ein 'Failed to Initialize from AssetProcessorPlatformConfig|Unable to find any scan folders|no platforms appear to be enabled' "${COMBINED}" || true)"
printf '%s\n' "${critical:-  NONE}" | tee -a "${REPORT}"

report "COMPLETION EVIDENCE:"
grep -E 'Asset Processor Batch Processing complete|Asset Processor Batch Processing Completed' "${CAPTURE}" | tail -n 5 | tee -a "${REPORT}" || true
report "CACHE: ${CACHE} ($(du -sh "${CACHE}" | awk '{print $1}'))"
report "CACHE FILES: $(find "${CACHE}" -type f | wc -l)"
report "CACHE SETREG FILES: $(find "${CACHE}" -type f -name '*.setreg' | wc -l)"
report "FILES CHANGED: evidence report only; no build, AssetProcessor, Launcher, Editor, project, or gameplay invocation"
report "ASSET PROCESSOR RE-RUN: NO"
report "GAME LAUNCHER STARTED: NO"
report "COST INCURRED: \$0.00"
