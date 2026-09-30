# Aktueller Stand (gepflegt von Claude, Orchestrator)

- **Ziel:** STW von der Produktionslinie bis Release 1.0 nach
  `docs/STW_PRODUCTION_ROADMAP.md` (PR #12) bringen.
- **Coordinator:** Claude (Owner-Anweisung 2026-09-30); Owner: MarsCommanderM
- **Produktion (origin):** `brauny/stw-game-production` = `1031d580` (A-002 Telemetriemodell, T4 PASS lokal; GitHub-CI läuft)
- **Offene PRs:** keine eigenen. Issue #5 / PR #7 / #8 schließen: Owner (Auto-Mode blockiert Claude)
- **Gate-Lock:** siehe `.agents/claims/gate.lock` (fehlt = frei)

## Aktive Aufträge

| ID | Agent | Roadmap-AP | Status |
|---|---|---|---|
| A-001 | codex | P1.1 Performance-Telemetrie, Stufe 1: Designvorschlag (read-only) | DONE (Review angenommen mit Korrekturen) |
| A-002 | codex | P1.1 Stufe 2a: engine-freies Telemetriemodell + Unit-Tests | DONE (`1031d58` in Produktion) |
| A-003 | codex | P1.1 Stufe 2b: Engine-Adapter + Frame-Lücke | ASSIGNED |
| C-001 | claude | P0.3 Gate erzwingt `MAIN_MENU_ACCEPTANCE` + `DESTRUCTIBLE_ACCEPTANCE` | DONE (PR #13 gemergt) |
| C-002 | claude | P1.7 Lighting-Vorarbeit (read-only) | DONE (`reports/C-002-lighting-prior-art.md`) |
| C-000 | claude | Integration PR #9/#10/#11/#12, Hub, Roadmap-Pflege | IN_PROGRESS |

## Nächste Aktion

Codex: Review `.agents/reports/A-001-review-claude.md` und `.agents/assignments/A-002.md` lesen,
bestätigen, Claim aktualisieren. Code-Arbeit darf sofort beginnen; Build/Gate erst nach Gate-Lock-Freigabe durch Claude.
