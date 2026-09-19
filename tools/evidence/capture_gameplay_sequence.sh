#!/usr/bin/env bash
# Records a short REAL gameplay clip (first-person, automated acceptance play) from the STW launcher that the last
# gate run built, and turns it into PNG frames, a contact sheet and luma metrics. Read-only with respect to the
# repository and the build: it only writes under stw-o3de-gate/gameplay-sequence-<timestamp>/.
#
# Uses the opt-in evidence mode of the runtime (STW_NATIVE_CAPTURE_INTERVAL / _MAX_FRAMES), which the standard
# gate does not set. Requires a finished gate run first (build + asset cache in stw-o3de-worktree / stw-o3de-build).
#
# usage: tools/evidence/capture_gameplay_sequence.sh [frames=48] [interval_seconds=0.5]
set -Eeuo pipefail

FRAMES="${1:-48}"
INTERVAL="${2:-0.5}"
ROOT="/teamspace/studios/this_studio"
ENGINE="${ROOT}/o3de-2605"
PROJECT="${ROOT}/stw-o3de-worktree/stw-o3de/Project"
BIN="${ROOT}/stw-o3de-build/linux/bin/profile"
LAUNCHER="${BIN}/STW.GameLauncher"
RUN_DIR="${ROOT}/stw-o3de-gate/gameplay-sequence-$(date -u +%Y%m%dT%H%M%SZ)"
ICD="${RUN_DIR}/nvidia_icd.json"
RUNTIME="${RUN_DIR}/xdg-runtime"
ALSA="${RUNTIME}/asound-null.conf"
BASE="${RUN_DIR}/stw-gameplay-sequence.ppm"
xvfb_pid=""; launcher_pid=""

cleanup() {
  [[ -n "${launcher_pid}" ]] && kill "${launcher_pid}" 2>/dev/null || true
  [[ -n "${xvfb_pid}" ]] && kill "${xvfb_pid}" 2>/dev/null || true
}
trap cleanup EXIT

[[ -x "${LAUNCHER}" ]] || { echo "SEQUENCE_CAPTURE=BLOCKED reason=no_launcher path=${LAUNCHER} (run the gate first)"; exit 2; }
mkdir -p "${RUN_DIR}" "${RUNTIME}"; chmod 700 "${RUNTIME}"

# Process-local Vulkan ICD for the already-installed NVIDIA library; no global Vulkan state changes.
nvidia_lib="$(ldconfig -p 2>/dev/null | awk '$1 == "libGLX_nvidia.so.0" {print $NF; exit}')"
[[ -n "${nvidia_lib}" && -r "${nvidia_lib}" ]]
python3 - "${ICD}" "${nvidia_lib}" <<'PY'
import json, sys
path, library = sys.argv[1:]
json.dump({"file_format_version": "1.0.0", "ICD": {"library_path": library, "api_version": "1.3.280"}},
          open(path, "w", encoding="utf-8"), indent=2)
PY
printf 'pcm.!default { type null }\n' >"${ALSA}"

display_number=""
for n in $(seq 121 150); do [[ ! -S "/tmp/.X11-unix/X${n}" ]] && { display_number="${n}"; break; }; done
[[ -n "${display_number}" ]]; display=":${display_number}"
Xvfb "${display}" -screen 0 1920x1080x24 -nolisten tcp -noreset >"${RUN_DIR}/xvfb.log" 2>&1 & xvfb_pid=$!
for _ in $(seq 1 40); do [[ -S "/tmp/.X11-unix/X${display_number}" ]] && break; sleep .25; done

echo "SEQUENCE_CAPTURE_RUN_DIR=${RUN_DIR} frames=${FRAMES} interval=${INTERVAL}s"
(
  cd "${RUN_DIR}"
  exec setsid env DISPLAY="${display}" XDG_RUNTIME_DIR="${RUNTIME}" ALSA_CONFIG_PATH="${ALSA}" \
    STW_NATIVE_CAPTURE_PATH="${BASE}" STW_NATIVE_CAPTURE_INTERVAL="${INTERVAL}" STW_NATIVE_CAPTURE_MAX_FRAMES="${FRAMES}" \
    STW_PHYSX_ACCEPTANCE=1 \
    VK_ICD_FILENAMES="${ICD}" LD_LIBRARY_PATH="${BIN}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
    "${LAUNCHER}" "--project-path=${PROJECT}" "--engine-path=${ENGINE}" \
    --bg_ConnectToAssetProcessor false "--regset=/Amazon/AzCore/Bootstrap/linux_wait_for_connect=0" -sys_audio_disable 1
) >"${RUN_DIR}/launcher.log" 2>&1 &
launcher_pid=$!

# Wait for the requested number of frames (or the launcher exiting / a generous deadline).
deadline=$(( $(date +%s) + 60 + $(python3 -c "print(int(${FRAMES}*${INTERVAL}*8))") ))
while [[ $(date +%s) -lt ${deadline} ]]; do
  count="$(find "${RUN_DIR}" -maxdepth 1 -name 'stw-gameplay-sequence_*.ppm' | wc -l)"
  [[ "${count}" -ge "${FRAMES}" ]] && break
  kill -0 "${launcher_pid}" 2>/dev/null || break
  sleep 2
done
cleanup; sleep 1
count="$(find "${RUN_DIR}" -maxdepth 1 -name 'stw-gameplay-sequence_*.ppm' | wc -l)"
echo "SEQUENCE_CAPTURED_FRAMES=${count}"
[[ "${count}" -gt 0 ]] || { echo "SEQUENCE_CAPTURE=FAIL reason=no_frames"; exit 1; }

python3 - "${RUN_DIR}" <<'PY'
import glob, os, sys
import numpy as np
from PIL import Image
run = sys.argv[1]
paths = sorted(glob.glob(os.path.join(run, "stw-gameplay-sequence_*.ppm")))
rows = []
for p in paths:
    im = Image.open(p).convert("RGB")
    im.save(p.replace(".ppm", ".png"), optimize=True)
    a = np.asarray(im).astype(float)
    y = (0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2])[100:]
    rows.append((os.path.basename(p), y.mean(), y.std(), 100 * (y >= 250).mean(), 100 * (y <= 8).mean()))
    os.remove(p)  # only the frame this script itself just wrote; the PNG keeps the data
m = np.array([r[1:] for r in rows])
print("SEQUENCE_LUMA mean=%.1f (min %.1f max %.1f) rms=%.1f clipped>=250=%.2f%% crushed<=8=%.2f%% frames=%d" % (
    m[:, 0].mean(), m[:, 0].min(), m[:, 0].max(), m[:, 1].mean(), m[:, 2].mean(), m[:, 3].mean(), len(rows)))
# contact sheet: 8 evenly spaced frames, 4x2
pngs = sorted(glob.glob(os.path.join(run, "stw-gameplay-sequence_*.png")))
picks = [pngs[int(i * (len(pngs) - 1) / 7)] for i in range(8)] if len(pngs) >= 8 else pngs
tiles = [Image.open(p).convert("RGB").resize((640, 360)) for p in picks]
sheet = Image.new("RGB", (640 * 4, 360 * ((len(tiles) + 3) // 4)))
for i, t in enumerate(tiles):
    sheet.paste(t, ((i % 4) * 640, (i // 4) * 360))
sheet.save(os.path.join(run, "contact-sheet.png"))
print("CONTACT_SHEET=%s" % os.path.join(run, "contact-sheet.png"))
PY
echo "SEQUENCE_CAPTURE=DONE"
