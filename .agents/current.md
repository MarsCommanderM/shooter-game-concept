# Aktueller Stand (gepflegt von Grok, Orchestrator auf Zeit)

**Hub ist aktiv.** Letztes Update: 2026-10-01T21:21Z. Claude ist offline. Der Owner hat Grok auf den Orchestrator-Sitz gesetzt, bis er Claude zurücksetzt. Codex bleibt Engineer. Dieselben Regeln aus `HUB.md`. Nachrichten von Grok kommen als Agent `grok`.

## Arbeitsweise ab jetzt (ein AP, eine SHA, ein Beweis)

1. Es läuft immer nur das oberste offene Arbeitspaket der niedrigsten offenen Phase, dessen Abhängigkeiten erfüllt sind. Heute ist das **P1.1 / A-003**. A-004 bleibt QUEUED.
2. Ein Beweis gilt nur für die exakte SHA, die im Gate-Log als `SOURCE_COMMIT` steht. Ein PASS auf einer älteren SHA zählt für den neuen Commit nicht.
3. `done` enthält SHA, Branch, Pfade, Befehle mit Exit-Codes, Testzahl, Evidenzpfad und alles, was nicht verifiziert ist. Der Orchestrator reviewt den Diff gegen die Abnahme des Auftrags. Lücken werden ein Nachzieh-Commit auf demselben Auftrag, kein neues AP.
4. Integration ist eine eigene SHA: Produktions-HEAD plus bereits bewiesene, noch nicht gepushte Fixes, die CI braucht, plus das reviewte AP. Ein T4 auf dieser kombinierten SHA, Roadmap-Status im selben PR. Push nach Produktion nur Orchestrator oder Owner.
5. Eine T4. Wer den Lauf startet, hält `.agents/claims/gate.lock`, bis der Prozess weg ist, und postet danach das Ergebnis. Kein zweiter Build, kein `task.sh`, kein Launcher daneben.

## A-003, Stand 21:21Z

- Branch `codex/stw-perf-telemetry-adapter-20260930`, Worktree `stw-perf-telemetry-adapter-20260930`. Drei Commits über Produktion `db369f9`: `0fadded`, `e02fab3`, `4804029`. Enthält C-003 (`2bb5a99`) nicht.
- `e02fab3`: Player-Slice `stw-o3de-gate/player-slice-20261001T210924Z` und Multiplayer `stw-o3de-gate/multiplayer-20261001T211413Z-417568`, beide `RESULT=PASS`. `PERFORMANCE_TELEMETRY_CSV=PASS samples=1989 duration_s=60.002 rows=2954`. Dieselbe Report-Zeile `PERFORMANCE_BASELINE` trägt weiter `cpu_frame_ms=UNAVAILABLE gpu_frame_ms=UNAVAILABLE`, weil `RecordPerformance` das literal druckt (`STWGameplaySystemComponent.cpp` Z. 1820). Damit ist A-003 Punkt 5 auf dieser SHA offen.
- `e02fab3` ändert `.github/lightning-t4/task.sh` (+55). Das bleibt für dieses AP im Review. Weitere Änderungen an `task.sh` nur über eine `dependency`.
- **Jetzt:** Codex hält `gate.lock` seit 21:18Z für den vollen T4 auf `4804029`. `task.sh` läuft (PIDs 428125, 428155). Diesen Lauf zu Ende führen. Keine Commits, kein Rebase, kein A-004, kein zweiter Build, solange der Lock liegt.

## Nächster Schritt nach dem Lauf

Codex postet `status`: SHA `4804029`, `RESULT`, Evidenzpfad, ob `PERFORMANCE_BASELINE` echte cpu/gpu-Werte hat, ob `PERFORMANCE_FRAME_GAP` und `PERFORMANCE_TELEMETRY_CSV=PASS` im Log stehen, Exit-Codes. Wenn der Lauf PASS ist und die Baseline weiter UNAVAILABLE druckt: ein Commit, nur diese Printf-Zeile, nichts in `UpdateAutomatedAcceptance`. Danach ein neuer Lock und ein T4 auf der neuen SHA. Wenn der Lauf FAIL ist: Ursache mit Logpfad posten, nicht denselben Lauf wiederholen.

C-003 bleibt außerhalb dieses Branches. Vor jedem Produktions-Push kommt eine Integrations-SHA aus `db369f9` + C-003 + reviewtem A-003, einmal gegated. Der 3×-PASS von C-003 ist Claudes Bericht, von Grok in diesem Turn nicht neu gemessen.

## Aufträge

| ID | Agent | Inhalt | Status |
|---|---|---|---|
| A-003 | codex | P1.1 Adapter | IN_ARBEIT, Gate auf `4804029` läuft |
| A-004 | codex | P1.2 First-Person-Arme | QUEUED bis A-003 `done` und reviewt |
| C-003 | claude | Aim-Fix, lokal 3× PASS laut Claude, nicht in Produktion | wartet auf Integrations-SHA |
| C-000 | grok | Orchestration | IN_ARBEIT |

P0.5 und P0.6 bleiben Owner-Entscheidungen. Daran arbeiten wir nicht.
