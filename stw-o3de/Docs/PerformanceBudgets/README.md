# Performance-Ziele und Messprotokoll

Maschinenlesbare Quelle: `Config/VisualForge/policy.json`.
Alle Zahlen sind **PROVISIONAL_TARGET**, nicht bereits erreichte Performance.
Erster Abnahme-Slot: 1920×1080 Ausgabe, Quality_High (0,9 Render Scale), acht
Spieler, RT aus, 60-fps-Ziel. Zielhardware für Produktrelease ist noch festzulegen;
die vorhandene T4 ist eine CI-Messmaschine, keine automatisch bestätigte Mindest-GPU.

| Messgröße | Gate |
|---|---|
| Frame p95 / p99 | ≤ 16,67 / 22,22 ms |
| CPU / GPU p95 | ≤ 12 / 14 ms, separat gemessen |
| Draw Calls Spitzenwert | ≤ 2.000 |
| VRAM / RAM Spitzenwert | ≤ 6.144 / 12.288 MiB |
| Läufe / Warmup / Messfenster | mindestens 3 / 30 s / 60 s je Lauf |

Alle vier Profile werden auf derselben Maschine mit derselben Szene, Kamera-
Route und Spiellast verglichen. GPU-Treiber, OS, Build-Konfiguration, Capture-Tool,
Engine-Commit, Spiel-Commit und effektive Einstellungen sind Pflichtfelder.
CPU-/GPU-Zeiten niemals aus FPS schätzen. MiB = 1.048.576 Byte.
Perzentile werden per Nearest Rank berechnet, Peaks über das gesamte Messfenster.
Shader-Warmup dokumentieren; zusätzliche Cold-Cache-Runs separat erfassen.

CSV benötigt lückenlose Frame-Samples, keine ausgedünnte Sekundenstatistik.
Jeder Lauf startet seine Zeit nahe null, enthält Warmup und mindestens 600 Samples
im Messfenster. Die summierte Framezeit muss innerhalb 5 % zur Laufzeit passen.
Unbekannte, nullgesetzte oder nicht endliche Telemetrie blockiert das Gate.

Waffenbudgets: First Person 85.000 Dreiecke, Third Person 45.000; maximal acht
Materialslots, FP-Texturbudget 180 MiB, Gameplay-Texturkante 2.048, drei LOD-Stufen.
Materialslots sind keine Draw-Call-Messung. Separate Cinematic-Klasse erlaubt
4K und höhere Geometriekosten, bleibt außerhalb der Gameplay-Freigabe.

Der Adapter wird ausschließlich mit `STW_PERF_TELEMETRY=1` und einem nichtleeren
`STW_PERF_TELEMETRY_CSV=<path>` aktiviert. Ohne diese beiden Variablen gibt es keine
Speicherabfrage, keine Draw-/GPU-Abfrage und keine Datei-I/O. Der CSV-Vertrag wird
von `STWGameplay/Source/Clients/STWPerformanceTelemetryAdapter` bedient.

`draw_calls` bedeutet im O3DE-Adapter die Anzahl der pro Frame ausgeführten Atom-
Draw-Items (`PassSystemFrameStatistics::m_totalDrawItemsRendered`), nicht eine
behauptete Anzahl niedriger Treiber- oder API-Aufrufe. `vram_mib` ist die Summe der
`m_totalResidentInBytes` aller RHI-Device-Heaps, ohne Pool-Doppelzählung. `ram_mib`
ist der aktuelle Process-Working-Set; `m_peakWorkingSet` bleibt als echte Prozess-
Spitzenmessung verfügbar. Die Frame-Gap-Zeile stellt RHI-CPU-Framezeit, Main-Thread-
CPU und Pass-Timestamp-GPU getrennt dar. Die öffentliche RHI-Schnittstelle stellt
keine Present-Statistik bereit, daher bleibt `present_ms=UNAVAILABLE` ausdrücklich
unbelegt statt aus anderen Zeiten abgeleitet.
