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
REMOTE_CONSOLE_SERVER="$((47000 + ($$ % 500)))"
REMOTE_CONSOLE_ONE="$((47500 + ($$ % 500)))"
REMOTE_CONSOLE_TWO="$((48000 + ($$ % 500)))"
DURATION="${STW_MP_DURATION:-30}"
XDG_RUNTIME_DIR="${RUN_DIR}/xdg-runtime"
CLIENT_ONE_USER="${RUN_DIR}/client-one-user"
CLIENT_TWO_USER="${RUN_DIR}/client-two-user"

server_pid=""
client_one_pid=""
client_two_pid=""
xvfb_one_pid=""
xvfb_two_pid=""
server_log_before=0

mkdir -p "${RUN_DIR}" "${XDG_RUNTIME_DIR}" "${CLIENT_ONE_USER}" "${CLIENT_TWO_USER}"
chmod 700 "${XDG_RUNTIME_DIR}" "${CLIENT_ONE_USER}" "${CLIENT_TWO_USER}"
exec > >(tee "${RUN_DIR}/report.log") 2>&1

cleanup() {
    set +e
    for pid in "${client_one_pid}" "${client_two_pid}" "${server_pid}" "${xvfb_one_pid}" "${xvfb_two_pid}"; do
        [[ -n "${pid}" ]] && kill "${pid}" 2>/dev/null || true
    done
    wait "${client_one_pid}" "${client_two_pid}" "${server_pid}" "${xvfb_one_pid}" "${xvfb_two_pid}" 2>/dev/null || true
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

require_count SERVER_INCOMING "New incoming connection from remote address" "${SERVER_CAPTURE}" 2
require_count SERVER_JOIN_RESULT "STW_MP_PLAYER_JOIN_RESULT spawned=1" "${SERVER_CAPTURE}" 2
require_count SERVER_PREFAB_COMPONENTS "STW_MP_PLAYER_PREFAB_COMPONENTS entity=" "${SERVER_CAPTURE}" 2
require_count SERVER_AUTHORITY_ROLES "role=Authority authority=1" "${SERVER_CAPTURE}" 2
require_count SERVER_AUTHORITY_SNAPSHOTS "STW_MP_AUTHORITY_SNAPSHOT_PUBLISHED" "${SERVER_CAPTURE}" 2
require_count SERVER_CONNECTION_QUALITY "quality state changed status from poor to ideal" "${SERVER_CAPTURE}" 2

for client_label_and_file in \
    "ONE|${CLIENT_ONE_CAPTURE}" \
    "TWO|${CLIENT_TWO_CAPTURE}"; do
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
if ! rg -q "STW_MP_PHYSX_REWIND displaced=1" "${CLIENT_ONE_CAPTURE}" "${CLIENT_TWO_CAPTURE}"; then
    echo "PHYSX_REWIND_DISPLACED_NOT_FOUND"
    exit 1
fi
echo "RESULT=PASS"
echo "EVIDENCE_DIR=${RUN_DIR}"
