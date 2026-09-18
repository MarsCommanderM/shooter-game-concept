# STW repository audit — 2026-09-18

Read-only findings gathered before any cleanup. Every claim below was checked
against git or the filesystem on 2026-09-18; nothing here is a guess. Actions
that the repository contract forbids without an owner decision are listed as
proposals, not performed.

## 1. Preservation state

| Item | State |
|---|---|
| `codex/stw-integration-2026-09-17` | Pushed. Remote tip equals local tip `7d0c6ee` (fast-forward from `e0e8a99`, 4 commits). |
| Weekend recovery, raw | On remote: `recovery/stw-weekend-full-2026-09-14` = `b012ae6`. |
| Weekend recovery, reconciled | On remote: `recovery/stw-weekend-reconciled-2026-09-14` = `85a775d`. |
| Phase 0 | On remote: `codex/stw-phase0-production` lineage and `backup/stw-phase0-production-20260917-c9f639e` = `c9f639e`. |
| Local backup | `recovery/backup-20260918/` next to the repo: verified bundle of all refs, patch of the uncommitted reconcile worktree, tar of 358 untracked gate-mirror files, checksums, manifest. |
| Local safety tags | `backup/20260918/*` (10 tags) pin the unreachable commits and every lineage tip. Not pushed. |

The 2026-09-17 audit stated that the integration commits existed only locally
and that no recovery refs existed on the remote. **That was wrong.** The local
clone is single-branch (`remote.origin.fetch` covers only the production
branch), so the other 31 remote branches were invisible to it.

## 2. Remote branches (32 heads)

Classified by ancestry against `brauny/stw-game-production` (`bfdc27f`) and the
integration branch.

**Fully contained in production or integration (21).** No unique commits; pure
history. Proof: `git merge-base --is-ancestor <tip> <production or integration>`.
`archive/stw-pre-cleanup-6992c804`, `codex/stw-o3de-canonicalization`,
`brauny/stw-architecture-lab`, `brauny/stw-combat-loop-v1`,
`brauny/stw-gameplay-presentation-v1`, `brauny/stw-gameplay-presentation-v1-integrated`,
`brauny/stw-mobile-look-fix-v1`, `brauny/stw-8a-t1a-verified`,
`brauny/stw-8a-t4b-visual-fix`, `codex/stw-8a-t1b`, `-t2a`, `-t2b`, `-t3a`,
`-t3b`, `-t4a`, `-t4b`, `codex/stw-playtest-v1`,
`codex/stw-playtest-mobile-deploy`, `codex/stw-playtest-v2-real-game-boot`,
`v0/brate666x-4766-51f0a3bf`, `v0/locco666-b1c90a2a`.

**Holding commits that are not in production (8).**

| Branch | Unique | Content | Note |
|---|---|---|---|
| `main` | 3 | Quarantine of the stale default branch, NOVA removal | Contract: never implement on `main`. |
| `recovery/stw-weekend-full-2026-09-14` | 20 | Raw weekend work | 18 of 20 patch-equivalent on integration; open PR #7. |
| `recovery/stw-weekend-reconciled-2026-09-14` | 22 | Reconciled weekend work | Open PR #8. |
| `codex/audit-von-block-6b-abschlieen` | 1 | `docs/PLATIN_AUDIT_2026-08-31.md` | Docs only; open PR #3. |
| `codex/stw-8a-t1-block-a` | 1 | CPU skeleton; near-duplicate of `stw-8a-t1a-verified` | Legacy `stw-engine` work. |
| `codex/nova-vf01-hdr-foundation` | 1 | NOVA HDR foundation | NOVA is retired. |
| `v0/brate666x-4766-6983af78` | 5 | v0.dev browser prototype | Legacy; open PR #1. |
| `vercel-agent/remove-tracked-node-modules` | 1 | Vercel bot fix | Legacy; open PR #2. |

Open PRs: #1, #2, #3, #4 (recovery freeze, draft), #7, #8 (draft). Issue #5
(recovery blocker) is still open.

## 3. Tracked files

Working tree is 870 MB but tracked content is small; 850 MB is ignored
generated output (`stw-o3de/Project/Cache`, `stw-o3de/Project/user`).

- **Active:** `stw-o3de/` (207 files, 16 MB), `.github/`, `tools/`, `docs/`, `runner.sh`.
- **Frozen legacy, ~250 files:** `app/`, `components/` (69), `hooks/`, `lib/`,
  `styles/`, `public/` (34, 4 MB), `deploy/`, `stw-engine/` (82), `unity-starter/`,
  `server.mjs`, and the Next.js config files. **Dependency audit:** no file under
  `stw-o3de/`, `tools/`, `.github/` or `runner.sh` references any of them; the only
  mention is prose in `stw-o3de/MIGRATION.md`.
- **Byte-identical duplicates:** `components/ui/use-toast.ts` = `hooks/use-toast.ts`,
  `components/ui/use-mobile.tsx` = `hooks/use-mobile.tsx` (legacy).
  `PAL_linux.cmake` = `PAL_windows.cmake` is an intentional platform layout.
- **Unreferenced by any tracked file or workflow:**
  `tools/assets/ensure_stw_obj_streams.py`, `.github/lightning-t4/multiplayer_gate.sh`
  (mentioned only in a spec).
- **Obsolete:** `tools/assets/create_stw_enemy_01.py` generates the placeholder box
  that `STW_ENEMY_01_RIN` replaced.
- **Overlapping planning docs:** four AI-authored files under `docs/superpowers/`
  (two "multiplayer-production-integrity", two "production-integrity"), same date.

## 4. Outside the repository (studio server)

| Path | Size | Finding |
|---|---|---|
| `stw`, `sttttw` | 23 / 24 MB | Extra clones of the same origin. Clean, no stash, no commit missing from the remote. |
| `stw-o3de-gate` | 13 GB | 236 run directories; 4 gameplay-sequence dirs are 9.6 GB. Evidence output. |
| `stw-reconcile-build` | 5.1 GB | Not referenced by `task.sh`. Build tree for the reconcile lineage. |
| `stw-o3de-production-project` | 830 MB | Not referenced by `task.sh`; stale sibling layout. |
| `stw-o3de-build`, `stw-o3de-worktree`, `o3de-2605`, `o3de-packages`, `stw-o3de-toolchain` | — | In use by `task.sh`. Keep. |
| `stw-live-session`, `Testing` | small | Recording scratch. |
| `stw-recovery-evidence-20260914`, `recovery` | small | Evidence. Keep. |

Worktrees: three clean, fully pushed scratch worktrees were removed. The
`stw-reconcile-production` worktree is kept because it holds uncommitted work
(backed up as a patch).

## 5. Contract constraints on cleanup

`docs/REPOSITORY_CONTRACT.md` forbids branch deletion and history rewrite,
allows only forensics on FROZEN paths, and says old branches stay until
equivalence or supersession is proven and archived. Issue #5 is open. So:

- Deleting the 21 contained branches, retiring PRs, and archiving the frozen
  legacy tree each need an explicit owner decision and a contract update.
- The evidence for those decisions is in sections 2 and 3.

## 6. What this audit does not establish

- Whether the weekend files that differ from integration are improvements or
  regressions; only symbol-level equivalence was checked.
- Whether unreferenced scripts are needed for manual workflows.
- The visual quality of the game: assets are procedurally generated
  blockout geometry; the gate checks only that a frame is non-empty.
