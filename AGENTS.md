# STW CANONICAL REPOSITORY CONTRACT

These instructions apply to the entire repository.

## Canonical identity

- Repository: `MarsCommanderM/shooter-game-concept`
- Repository ID: `1296772090`
- Production branch: `brauny/stw-game-production`
- Authoritative engine: O3DE 26.05.0
- Authoritative game root: `stw-o3de/`
- Authoritative gameplay Gem: `stw-o3de/Gems/STWGameplay/`

Resolve the production HEAD from the remote at the start of every task. Never
reuse an old SHA from a prompt, report, checkout, or prior conversation as the
current baseline.

## Production scope

New game implementation belongs under `stw-o3de/` and its controlled
Lightning/O3DE automation. The `STWGameplay` Gem owns native gameplay.

`nova/` is retired and must not be recreated or treated as production.

The browser/Next.js code, `server.mjs`, `stw-engine/`, and `unity-starter/`
were archived by the owner on 2026-09-18 after a dependency audit showed no
production reference (see `docs/audits/REPO_AUDIT_2026-09-18.md`). They are
preserved at tag `archive/legacy-web-prototype-20260918` and must not be
recreated. Do not use the legacy Node server as the native authoritative STW
server.

## Mandatory preflight before any mutation

1. Print the absolute Git repository top level.
2. Print the sanitized `origin` URL.
3. Print the current branch and exact HEAD.
4. Fetch `origin/brauny/stw-game-production` without changing the worktree.
5. Print `git status --porcelain=v2 --branch`.
6. Prove that the current work starts from the freshly fetched production line.
7. Run `bash tools/stw-repo-guard.sh start`.

If any identity, ancestry, upstream, branch, or worktree check is unexpected,
stop. Preserve local changes before any checkout, switch, pull, reset, restore,
rebase, merge, clean, stash, or deletion.

Do not create a branch unless the user or the active recovery/integration task
explicitly authorizes it. Never force-push or rewrite shared history.

## Recovery freeze

Issue #5 owns preservation of local Lightning work performed after 2026-09-08.
That package is expected to contain multiplayer, authoritative server, bodycam,
ragdoll, tests, and runtime evidence. Recovery is not feature development.

PR #4 remains frozen until the recovery package is safely pushed, reconciled,
and reverified. Do not merge, retarget, close, or mark it ready.

## Evidence standard

A completion claim must include exact local and remote SHAs, changed paths,
commands actually executed, exit codes, test counts, CI/check links, and every
blocked or unverified item. A prior green run does not prove a different SHA.
