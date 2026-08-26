#!/usr/bin/env bash
set -Eeuo pipefail

readonly O3DE="${HOME}/o3de-2605"
readonly PROJECT="${HOME}/stw-o3de-worktree/stw-o3de/Project"
readonly BIN="${HOME}/stw-o3de-build/linux/bin/profile"
readonly STATE="${HOME}/stw-o3de-gate"

echo "STW O3DE HEADLESS AUDIO READ-ONLY INVENTORY"
echo "NO BUILD"
echo "NO LAUNCH"
echo "NO INSTALL"
echo "NO SOURCE CHANGE"
echo "IDENTITY:"
id
printf 'UID=%s GID=%s HOME=%s\n' "$(id -u)" "$(id -g)" "${HOME}"
printf 'XDG_RUNTIME_DIR=%q\n' "${XDG_RUNTIME_DIR-}"
uid="$(id -u)"
echo "RUNTIME PATHS:"
for p in "/run/user" "/run/user/${uid}" "${XDG_RUNTIME_DIR-}" "${STATE}"; do
  [[ -n "${p}" ]] || continue
  if [[ -e "${p}" ]]; then stat -c '%A %a %U:%G %u:%g %n' "${p}"; else echo "MISSING ${p}"; fi
done

echo "AUDIO COMMANDS:"
for c in pactl pulseaudio pipewire pipewire-pulse pw-cli pw-dump aplay arecord speaker-test; do
  printf '%-18s %s\n' "${c}" "$(command -v "${c}" 2>/dev/null || echo MISSING)"
done
echo "AUDIO PROCESSES:"
ps -eo pid,ppid,user,stat,comm,args --sort=pid | grep -Ei 'pulse|pipewire|wireplumber|jack|alsa' | grep -v grep || true
echo "ALSA DEVICES:"
[[ -r /proc/asound/cards ]] && cat /proc/asound/cards || echo "/proc/asound/cards MISSING"
[[ -r /proc/asound/devices ]] && cat /proc/asound/devices || echo "/proc/asound/devices MISSING"
command -v aplay >/dev/null && aplay -l 2>&1 || true
echo "AUDIO ENVIRONMENT PRESENCE (VALUES REDACTED):"
env | sed -n -E '/^(XDG_RUNTIME_DIR|PULSE_SERVER|PIPEWIRE_REMOTE|ALSA_CONFIG_PATH|SDL_AUDIODRIVER|SDL_AUDIO_DRIVER)=/s/=.*/=<present>/p'

echo "O3DE AUDIO GEMS:"
find "${O3DE}/Gems" -maxdepth 3 -type f \( -iname 'gem.json' -o -iname '*.setreg' \) -path '*Audio*' -print | sort | sed -n '1,240p'
echo "PROJECT AUDIO REFERENCES:"
grep -RInE 'AudioSystem|MiniAudio|Audio|Sound' "${PROJECT}/project.json" "${PROJECT}/Registry" "${PROJECT}/Config" 2>/dev/null | sed -n '1,300p' || true

echo "O3DE AUDIO RUNTIME OPTIONS / SETTINGS:"
search_roots=(
  "${O3DE}/Gems/AudioSystem"
  "${O3DE}/Gems/AudioEngineWwise"
  "${O3DE}/Gems/AudioEngineNoSound"
  "${O3DE}/Gems/MiniAudio"
  "${O3DE}/Gems"
)
for root in "${search_roots[@]}"; do
  [[ -d "${root}" ]] || continue
  echo "--- SEARCH ROOT ${root} ---"
  grep -RInE --include='*.cpp' --include='*.h' --include='*.inl' --include='*.setreg' --include='*.json' --include='*.md' --include='*.cmake' \
    'AudioEngineNoSound|NoSound|null audio|NullAudio|disable.?audio|audio.?disable|no.?audio|AudioSystemImplementation|MiniAudio|AZ_CVAR.*[Aa]udio|SettingsRegistry.*[Aa]udio|s_[A-Za-z0-9_]*[Aa]udio' "${root}" 2>/dev/null | sed -n '1,500p'
done

echo "LAUNCHER/BINARY AUDIO STRINGS:"
strings "${BIN}/STW.GameLauncher" "${BIN}/libAudioSystem.so" "${BIN}/libO3DEMiniAudio.so" 2>/dev/null |
  grep -Ei 'AudioEngineNoSound|NoSound|null.?audio|disable.?audio|no.?audio|MiniAudio|AudioSystemImplementation|audio.*enabled|s_[A-Za-z0-9_]*audio' |
  sort -u | sed -n '1,300p' || true

echo "O3DE AUDIO REGISTRY INPUTS:"
find "${O3DE}/Gems" "${PROJECT}" "${BIN}/Registry" -type f \( -iname '*audio*.setreg' -o -iname '*miniaudio*.setreg' -o -iname '*audio*.json' \) -print 2>/dev/null | sort | sed -n '1,300p'
echo "COST INCURRED: \$0.00"
