#!/usr/bin/env bash
set -Eeuo pipefail
RUN_DIR="/teamspace/studios/this_studio/stw-o3de-gate/visual-gate-20260826T085208Z"
echo "READ-ONLY LAUNCH ERROR CONTEXT"
nl -ba "${RUN_DIR}/launcher.log" | sed -n '72,122p'
echo "GAME LOG ERRORS/WARNINGS:"
grep -Ein 'error|failed|warning|assert|fatal' /teamspace/studios/this_studio/stw-o3de-worktree/stw-o3de/Project/user/log/Game.log | sed -n '1,240p' || true
echo "COST INCURRED: \$0.00"
