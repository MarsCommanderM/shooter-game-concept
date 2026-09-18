# STW deployment gate

Production deployment is frozen while Issue #5 recovers and reconciles the
local Lightning multiplayer/server/bodycam/ragdoll package.

## Current controlled path

The tracked native verification entry point is:

- workflow: `.github/workflows/stw-lightning-t4.yml`
- task: `.github/lightning-t4/task.sh`
- source mirror: `stw-o3de/Gems/STWGameplay/`
- asset mirror: `stw-o3de/Project/Assets/`
- host: labeled self-hosted Lightning runner

The workflow must check out the exact triggering commit. Evidence from a
different SHA is not transferable.

## Server boundary

The former root `server.mjs` belonged to the archived browser prototype
(tag `archive/legacy-web-prototype-20260918`). It is not the authoritative
native STW dedicated server and must not be restored, deployed, or extended as
one.

The recovered native server path is accepted only after its exact branch and
SHA are known and the following are proven:

1. factual O3DE client/server target separation
2. authoritative state ownership and validation
3. at least two independent clients through the server
4. disconnect/reconnect behavior
5. headless server startup without presentation dependencies
6. deterministic tests and runtime evidence tied to the same commit
7. no hardcoded secrets or deployment credentials

The previous web deployment notes are preserved at
[`docs/history/DEPLOY_WEB_PROTOTYPE.md`](docs/history/DEPLOY_WEB_PROTOTYPE.md).
