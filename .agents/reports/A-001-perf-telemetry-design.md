# A-001 — Performance-Telemetry, Stufe 1: Design

**Status:** read-only abgeschlossen  
**Branch:** `codex/stw-agent-hub`  
**Auftrag:** P1.1 Performance Telemetry Stage 1  
**Bearbeiter:** Codex  
**Datum:** 2026-09-30

## Ergebnis

Die bestehende Performance-Messung liefert bereits belastbare CPU-/GPU-Zeitreihen, aber noch keinen vollständigen, maschinenlesbaren Run-Nachweis für die Visual-Forge-Verifikation. Die empfohlene Stufe 2 ist deshalb ein opt-in Telemetriepfad mit einem kleinen engine-unabhängigen Modell und einem dünnen Client-Adapter.

Der Adapter soll pro Sample genau diese CSV-Spalten schreiben:

```text
time_s,frame_ms,cpu_ms,gpu_ms,draw_calls,vram_mib,ram_mib
```

Die bestehende `PERFORMANCE_PROFILE`-Zeile bleibt als rückwärtskompatibler Kurzmarker erhalten. Der neue CSV-Export ist die kanonische Quelle für drei getrennte Runs und `forge.py`.

## Bestand und Befunde

### Bereits vorhanden

In `stw-o3de/Gems/STWGameplay/Code/Source/Clients/STWGameplaySystemComponent.cpp`:

- `RecordPerformanceProfile()` nutzt 30 s Warmup und 60 s Messfenster.
- Frame-Zeit wird aus `deltaTime` gesammelt.
- CPU-Zeit wird aktuell über `CLOCK_THREAD_CPUTIME_ID` als Main-Thread-CPU-Zeit gemessen.
- GPU-Zeit wird als frühester bis spätester gültiger Pass-Timestamp aggregiert; verschachtelte Child-Passes werden rekursiv berücksichtigt.
- Am Ende werden p50/p95/p99 für Frame/CPU/GPU ausgegeben.
- Der Marker enthält bereits `cpu_source=MainThreadCpuTime`, `gpu_source=PassTimestampExtent` und die Auflösung.

Beispiel aus dem vorhandenen Gate-Evidence:

`stw-o3de-gate/player-slice-20260925T194016Z/performance-profile.log`

```text
PERFORMANCE_PROFILE warmup_s=30 window_s=60.019 samples=1907 frame_sum_s=60.019 frame_ms=31.269/36.168/39.533 cpu_ms=8.229/10.312/10.903 gpu_ms=11.193/11.814/12.093 cpu_samples=1907 gpu_samples=1907 cpu_source=MainThreadCpuTime gpu_source=PassTimestampExtent resolution=1920x1080
```

Das bestätigt: CPU und GPU werden gemessen; der Engpass ist aktuell die hohe Frame-p95 gegenüber dem Budget, nicht das Fehlen dieser beiden Messwerte.

### O3DE-26.05-APIs

Die geprüften nativen Schnittstellen sind:

| Messgröße | Empfohlene Quelle | Einschränkung |
|---|---|---|
| Frame-Zeit | bestehendes `deltaTime` beziehungsweise Scheduler-Sample | Sample-Zeitpunkt muss im CSV monoton sein |
| CPU | `AZ::RHI::RHISystemInterface::Get()->GetCpuFrameTime()` als RHI-native Alternative; bestehender Main-Thread-Counter bleibt für Kompatibilität möglich | CPU-Framezeit ist nicht identisch mit gesamter Prozesszeit |
| GPU | Root-Pass `GetLatestTimestampResult()`; alternativ dieselbe Pass-Extent-Logik wie `GpuPassProfiler` | Timestamp-Ergebnis kann frameversetzt sein; die ersten Query-Ergebnisse können ungültig sein |
| Draws | `AZ::RPI::PassSystemInterface::Get()->GetFrameStatistics().m_totalDrawItemsRendered` | O3DE nennt dies Draw Items, nicht garantiert wörtliche Treiber-Draw-Calls; Indirect/Multi-Draw kann abweichen |
| VRAM | `AZ::RHI::RHIMemoryStatisticsInterface::Get()->GetMemoryStatistics()`; Device-Heaps/Pools aus `MemoryUsage` aggregieren | Memory-Statistiken können teuer sein und sollen nur opt-in aktiviert werden |
| RAM | `AZ::QueryMemInfo()` und `ProcessMemInfo::m_workingSet` | `workingSet` ist der aktuelle Prozesswert; Peak separat als Run-Maximum erfassen |

Wichtige O3DE-Referenzen:

- `Gems/Atom/RHI/Code/Include/Atom/RHI/RHISystemInterface.h` und `FrameScheduler.h`: CPU-Framezeit.
- `Gems/Atom/RPI/Code/Include/Atom/RPI.Public/Pass/Pass.h`, `GpuQuery/GpuQueryTypes.h` und `GpuPassProfiler.h`: Pass-Timestamps und GPU-Extent.
- `Gems/Atom/RPI/Code/Include/Atom/RPI.Public/Pass/PassSystemInterface.h`: Frame-Statistik und Draw-Item-Zähler.
- `Gems/Atom/RHI/Code/Include/Atom/RHI/RHIMemoryStatisticsInterface.h`, `RHI.Reflect/MemoryStatistics.h` und `MemoryUsage.h`: VRAM-Heaps/Pools und Budgets.
- `Code/Framework/AzCore/AzCore/Process/ProcessInfo.h`: plattformübergreifende Prozessspeichernutzung.

## Implementierungsentwurf für Stufe 2

### 1. Reines Datenmodell

Neue, testbare Komponente ohne RHI-/RPI-Abhängigkeit:

```text
Code/Include/STWGameplay/PerformanceTelemetryModel.h
Code/Source/PerformanceTelemetryModel.cpp
```

Das Modell enthält `TelemetrySample` mit den sieben CSV-Feldern, nimmt Samples in Zeitreihenfolge an und berechnet/validiert:

- monotone Zeitstempel,
- Warmup-/Messfenster und effektive Sample-Anzahl,
- Frame-Summe gegen Fensterdauer,
- p50/p95/p99 für Frame/CPU/GPU,
- Draw-/VRAM-/RAM-Peaks,
- fehlende, nichtfinite oder nichtpositive Messwerte.

Die p95/p99-Berechnung soll dieselbe nearest-rank-Definition wie der bestehende Profilmarker und `forge.py` verwenden. Das Modell kennt weder Environment noch Dateisystem.

### 2. Dünner Client-Adapter

Vorgeschlagene Dateien:

```text
Code/Include/STWGameplay/Clients/STWPerformanceTelemetryAdapter.h
Code/Source/Clients/STWPerformanceTelemetryAdapter.cpp
```

Der Adapter wird ausschließlich im Clientpfad gebaut beziehungsweise mit dem bestehenden `AZ_TRAIT_SERVER`-Split compile-safe gehalten. Pro Tick sammelt er die Quellen aus der Tabelle und übergibt einen Sample an das Modell. Die Ausgabe erfolgt gepuffert in eine Run-spezifische CSV-Datei.

Aktivierung ausschließlich über `STW_PERF_TELEMETRY=1` (und einen expliziten Ausgabe-Pfad). Ohne diese Variable darf der normale Spielpfad keine Timestamp-Queries, Memory-Statistik-Abfragen, Sample-Vektoren oder Datei-I/O starten.

### 3. CSV- und Gate-Vertrag

- Header exakt: `time_s,frame_ms,cpu_ms,gpu_ms,draw_calls,vram_mib,ram_mib`.
- Zeit beginnt beim ersten Telemetrie-Tick; Warmup-Zeilen dürfen geschrieben, aber bei der Auswertung nicht gezählt werden.
- Nach dem Warmup mindestens 600 gültige Zeilen und mindestens 60 s Dauer.
- Alle Messwerte müssen endlich und größer als 0 sein; `draw_calls` darf bei einem tatsächlich leeren Frame nur dann 0 sein, wenn der Validator diesen Fall explizit erlaubt. Für den bestehenden Contract sollte die Semantik vor Implementierung festgelegt werden.
- Pro Gate-Run ein eigener Artefaktname; mindestens drei Runs mit unterschiedlichen Commit-SHAs beziehungsweise eindeutigem Run-Kontext.
- Nach dem Schreiben den vorhandenen `Tools/visual_forge/forge.py` verwenden; keine zweite, abweichende Budgetlogik im Runtime-Code.
- Logmarker mit Run-ID, CSV-Pfad, Sample-Anzahl, Warmup-/Fensterdauer und Quellen ausgeben.

### 4. Tests

Unit-Tests für das reine Modell:

1. gültige monotone Samples und nearest-rank p50/p95/p99;
2. Zeitlücken, rückwärts laufende Zeit und zu kurze Fenster;
3. `NaN`, `inf`, negative und fehlende Werte;
4. Draw-/VRAM-/RAM-Peak-Aggregation;
5. exakte CSV-Spaltenreihenfolge und Dezimalformat.

Adaptertests sollten Messquellen über kleine Provider-/Fake-Schnittstellen injizieren. Mindestens ein Test muss verifizieren, dass der Adapter bei fehlendem `STW_PERF_TELEMETRY` vollständig deaktiviert bleibt. GPU-/Treiberwerte selbst gehören in den T4-Evidence-Run, nicht in Unit-Tests.

## Risiken und offene Entscheidungen

1. **GPU-Latenz:** Pass-Timestamps können dem CPU-Tick voraus-/nachlaufen. Ein Sample muss deshalb eine Frame-ID oder eine dokumentierte One-Frame-Delay-Semantik haben.
2. **Draw-Definition:** `m_totalDrawItemsRendered` ist die beste öffentliche O3DE-Metrik, aber kein garantiertes Hardware-Draw-Call-Äquivalent. Falls das Budget echte API-Calls meint, braucht es Backend-/Command-Recording-Instrumentierung.
3. **VRAM-Kosten:** `GatherMemoryStatistics` ist laut O3DE-Flags potenziell teuer. Nur im Telemetrieprofil aktivieren und den genauen Aggregationswert (`totalResident` oder `usedResident`) im Contract festschreiben.
4. **Server-Build:** STWGameplay-Quellen werden auch im Server-Target gebaut. Client-only Includes und Runtime-Code müssen daher sauber getrennt oder mit `AZ_TRAIT_SERVER` geschützt werden.
5. **Present-Gap:** O3DE registriert eine RHI-Statistik `Present` in den Plattform-Command-Queues. Diese kann als Diagnosewert ergänzt werden, ist aber nicht Teil des vereinbarten CSV-Schemas und sollte nicht mit GPU-Renderzeit vermischt werden.
6. **Budgetauslegung:** Die bestehende Policy setzt Frame-p95 16.67 ms, CPU-p95 12 ms und GPU-p95 14 ms. Vor Implementierung muss bestätigt werden, ob die aktuelle 37.6-ms-Frame-p95 ein bewusst akzeptierter Prototypzustand oder ein harter Release-Blocker ist.

## Geprüfte Evidenz und Verifikation

Read-only geprüft:

- Produktionsstand unter `/teamspace/studios/this_studio/stw-production`, Branch `brauny/stw-game-production`; Arbeitsbaum sauber.
- Runtime-Evidence: `stw-o3de-gate/player-slice-20260925T194016Z/performance-profile.log`.
- Vergleichsreport: `stw-o3de-gate/player-slice-20260922T231132Z/report.log` mit Frame-p95 über Budget sowie CPU/GPU-p95 innerhalb der damaligen Grenzwerte.
- Contract/Validator: `Docs/TechnicalBible/evidence-contracts.md`, `.github/lightning-t4/task.sh`, `Tools/visual_forge/forge.py` und `Config/VisualForge/policy.json`.
- O3DE-26.05-Quellen unter `/teamspace/studios/this_studio/o3de-2605` für die genannten RHI/RPI/AzCore-APIs.

Ausgeführt: reine Dateiprüfungen und Quelltextsuche erfolgreich (Exit Code 0). Kein Build, kein T4-/MP-Gate und keine Änderung an Produktionscode; dies entspricht dem read-only-Auftrag A-001.

Nicht verifiziert: neue CSV-Artefakte mit Draw-/VRAM-/RAM-Werten, drei vollständige Telemetrie-Runs und die tatsächliche Laufzeit-/Treibersemantik der Draw-Item-Zählung.

## Empfehlung an Claude / nächste Übergabe

Stufe 2 sollte zuerst das reine Modell plus Unit-Tests implementieren und anschließend den opt-in Client-Adapter anschließen. Danach drei getrennte T4-Runs erzeugen und ausschließlich über den bestehenden Forge-Validator bewerten. Die Gate-Ausführung benötigt vorher den Hub-Gate-Lock; A-001 selbst hat bewusst keinen Lock beansprucht.
