# Verifikation A-003 SHA 5ea7558, gelesen 2026-10-01T21:40Z

SHA `5ea7558d801f31a474c58ad048399f312a9fb2a2`. Lauf `stw-o3de-gate/player-slice-20261001T213251Z`. Kein Multiplayer-Ordner für diese SHA. `task.sh` ist beendet. `RESULT=PASS` steht in `report.log` nicht.

## Verifiziert

- `report.log` Zeile 10 und 37: `SOURCE_COMMIT` und `CHECKOUT_HEAD` sind diese SHA.
- `tests.log`: `Gem::STWGameplay.Tests.main::TEST_RUN` 1/1 Passed.
- `Game.log` 21:37:00: `PERFORMANCE_BASELINE average_fps=27.329 median_frame_ms=30.680 sample_seconds=10.026 samples=274 resolution=1920x1080`. Die Wörter `cpu_frame_ms=UNAVAILABLE` und `gpu_frame_ms=UNAVAILABLE` stehen in dieser Zeile nicht.
- `report.log` 21:38:20: `PERFORMANCE_PROFILE` mit echten cpu/gpu-Werten, window 60.015 s, 1972 Samples.
- Dieselbe Sekunde: `PERFORMANCE_TELEMETRY enabled=1 valid=false csv_written=true samples=1972 duration_s=59.985 frame_p95_ms=0.000 cpu_p95_ms=0.000 gpu_p95_ms=0.000 error=capture too short`.
- `PERFORMANCE_FRAME_GAP` daneben: Frame, CPU und GPU p95 sind 0. RHI-CPU p95 ist 37.599. Present ist `UNAVAILABLE`.

## Ursache, an den Zeilen gelesen

`PerformanceTelemetryModel.cpp` setzt `m_durationSeconds` auf die Spanne der Samples nach dem Warmup und gibt `capture too short` zurück, wenn diese Spanne kleiner als 60.0 ist (Zeilen 113–118). Der Adapter ruft `Finalize` auf, wenn die Summe der Frame-Deltas minus 30 mindestens 60 ist (`STWPerformanceTelemetryAdapter.cpp` Zeile 291). Im Log ist die gemessene Spanne 59.985 s. Der Lauf ist daran ausgestiegen, bevor der Multiplayer-Gate startete.

## Urteil

A-003 ist auf `5ea7558` nicht bestanden und nicht fertig. Ein unveränderter Wiederholungslauf beweist die Grenze nicht, weil 59.985 gegen 60.0 schon einmal real aufgetreten ist. Der nächste Commit auf diesem Branch darf nur die Stopp-Bedingung so legen, dass `Finalize` erst läuft, wenn die Spanne, die `Validate` misst, mindestens 60.0 s ist. Danach ein neuer T4 auf der neuen SHA.
