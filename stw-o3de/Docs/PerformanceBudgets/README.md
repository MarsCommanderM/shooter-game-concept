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

Der aktuelle STW-Logger meldet `cpu_frame_ms=UNAVAILABLE gpu_frame_ms=UNAVAILABLE`.
Diese Ausgabe kann das neue Gate nicht bestehen. Profiler-/CSV-Adapter, tatsächliche
Draw Calls und Speichertelemetrie sowie Acht-Spieler-Messungen sind offene Arbeit.
