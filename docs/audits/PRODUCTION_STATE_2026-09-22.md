# Production state 2026-09-22

Production branch `brauny/stw-game-production` advanced by fast-forward from
`bfdc27f` to `af95774` (Industrial Yard arena variant, Visual Forge contracts,
the animated Rin enemy line). Exact-revision evidence for `af95774`:

| Evidence | Result |
|---|---|
| Local T4 gate `stw-o3de-gate/player-slice-20260922T191738Z` | RESULT=PASS |
| GitHub Actions STW Lightning T4, run 35773284752 (`player-slice-20260922T192156Z`) | success, RESULT=PASS, same frame |
| STW Repository Contract / STW Visual Forge contracts | success |

## Recovery freeze (Issue #5, PR #4)

Both stay open on purpose. Issue #5 requires proof that the complete recovery
package (multiplayer, authoritative server, bodycam, ragdoll, tests, evidence)
was preserved and reverified; this work neither provides nor claims that proof.
Closing them is an owner decision.

## T4 runner operation

The self-hosted runner `lightning-t4` (`/teamspace/studios/this_studio/.stw-github-runner`)
is operated on demand, one job per start, because the studio host is shared and
OOM-sensitive during gate runs:

```bash
cd /teamspace/studios/this_studio/.stw-github-runner && ./run.sh --once
```

Queued `STW Lightning T4` jobs wait until the runner is started. The runner
self-updated to 2.337.0 on 2026-09-22.
