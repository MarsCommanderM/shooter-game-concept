# Review A-001 (Claude, Orchestrator), 2026-09-30

**Urteil: angenommen mit Korrekturen.** Stufe 2 wird als A-002 freigegeben.

## Stichprobe gegen Quellen (alle bestätigt)

| Behauptung | Beleg |
|---|---|
| `RHISystemInterface::GetCpuFrameTime()` | `o3de-2605/Gems/Atom/RHI/Code/Include/Atom/RHI/RHISystemInterface.h:69` |
| `PassSystemFrameStatistics::m_totalDrawItemsRendered` | `.../RPI.Public/Pass/PassSystemInterface.h:59`, `GetFrameStatistics()` Z. 141 |
| `RHIMemoryStatisticsInterface::GetMemoryStatistics()` | `.../Atom/RHI/RHIMemoryStatisticsInterface.h:47` |
| `ProcessMemInfo`, `QueryMemInfo()` | `Code/Framework/AzCore/AzCore/Process/ProcessInfo.h:23,33` |
| `Pass::GetLatestTimestampResult()` | `.../RPI.Public/Pass/Pass.h:263` |
| CSV-Spalten | `stw-o3de/Tools/visual_forge/forge.py:259`, identisch mit dem Vorschlag |
| bestehender Profiler | `STWGameplaySystemComponent.cpp:1661` (`CLOCK_THREAD_CPUTIME_ID`), `:1682` `RecordPerformanceProfile` |

## Korrekturen (verbindlich für A-002)

1. **Drei Läufe = dieselbe SHA.** `forge.py:237` verlangt eine einzige `revision`,
   `:253` eindeutige Daten (`sha256`) je Lauf. Nicht „unterschiedliche Commit-SHAs“.
2. **RAM-Peak:** `ProcessMemInfo::m_peakWorkingSet` existiert (`ProcessInfo.h:24`); nutzen.
3. **`draw_calls` darf nie 0 sein:** `forge.py:268` verlangt alle Werte > 0. Kein
   Sonderfall für leere Frames. Semantik = Draw Items (`m_totalDrawItemsRendered`),
   so im Evidence-Contract dokumentieren.
4. **Header-Ort:** Client-Adapter unter `Code/Source/Clients/` (wie `PhysXArenaRuntime.h`),
   nicht `Include/STWGameplay/Clients/`. Das Modell unter `Include/STWGameplay/`.
5. **CPU-Spalte:** `cpu_ms` bleibt die bestehende `MainThreadCpuTime`-Messung
   (Kontinuität zu allen bisherigen Evidenzen). `GetCpuFrameTime()` nur als
   zusätzlicher Diagnosewert im Log-Marker, nicht in der CSV.
6. **Budgetfrage:** Frame p95 37,6 ms ist **kein** akzeptierter Zustand. Die
   Vision verbietet, das Ziel abzusenken. Die T4 ist aber CI-Messmaschine,
   nicht Release-Zielhardware (Owner-Entscheidung E2). Folge: A-002 blockiert
   nicht an der Budgetzahl, muss die Lücke aber **erklären**.

## Lücke

Punkt 1b (Ursache Frame 37,6 ms vs. CPU 10,7 / GPU 12,0 ms) fehlt. Er wird Teil
von A-002 (Diagnosewerte), nicht vergessen.

## Scope-Befund (wichtig)

`forge.py:249` verlangt `connected_players >= 8`. Eine vollständige Forge-Abnahme
ist erst mit Roadmap P1.14 (8-Spieler) möglich. A-002 liefert gültige
Einzel-Run-CSVs, die die **per-Run-Prüfungen** von forge bestehen (Spalten,
>0, Warmup/Fenster, ≥600 Samples, Frame-Summe ±5 %). Die volle Forge-Abnahme
bleibt ausdrücklich offen.
