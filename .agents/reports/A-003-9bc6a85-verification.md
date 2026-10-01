# Verifikation A-003 SHA 9bc6a85, gelesen 2026-10-01T21:55Z

SHA `9bc6a856582a84a7b3b59c35dc337a4b12a04b47`, Branch `codex/stw-perf-telemetry-adapter-20260930`, Arbeitsbaum sauber. Kein Gate-Lock, kein `task.sh`.

## Gelesen

- `stw-o3de-gate/player-slice-20261001T214407Z/report.log`: `SOURCE_COMMIT=9bc6a856582a84a7b3b59c35dc337a4b12a04b47`, `GTEST_CASE_COUNT=414`, `PERFORMANCE_PROFILE=PASS samples=1909 window_s=60.006`, `PERFORMANCE_TELEMETRY_CSV=PASS samples=1910 duration_s=60.003 rows=2800`.
- `Game.log` 21:49:00: `PERFORMANCE_BASELINE average_fps=25.251 median_frame_ms=31.772 sample_seconds=10.019 samples=253 resolution=1920x1080`. Kein `UNAVAILABLE` in dieser Zeile.
- `Game.log` 21:50:20: `PERFORMANCE_TELEMETRY enabled=1 valid=true csv_written=true samples=1910 duration_s=60.003 frame_p95_ms=38.890 cpu_p95_ms=10.074 gpu_p95_ms=12.128 error=none`.
- Dieselbe Sekunde: `PERFORMANCE_FRAME_GAP frame_p95_ms=38.890 cpu_main_p95_ms=10.074 gpu_p95_ms=12.128 rhi_cpu_frame_p95_ms=38.851 present_p95_ms=UNAVAILABLE unattributed_gap_ms=26.762`.
- `stw-o3de-gate/multiplayer-20261001T215052Z-499552/report.log`: dieselbe SHA, `RESULT=PASS`.

## Urteil

A-003 ist auf dieser SHA bestanden. Present bleibt `UNAVAILABLE` und ist im Marker benannt. `5ea7558` bleibt ein Fehlschlag. Kein Push. Die Profil-p95 der CPU im Report ist 10.085 ms, die Telemetrie-Zeile sagt 10.074 ms. Beide stehen so in den Dateien.
