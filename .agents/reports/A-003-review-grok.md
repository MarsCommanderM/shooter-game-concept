# A-003 Review, Grok, 2026-10-01T21:30Z

SHA `48040296211c53db94b6f6971e51b437ac428942` auf `codex/stw-perf-telemetry-adapter-20260930`.
Basis `db369f9`. C-003 (`2bb5a99`) ist nicht enthalten.

## Beweis auf dieser SHA

- Player-Slice-Evidenz: `stw-o3de-gate/player-slice-20261001T212051Z`
- `SOURCE_COMMIT=48040296211c53db94b6f6971e51b437ac428942`
- `PERFORMANCE_TELEMETRY_CSV=PASS samples=1940 duration_s=60.026 rows=2870`
- Game.log derselben Session, 21:26:36:
  - `PERFORMANCE_TELEMETRY enabled=1 valid=true csv_written=true samples=1940 duration_s=60.026 frame_p95_ms=38.117 cpu_p95_ms=10.075 gpu_p95_ms=11.510 draw_calls_peak=958 vram_mib_peak=1020.562 ram_mib_peak=984.586 error=none`
  - `PERFORMANCE_FRAME_GAP frame_p95_ms=38.117 cpu_main_p95_ms=10.075 gpu_p95_ms=11.510 rhi_cpu_frame_p95_ms=38.143 present_p95_ms=UNAVAILABLE unattributed_gap_ms=26.607`
- `PERFORMANCE_PROFILE` im Evidenzordner: window 60.023 s, 1939 Samples, cpu und gpu mit echten Werten.
- Multiplayer: `stw-o3de-gate/multiplayer-20261001T212709Z-443342`, `RESULT=PASS`, gleiche SHA.
- Gate-CTest: `tests.log`, `Gem::STWGameplay.Tests.main::TEST_RUN` 1/1 Passed.
- `task.sh` des Laufs ist zu Ende gegangen und hat den Multiplayer-Gate angehängt. Die Prozesse sind seit 21:30Z weg. `gate.lock` liegt noch.

## Was die Zahlen sagen

Die Roadmap-Frage aus P1.1 ist beantwortet. Frame p95 38.1 ms liegt auf der RHI-CPU-Framezeit 38.1 ms. Main-Thread-CPU 10.1 ms und GPU 11.5 ms erklären sie nicht. `present_p95_ms=UNAVAILABLE` ist im Marker benannt, keine erfundene Present-Zahl.

## Offener Auftragspunkt

`PERFORMANCE_BASELINE` druckt weiter `cpu_frame_ms=UNAVAILABLE gpu_frame_ms=UNAVAILABLE` (`STWGameplaySystemComponent.cpp`, `RecordPerformance`). Das Gate prüft nur, dass die Zeile existiert. Die echten CPU/GPU-Werte stehen in `PERFORMANCE_PROFILE`, `PERFORMANCE_TELEMETRY` und `PERFORMANCE_FRAME_GAP`.

Entscheidung des Orchestrators: diese Altzeile wird in A-003 nicht mehr angefasst. Ein weiterer voller T4 nur dafür liefert weniger als der nächste Roadmap-Schritt. `RecordPerformance` und `UpdateAutomatedAcceptance` bleiben tabu.

## Review-Urteil

A-003 ist auf `4804029` sachlich bewiesen, sobald Codex `done` mit dieser SHA und diesen Pfaden postet. Kein weiterer Commit auf dem Adapter-Branch. Integration nach Produktion erst zusammen mit C-003, als eigene SHA, mit eigenem T4. `task.sh` (+55 in `e02fab3`) gehört zu diesem AP und wird mit integriert, nicht noch einmal erweitert.
