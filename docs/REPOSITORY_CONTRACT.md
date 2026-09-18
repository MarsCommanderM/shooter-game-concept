# STW repository contract

## Single source of truth

| Field | Required value |
|---|---|
| Repository | `MarsCommanderM/shooter-game-concept` |
| Repository ID | `1296772090` |
| Production branch | `brauny/stw-game-production` |
| Active engine root | `stw-o3de/` |
| Gameplay owner | `stw-o3de/Gems/STWGameplay/` |
| Engine pin | O3DE 26.05.0 / `3db6943249d8bd7960b9ed7e9aee310b7668586e` |

A task starts from the freshly fetched production HEAD. An old SHA is evidence
of a past state, never permission to use a stale base.

## Path states

| State | Paths | Allowed work |
|---|---|---|
| ACTIVE | `stw-o3de/**`, `.github/lightning-t4/**`, native asset tools | Explicit STW production tasks |
| RETIRED | `nova/**` | None; archive inspection only |
| ARCHIVED | browser/Next.js, `server.mjs`, `stw-engine/**`, `unity-starter/**` (removed 2026-09-18; tag `archive/legacy-web-prototype-20260918`) | None; must not be recreated |
| RECOVERY | local Lightning work identified by Issue #5 | Preservation first; no integration until reviewed |

## Branch rules

- Never implement on stale `main`.
- Direct production work requires an explicit task and an exact fresh baseline.
- A temporary Codex or recovery branch must contain the freshly fetched
  production HEAD when created.
- Unexpected dirt, detached HEAD, unrelated history, missing upstream, or a
  changed remote is a stop condition.
- No force push, reset of shared refs, history rewrite, or branch deletion.
- Old branches remain until equivalence or supersession is proven.

## Integration rules

1. Preserve the source state on a remote recovery branch.
2. Compare it with the fresh production HEAD.
3. Classify every changed path as active, legacy, generated, secret, or
   unrelated.
4. Integrate only the coherent native package.
5. Run deterministic tests, native targets, launcher, hardware renderer,
   multiplayer/server, ragdoll, and runtime acceptance appropriate to the diff.
6. Bind all evidence to the exact candidate SHA.
7. Merge only after required checks are green.

## Repository settings still required

After recovery and test reconciliation:

- make the canonical trunk the GitHub default branch
- protect it against force pushes and deletion
- require pull requests and exact STW status checks
- require conversation resolution
- block stale approvals after new commits
- retire old PRs and branches only after archival proof
