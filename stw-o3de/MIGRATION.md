# O3DE migration status

The initial O3DE feasibility gate has been superseded. Its unmodified historical
record is preserved at
[`../docs/history/O3DE_MIGRATION_GATE_2026-08-22.md`](../docs/history/O3DE_MIGRATION_GATE_2026-08-22.md).

Current authority is explicit:

- native production root: `stw-o3de/`
- authoritative gameplay module: `stw-o3de/Gems/STWGameplay/`
- pinned engine: O3DE 26.05.0 at
  `3db6943249d8bd7960b9ed7e9aee310b7668586e`
- controlled host automation: `.github/lightning-t4/`
- retired implementation: `nova/`
- frozen reference implementations: browser/Node, `stw-engine/`, and
  `unity-starter/`

Do not use statements in the historical gate that predate the native
`STWGameplay` implementation as a description of the current production
state.

The reported weekend multiplayer/server/bodycam/ragdoll work remains outside
the known GitHub baseline and is governed by Issue #5 until recovered and
verified.
