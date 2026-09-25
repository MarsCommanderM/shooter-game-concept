# STW CANONICAL REPOSITORY CONTRACT

> ## PRODUCTION QUALITY BAR: REALISTIC CINEMATIC. NOT BLOCKOUT. NOT A PROTOTYPE.
>
> STW's target presentation is a hyper-realistic, cinematic AAA native 3D
> multiplayer FPS. Over 100 prototypes of this game already exist. This is
> not another one. Never propose, accept, describe, or ship blockout-tier,
> placeholder-tier, greybox, or "tech demo"-tier visual or gameplay quality
> as a finished result, in any folder, on any branch. Temporary blockouts
> are a development tool, never the target. Read
> [`docs/STW_PRODUCT_VISION.md`](docs/STW_PRODUCT_VISION.md) before any
> architecture, asset, level, rendering, or cleanup decision. This applies
> to every agent, every contributor, every session — no exceptions, and no
> "just this once."

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

## Product identity

STW is a high-end native 3D multiplayer FPS with a hyper-realistic cinematic
sci-fi target, not a blockout project or tech demo. Read
`docs/STW_PRODUCT_VISION.md` before architecture, asset or cleanup work. Never
lower the product target to match the implementation; report the gap instead.
Classify visual content as `TEMPORARY_DEBUG`, `BLOCKOUT`, `TECHNICAL_VALIDATION`,
`PRODUCTION_CANDIDATE` or `PRODUCTION_READY`, and never call placeholder content
final or production-ready without visual and runtime evidence.

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

For visual production work, follow `stw-o3de/Docs/README.md` (Project Visual
Forge). Before substantial asset or level edits, record affected files and
expected performance impact in a task report. Visual choices must be reproducible
from versioned sources, import settings and presets. Passing tooling CI does not
approve assets: preserve explicit classifications, validate asset evidence, and
require exact-revision visual reviews and measured performance before promotion.


A completion claim must include exact local and remote SHAs, changed paths,
commands actually executed, exit codes, test counts, CI/check links, and every
blocked or unverified item. A prior green run does not prove a different SHA.
