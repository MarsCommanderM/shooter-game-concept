#!/usr/bin/env bash
set -Eeuo pipefail

# Cross-process production gate for the STW network player slice.
# It deliberately requires two real clients, two server authorities, replicated
# remote snapshots, and a render-ready remote EMotionFX/Atom presentation.

ROOT="/teamspace/studios/this_studio"
WORKSPACE="${GITHUB_WORKSPACE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
ENGINE="${ROOT}/o3de-2605"
O3DE_ROOT="${ROOT}/stw-o3de-worktree/stw-o3de"
PROJECT="${O3DE_ROOT}/Project"
BUILD="${ROOT}/stw-o3de-build/linux"
BIN="${BUILD}/bin/profile"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
RUN_DIR="${ROOT}/stw-o3de-gate/multiplayer-${RUN_ID}"
SERVER_LOG="${PROJECT}/user/log/Server.log"
SERVER_PORT="$((46000 + ($$ % 500)))"
DISPLAY_BASE="$((200 + ($$ % 40)))"
DISPLAY_ONE=":${DISPLAY_BASE}"
DISPLAY_TWO=":$((DISPLAY_BASE + 1))"
DISPLAY_THREE=":$((DISPLAY_BASE + 2))"
REMOTE_CONSOLE_SERVER="$((47000 + ($$ % 500)))"
REMOTE_CONSOLE_ONE="$((47500 + ($$ % 500)))"
REMOTE_CONSOLE_TWO="$((48000 + ($$ % 500)))"
REMOTE_CONSOLE_THREE="$((48500 + ($$ % 500)))"
DURATION="${STW_MP_DURATION:-30}"
# Second observation window: one client reconnects into the freed roster
# slot with command delay/loss enabled only for that one process, proving
# the gated transport bookkeeping and the disconnect/respawn cleanup path.
DELAY_DURATION="${STW_MP_DELAY_DURATION:-20}"
GATE_DELAY_STEPS="${STW_MP_GATE_DELAY_STEPS:-3}"
GATE_LOSS_EVERY="${STW_MP_GATE_LOSS_EVERY:-4}"
XDG_RUNTIME_DIR="${RUN_DIR}/xdg-runtime"
CLIENT_ONE_USER="${RUN_DIR}/client-one-user"
CLIENT_TWO_USER="${RUN_DIR}/client-two-user"
CLIENT_THREE_USER="${RUN_DIR}/client-three-user"

server_pid=""
client_one_pid=""
client_two_pid=""
client_three_pid=""
xvfb_one_pid=""
xvfb_two_pid=""
xvfb_three_pid=""
server_log_before=0

mkdir -p "${RUN_DIR}" "${XDG_RUNTIME_DIR}" "${CLIENT_ONE_USER}" "${CLIENT_TWO_USER}" "${CLIENT_THREE_USER}"
chmod 700 "${XDG_RUNTIME_DIR}" "${CLIENT_ONE_USER}" "${CLIENT_TWO_USER}" "${CLIENT_THREE_USER}"
exec > >(tee "${RUN_DIR}/report.log") 2>&1

cleanup() {
    set +e
    for pid in "${client_one_pid}" "${client_two_pid}" "${client_three_pid}" "${server_pid}" "${xvfb_one_pid}" "${xvfb_two_pid}" "${xvfb_three_pid}"; do
        [[ -n "${pid}" ]] && kill "${pid}" 2>/dev/null || true
    done
    wait "${client_one_pid}" "${client_two_pid}" "${client_three_pid}" "${server_pid}" "${xvfb_one_pid}" "${xvfb_two_pid}" "${xvfb_three_pid}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "STW MULTIPLAYER PRODUCTION GATE"
echo "GITHUB_REPOSITORY=${GITHUB_REPOSITORY:-UNSET}"
echo "GITHUB_REF=${GITHUB_REF:-UNSET}"
echo "SOURCE_COMMIT=$(git -C "${WORKSPACE}" rev-parse HEAD)"
echo "WORKSPACE=${WORKSPACE}"
echo "ENGINE=${ENGINE}"
echo "SERVER_PORT=${SERVER_PORT}"
echo "DURATION_SECONDS=${DURATION}"

[[ "${GITHUB_REPOSITORY:-MarsCommanderM/shooter-game-concept}" == "MarsCommanderM/shooter-game-concept" ]]
[[ -x "${BIN}/STW.HeadlessServerLauncher" ]]
[[ -x "${BIN}/STW.GameLauncher" ]]
[[ -s "${PROJECT}/Cache/linux/assets/network/stw_player/stw_player.network.spawnable" ]]
[[ -x "$(command -v Xvfb)" ]]

server_log_fingerprint=""
if [[ -f "${SERVER_LOG}" ]]; then
    server_log_before="$(wc -l < "${SERVER_LOG}")"
    server_log_fingerprint="$(head -n 1 "${SERVER_LOG}" || true)"
fi

export LD_LIBRARY_PATH="${BIN}"
export ALSA_CONFIG_PATH=/dev/null
export XDG_RUNTIME_DIR
export STW_MP_REWIND_FORWARD="${STW_MP_REWIND_FORWARD:-1}"

"${BIN}/STW.HeadlessServerLauncher" \
    --project-path="${PROJECT}" \
    --engine-path="${ENGINE}" \
    --project-user-path="${PROJECT}/user" \
    --bg_ConnectToAssetProcessor=false \
    --regset=/Amazon/AzCore/Bootstrap/linux_wait_for_connect=0 \
    -rhi=null -sys_audio_disable=1 \
    -sv_port="${SERVER_PORT}" -sv_portRange=0 \
    -sv_isDedicated=1 -sv_dedicated_host_onstartup=1 \
    -sv_terminateOnPlayerExit=0 \
    -log_RemoteConsolePort="${REMOTE_CONSOLE_SERVER}" \
    >"${RUN_DIR}/server.stdout" 2>&1 &
server_pid=$!

sleep 10
kill -0 "${server_pid}" 2>/dev/null

Xvfb "${DISPLAY_ONE}" -screen 0 1920x1080x24 -nolisten tcp >"${RUN_DIR}/xvfb-one.stdout" 2>&1 &
xvfb_one_pid=$!
Xvfb "${DISPLAY_TWO}" -screen 0 1920x1080x24 -nolisten tcp >"${RUN_DIR}/xvfb-two.stdout" 2>&1 &
xvfb_two_pid=$!
sleep 2

DISPLAY="${DISPLAY_ONE}" "${BIN}/STW.GameLauncher" \
    --project-path="${PROJECT}" \
    --engine-path="${ENGINE}" \
    --project-user-path="${CLIENT_ONE_USER}" \
    --bg_ConnectToAssetProcessor=false \
    --regset=/Amazon/AzCore/Bootstrap/linux_wait_for_connect=0 \
    -rhi=null -sys_audio_disable=1 \
    -connect=127.0.0.1:"${SERVER_PORT}" \
    -log_RemoteConsolePort="${REMOTE_CONSOLE_ONE}" \
    >"${RUN_DIR}/client-one.stdout" 2>&1 &
client_one_pid=$!

DISPLAY="${DISPLAY_TWO}" "${BIN}/STW.GameLauncher" \
    --project-path="${PROJECT}" \
    --engine-path="${ENGINE}" \
    --project-user-path="${CLIENT_TWO_USER}" \
    --bg_ConnectToAssetProcessor=false \
    --regset=/Amazon/AzCore/Bootstrap/linux_wait_for_connect=0 \
    -rhi=null -sys_audio_disable=1 \
    -connect=127.0.0.1:"${SERVER_PORT}" \
    -log_RemoteConsolePort="${REMOTE_CONSOLE_TWO}" \
    >"${RUN_DIR}/client-two.stdout" 2>&1 &
client_two_pid=$!

sleep "${DURATION}"
kill -0 "${server_pid}" 2>/dev/null
kill -0 "${client_one_pid}" 2>/dev/null
kill -0 "${client_two_pid}" 2>/dev/null
echo "PHASE_1_WINDOW_COMPLETE duration=${DURATION}"

# Disconnect / respawn cleanup: drop one client after the first window and
# prove the server releases its authority, the remaining client tears down
# its proxy presentation, and the freed roster slot admits a fresh
# reconnect rather than replaying the dead connection.
kill "${client_two_pid}" 2>/dev/null || true
wait "${client_two_pid}" 2>/dev/null || true
client_two_pid=""
sleep 3

Xvfb "${DISPLAY_THREE}" -screen 0 1920x1080x24 -nolisten tcp >"${RUN_DIR}/xvfb-three.stdout" 2>&1 &
xvfb_three_pid=$!
sleep 2

# STW_MP_COMMAND_DELAY_STEPS / STW_MP_COMMAND_LOSS_EVERY are set only on
# this one launched process, not exported into the script's own
# environment. Client one and the server above never see them, so a normal
# play session (and client one throughout this whole run) stays
# immediate-send by construction.
DISPLAY="${DISPLAY_THREE}" \
STW_MP_COMMAND_DELAY_STEPS="${GATE_DELAY_STEPS}" \
STW_MP_COMMAND_LOSS_EVERY="${GATE_LOSS_EVERY}" \
    "${BIN}/STW.GameLauncher" \
    --project-path="${PROJECT}" \
    --engine-path="${ENGINE}" \
    --project-user-path="${CLIENT_THREE_USER}" \
    --bg_ConnectToAssetProcessor=false \
    --regset=/Amazon/AzCore/Bootstrap/linux_wait_for_connect=0 \
    -rhi=null -sys_audio_disable=1 \
    -connect=127.0.0.1:"${SERVER_PORT}" \
    -log_RemoteConsolePort="${REMOTE_CONSOLE_THREE}" \
    >"${RUN_DIR}/client-three.stdout" 2>&1 &
client_three_pid=$!

sleep "${DELAY_DURATION}"
kill -0 "${server_pid}" 2>/dev/null
kill -0 "${client_one_pid}" 2>/dev/null
kill -0 "${client_three_pid}" 2>/dev/null
echo "PHASE_2_WINDOW_COMPLETE duration=${DELAY_DURATION} delay_steps=${GATE_DELAY_STEPS} loss_every=${GATE_LOSS_EVERY}"

if [[ -f "${SERVER_LOG}" ]]; then
    server_log_after="$(wc -l < "${SERVER_LOG}")"
    server_log_fingerprint_after="$(head -n 1 "${SERVER_LOG}" || true)"
    # A dedicated server often truncates Server.log on startup and then writes
    # a new file that can grow past the old line count. The first line changes
    # when that happens. Tailing from the old count then drops the new head,
    # including joins. A changed first line means this file is the whole run.
    if [[ "${server_log_fingerprint_after}" != "${server_log_fingerprint}" ]] || (( server_log_after <= server_log_before )); then
        cp "${SERVER_LOG}" "${RUN_DIR}/server.log"
    else
        tail -n +"$((server_log_before + 1))" "${SERVER_LOG}" >"${RUN_DIR}/server.log"
    fi
else
    : >"${RUN_DIR}/server.log"
fi
cp "${CLIENT_ONE_USER}/log/Game.log" "${RUN_DIR}/client-one.log"
cp "${CLIENT_TWO_USER}/log/Game.log" "${RUN_DIR}/client-two.log"
cp "${CLIENT_THREE_USER}/log/Game.log" "${RUN_DIR}/client-three.log"

count_marker() {
    local marker="$1"
    local file="$2"
    rg -c --fixed-strings "${marker}" "${file}" 2>/dev/null || true
}

require_count() {
    local label="$1"
    local marker="$2"
    local file="$3"
    local minimum="$4"
    local count
    count="$(count_marker "${marker}" "${file}")"
    echo "MARKER_COUNT_${label}=${count} REQUIRED_MIN=${minimum} FILE=${file}"
    (( count >= minimum ))
}

SERVER_CAPTURE="${RUN_DIR}/server.log"
CLIENT_ONE_CAPTURE="${RUN_DIR}/client-one.log"
CLIENT_TWO_CAPTURE="${RUN_DIR}/client-two.log"
CLIENT_THREE_CAPTURE="${RUN_DIR}/client-three.log"

require_count SERVER_INCOMING "New incoming connection from remote address" "${SERVER_CAPTURE}" 2
require_count SERVER_JOIN_RESULT "STW_MP_PLAYER_JOIN_RESULT spawned=1" "${SERVER_CAPTURE}" 2
require_count SERVER_PREFAB_COMPONENTS "STW_MP_PLAYER_PREFAB_COMPONENTS entity=" "${SERVER_CAPTURE}" 2
require_count SERVER_AUTHORITY_ROLES "role=Authority authority=1" "${SERVER_CAPTURE}" 2
require_count SERVER_AUTHORITY_SNAPSHOTS "STW_MP_AUTHORITY_SNAPSHOT_PUBLISHED" "${SERVER_CAPTURE}" 2
require_count SERVER_CONNECTION_QUALITY "quality state changed status from poor to ideal" "${SERVER_CAPTURE}" 2

for client_label_and_file in \
    "ONE|${CLIENT_ONE_CAPTURE}" \
    "TWO|${CLIENT_TWO_CAPTURE}" \
    "THREE|${CLIENT_THREE_CAPTURE}"; do
    client_label="${client_label_and_file%%|*}"
    client_file="${client_label_and_file#*|}"
    require_count "CLIENT_${client_label}_AUTONOMOUS" "role=Autonomous authority=0 autonomous=1" "${client_file}" 1
    require_count "CLIENT_${client_label}_REMOTE_ROLE" "role=Client authority=0 autonomous=0 proxy=1" "${client_file}" 1
    require_count "CLIENT_${client_label}_REMOTE_SNAPSHOT" "STW_MP_REMOTE_SNAPSHOT_RECEIVED" "${client_file}" 1
    require_count "CLIENT_${client_label}_PRESENTATION_ACTIVE" "STW_MP_REMOTE_PRESENTATION_ACTIVE" "${client_file}" 1
    require_count "CLIENT_${client_label}_PRESENTATION_RENDER_READY" "STW_MP_REMOTE_PRESENTATION_RENDER_READY" "${client_file}" 1
    if rg -q "STW_MP_REMOTE_SNAPSHOT_(REJECTED|NOT_ACCEPTED)" "${client_file}"; then
        echo "REMOTE_SNAPSHOT_REJECTION_FOUND client=${client_label}"
        exit 1
    fi
done

require_count SERVER_HIT_TRYFIRE "STW_MP_HIT_VALIDATION result=accept reason=accept source=tryfire" "${SERVER_CAPTURE}" 1
require_count SERVER_HIT_DELTA "STW_MP_HIT_DAMAGE_DELTA=16.00 source=tryfire" "${SERVER_CAPTURE}" 1
require_count SERVER_HIT_SELF "STW_MP_HIT_VALIDATION result=reject reason=self source=apply" "${SERVER_CAPTURE}" 1
require_count SERVER_HIT_SELF_UNCHANGED "STW_MP_HIT_REJECTED_self health_unchanged=1 source=apply" "${SERVER_CAPTURE}" 1
require_count SERVER_REWIND_DISPLACED "STW_MP_SERVER_REWIND displaced=1" "${SERVER_CAPTURE}" 1
if ! rg -q "STW_MP_PHYSX_REWIND displaced=1" "${CLIENT_ONE_CAPTURE}" "${CLIENT_TWO_CAPTURE}"; then
    echo "PHYSX_REWIND_DISPLACED_NOT_FOUND"
    exit 1
fi

# --- MP integrity slice 2: gated delay/loss transport + disconnect cleanup ---

# Regression guard: client one and the original client two never had
# STW_MP_COMMAND_DELAY_STEPS / STW_MP_COMMAND_LOSS_EVERY set, so their
# CreateInput path must never print a transport marker at all. A normal
# play session looks identical to client one/two here.
for regression_label_and_file in \
    "ONE|${CLIENT_ONE_CAPTURE}" \
    "TWO|${CLIENT_TWO_CAPTURE}"; do
    regression_label="${regression_label_and_file%%|*}"
    regression_file="${regression_label_and_file#*|}"
    regression_count="$(count_marker "STW_MP_COMMAND_TRANSPORT" "${regression_file}")"
    echo "MARKER_COUNT_CLIENT_${regression_label}_NO_GATE_TRANSPORT=${regression_count} REQUIRED_MAX=0 FILE=${regression_file}"
    (( regression_count == 0 ))
done

DELAY_RELEASE_MARKER="STW_MP_COMMAND_TRANSPORT sent=1 dropped=0 delay_steps=${GATE_DELAY_STEPS} loss_every=${GATE_LOSS_EVERY}"
DELAY_DROP_MARKER="STW_MP_COMMAND_TRANSPORT sent=0 dropped=1 delay_steps=${GATE_DELAY_STEPS} loss_every=${GATE_LOSS_EVERY}"
require_count CLIENT_THREE_COMMAND_HELD_RELEASED "${DELAY_RELEASE_MARKER}" "${CLIENT_THREE_CAPTURE}" 1
require_count CLIENT_THREE_COMMAND_DROPPED "${DELAY_DROP_MARKER}" "${CLIENT_THREE_CAPTURE}" 1

# Disconnect / respawn cleanup: the server must record dropping client
# two's authority, and client one (still connected throughout) must tear
# down that proxy's presentation.
if ! rg -q "STW_MP_PLAYER_LEAVE net_entity=[0-9]+ reason=[0-9]+ removed_count=[1-9][0-9]* authority_dropped=1" "${SERVER_CAPTURE}"; then
    echo "SERVER_PLAYER_LEAVE_AUTHORITY_DROP_NOT_FOUND"
    exit 1
fi
echo "MARKER_FOUND_SERVER_PLAYER_LEAVE_AUTHORITY_DROP=1 FILE=${SERVER_CAPTURE}"
require_count CLIENT_ONE_PROXY_PRESENTATION_STOPPED "STW_MP_REMOTE_PRESENTATION_STOPPED" "${CLIENT_ONE_CAPTURE}" 1

# No ghost replay: once client one drops a proxy's presentation, that exact
# local entity must never be referenced again in client one's log (no
# further remote snapshot, role, or presentation activity for the dead
# connection).
STOPPED_LINE="$(rg -n --fixed-strings "STW_MP_REMOTE_PRESENTATION_STOPPED entity=" "${CLIENT_ONE_CAPTURE}" | tail -n1)"
STOPPED_LINENO="${STOPPED_LINE%%:*}"
STOPPED_ENTITY="$(printf '%s' "${STOPPED_LINE#*:}" | sed -n 's/.*STW_MP_REMOTE_PRESENTATION_STOPPED entity=\(\[[0-9]*\]\).*/\1/p')"
if [[ -z "${STOPPED_ENTITY}" ]]; then
    echo "STOPPED_ENTITY_NOT_PARSED"
    exit 1
fi
if tail -n "+$((STOPPED_LINENO + 1))" "${CLIENT_ONE_CAPTURE}" | rg -qF "${STOPPED_ENTITY}"; then
    echo "GHOST_REPLAY_DETECTED entity=${STOPPED_ENTITY}"
    exit 1
fi
echo "NO_GHOST_REPLAY_CONFIRMED entity=${STOPPED_ENTITY} client=ONE"

# The remaining client (one) must still be Autonomous at the end of the
# run, proven against the full two-phase log (not just the phase-1
# window already checked above by the per-client loop).
require_count CLIENT_ONE_STILL_AUTONOMOUS "role=Autonomous authority=0 autonomous=1" "${CLIENT_ONE_CAPTURE}" 1

echo "RESULT=PASS"
echo "EVIDENCE_DIR=${RUN_DIR}"
