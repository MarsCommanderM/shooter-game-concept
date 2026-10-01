# Aktueller Stand (gepflegt von Claude, Orchestrator)

**Hub ist aktiv.** Letztes Update: 2026-10-01T17:14Z. Falls das hier alt aussieht: `git -C stw-agent-hub pull origin codex/stw-agent-hub` — dieses File liegt auf dem Branch, nicht nur lokal.

- **Ziel:** STW von der Produktionslinie bis Release 1.0 nach
  `docs/STW_PRODUCTION_ROADMAP.md` bringen.
- **Coordinator:** Claude (Owner-Anweisung 2026-09-30); Owner: MarsCommanderM
- **Produktion (origin, `brauny/stw-game-production`):** `db369f9` (Stand 2026-09-30 18:30 UTC). NICHT mehr `1031d58` — das ist veraltet.
- **Hängender Fast-Forward:** C-003 v2 (`2bb5a99`, deterministisches Primary-Enemy-Aiming, fixt den CI-Red-Grund von 1031d58) ist 3x konsekutiv T4-Gate `RESULT=PASS` bewiesen (Läufe `multiplayer-20261001T163417Z-138786`, `-164209Z-149594`, `-164918Z-159876`), aber der Push nach Produktion ist vom Claude-Code-Auto-Mode-Classifier blockiert (`[Production Deploy]`), zweimal versucht. Owner muss selbst pushen (`! git push ...`) oder eine Permission-Regel setzen.
- **PR #14** (dokumentiert das Schließen von Issue #5 / PR #4, siehe unten): offen, CI grün bis auf den bekannten irrelevanten Vercel-Check, Merge ebenfalls vom Classifier blockiert.
- **Issue #5 / PR #4:** beide am 2026-10-01 geschlossen (Owner-Freigabe). Begründung direkt gegen Code geprüft: Ragdoll in Produktion und beiden Recovery-Branches identischer Stub; Produktion hat beim MP-Code 111 vs. 91 Dateien inkl. `MatchSession.h`, das der Recovery-Branch fehlt.
- **Gate-Lock:** `.agents/claims/gate.lock` (fehlt = frei, aktuell frei)

## Aktive Aufträge

| ID | Agent | Roadmap-AP | Status |
|---|---|---|---|
| A-001 | codex | P1.1 Performance-Telemetrie, Stufe 1: Designvorschlag (read-only) | DONE |
| A-002 | codex | P1.1 Stufe 2a: engine-freies Telemetriemodell + Unit-Tests | DONE (`1031d58`, in Produktion) |
| A-003 | codex | P1.1 Stufe 2b: Engine-Adapter + Frame-Lücke | **IN_ARBEIT — siehe Befund unten** |
| A-004 | codex | P1.2 First-Person-Arme für alle 10 Waffenprofile | QUEUED (nach A-003), siehe `.agents/assignments/A-004.md` |
| C-003 | claude | CI-Red-Fix (nicht-deterministisches Zielen in der Akzeptanz) | 3x T4 PASS bewiesen, Push blockiert (siehe oben) |
| C-001 | claude | P0.3 Gate erzwingt `MAIN_MENU_ACCEPTANCE` + `DESTRUCTIBLE_ACCEPTANCE` | DONE (PR #13 gemergt) |
| C-002 | claude | P1.7 Lighting-Vorarbeit (read-only) | DONE |
| C-000 | claude | Integration, Hub, Roadmap-Pflege, Freeze-Closure | IN_ARBEIT |

## A-003 — Review-Befund 2026-10-01T17:11Z (für Codex)

Euer uncommitteter Adapter (`STWPerformanceTelemetryAdapter.cpp/.h` + Tests,
Worktree `/teamspace/studios/this_studio/stw-perf-telemetry-adapter-20260930`,
Branch `codex/stw-perf-telemetry-adapter-20260930`) hatte 2 echte Compile-Fehler:
`passSystem` fälschlich `const AZ::RPI::PassSystemInterface*` deklariert (Z. 88),
aber `GetRootPass()`/`GetFrameStatistics()` sind nicht-const. Claude hat das
**minimal gefixt** (nur `const` entfernt, keine Logikänderung), **uncommitted**
im selben Worktree — euer Claim, nicht gepusht.

Danach: Build clean, Unit-Test PASS. Voller T4-Gate-Lauf
(`stw-o3de-gate/player-slice-20261001T165828Z`) endet aber mit
`TASK_EXIT_CODE=1`: `WEAPON_SWITCH_ACCEPTANCE`/`LOADOUT_ACCEPTANCE` noch PASS um
17:04:09, aber `PHYSX`/`VIEWMODEL`/`COMBAT_FEEDBACK`/`BLOCK_22`/`BODYCAM`/`ARENA`/
`MAIN_MENU`/`DESTRUCTIBLE`/`PERFORMANCE_PROFILE` fehlen komplett — sieht nach
Absturz/Hang des GameLaunchers mitten im Lauf aus, zeitlich nah am
Performance-Messfenster (passt zum neuen Adapter-Code: `AZ::RPI::PassSystemInterface`
/ `PassTimestamp`-Zugriff). `report.log` bricht bei `STW DIAGNOSTIC DUMP END` ab,
kein `RESULT=`, kein `stw-player-slice.png` (nur `.ppm`). Nicht weiter von Claude
untersucht — Adapter-interne Logik ist euer Claim.

**Evidenz:** `stw-o3de-gate/player-slice-20261001T165828Z/{report.log,launcher.log}`

## Nächste Aktion

Codex: A-003 — Absturzursache im Adapter finden (vermutlich `RecordFrame`/Pass-Timestamp-Pfad),
fixen, committen. Build/CTest sofort erlaubt; voller T4-Gate erst nach Lock-Freigabe (aktuell frei).
