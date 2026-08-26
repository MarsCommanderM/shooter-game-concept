#!/usr/bin/env bash
set -Eeuo pipefail

echo "LIGHTNING NATIVE CAPTURE READ-ONLY INVENTORY"
for command_name in ffmpeg avconv import magick convert identify scrot maim xwd xwininfo xrandr xset gnome-screenshot python3; do
  printf '%-20s %s\n' "${command_name}" "$(command -v "${command_name}" 2>/dev/null || echo MISSING)"
done
echo "PYTHON MODULES:"
python3 - <<'PY'
for name in ("PIL","mss","pyautogui","Xlib","cv2"):
    try:
        module=__import__(name)
        print(f"{name}: PRESENT {getattr(module,'__version__','')}")
    except Exception as exc:
        print(f"{name}: MISSING ({type(exc).__name__})")
PY
echo "X11 LIBRARIES:"
ldconfig -p 2>/dev/null | grep -E 'libX11|libxcb|libXext' | sed -n '1,120p' || true
echo "RECENT GAMELAUNCHER LOG FILES:"
find "${HOME}/stw-o3de-worktree/stw-o3de/Project/user" "${HOME}/stw-o3de-gate" -type f \( -iname '*game*.log' -o -iname '*launcher*.log' -o -iname '*game*.txt' \) -printf '%p | %s bytes | %TY-%Tm-%Td %TH:%TM:%TS\n' 2>/dev/null | sort | tail -n 80
echo "NO BUILD"
echo "NO LAUNCH"
echo "COST INCURRED: \$0.00"
