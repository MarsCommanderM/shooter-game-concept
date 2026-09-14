# STW REPOSITORY SAFETY GATE

## STOP: this branch is not the current production line

This repository is the correct repository, but the default `main` branch is not
the current STW production baseline. Do not implement, refactor, build, merge,
or delete anything from this branch.

## Canonical identity

- GitHub repository: `MarsCommanderM/shooter-game-concept`
- GitHub repository ID: `1296772090`
- Current production branch: `brauny/stw-game-production`
- Authoritative native game path: `stw-o3de/`
- Authoritative gameplay Gem: `stw-o3de/Gems/STWGameplay/`

Never hardcode an old production SHA. Fetch the remote and resolve
`origin/brauny/stw-game-production` at the start of every task.

## Legacy paths

The following are historical prototypes or superseded implementations and are
not valid production targets unless the user explicitly authorizes archival or
forensic work:

- `nova/`
- the Next.js/browser game under `app/`, `components/`, and `server.mjs`
- `unity-starter/`

Do not port new gameplay into these paths. Do not describe NOVA as the current
production engine.

Treat `stw-engine/` as migration/reference material until its remaining
dependencies are explicitly audited. Do not extend it and do not delete it
without a verified archive and dependency report.

## Mandatory pre-mutation evidence

Before changing any file:

1. Print the absolute repository top level.
2. Print a sanitized `origin` URL.
3. Print the current branch and exact HEAD.
4. Fetch `origin/brauny/stw-game-production` without changing the worktree.
5. Print `git status --porcelain=v2 --branch`.
6. Prove whether the current HEAD contains the freshly fetched production HEAD.

If the repository, branch, ancestry, worktree, or upstream is unexpected, stop.
Preserve all local work before any checkout, switch, pull, reset, restore,
rebase, merge, clean, or stash operation.

## Active recovery freeze

Issue #5 is the only authorized operation for any Lightning worktree that may
contain work performed after 2026-09-08:

https://github.com/MarsCommanderM/shooter-game-concept/issues/5

PR #4 is intentionally frozen as a draft. Do not merge, retarget, close, rebase,
or force-push it.

## Completion standard

Claims are not evidence. A completion report must include exact local and
remote SHAs, changed paths, executed test commands, exit codes, CI/check URLs,
and any unverified or blocked item. Never report green for a test that was not
run against the reported commit.
