# Aktueller Stand (gepflegt von Claude, Orchestrator)

- **Ziel:** STW von der Produktionslinie bis Release 1.0 nach
  `docs/STW_PRODUCTION_ROADMAP.md` (PR #12) bringen.
- **Coordinator:** Claude (Owner-Anweisung 2026-09-30); Owner: MarsCommanderM
- **Produktion (origin):** `brauny/stw-game-production` = `b0c39841` (seit 2026-09-30 18:27 UTC; PR #9/#10/#11 gemergt)
- **Offene PRs:** #12 Roadmap, #13 C-001 (Gate erzwingt Menü/Destructible, lokal T4 PASS auf `2be2f14`)
- **Gate-Lock:** siehe `.agents/claims/gate.lock`

## Aktive Aufträge

| ID | Agent | Roadmap-AP | Status |
|---|---|---|---|
| A-001 | codex | P1.1 Performance-Telemetrie, Stufe 1: Designvorschlag (read-only) | ASSIGNED |
| C-001 | claude | P0.3 Gate erzwingt `MAIN_MENU_ACCEPTANCE` + `DESTRUCTIBLE_ACCEPTANCE` | REVIEW (PR #13, T4 PASS `2be2f14`) |
| C-000 | claude | Integration PR #9/#10/#11/#12, Hub, Roadmap-Pflege | IN_PROGRESS |

## Nächste Aktion

Codex: `.agents/HUB.md` und `.agents/assignments/A-001.md` lesen, mit
`status`-Nachricht bestätigen, dann A-001 ausführen.
