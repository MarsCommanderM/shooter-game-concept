# Platin-Systemaudit — 31. August 2026

## 1. Executive Status

**Gesamtklassifikation: nicht produktionsreif.** Der aktuelle Branch ist nicht der im
übergebenen Block-6B-Bericht beschriebene O3DE-Produktionsstand. Er enthält stattdessen
drei parallel gepflegte Implementierungen: eine Next.js-/WebSocket-Anwendung, die
Rust/wgpu-Engine NOVA und einen C++/OpenGL-Prototyp. Die genannten
`stw-o3de/...`-Pfade, `task.sh`-Gates und O3DE-Runtime-Beweise existieren in diesem
Checkout nicht und konnten deshalb nicht bestätigt werden.

Der Scan umfasste alle 197 von Git erfassten Dateien sowie Konfigurationen und
Workspace-Dateien außerhalb von `.git`, `node_modules`, Build-Caches und binären
Medieninhalten. Abhängigkeiten wurden über ihre Manifeste und Lockfiles geprüft;
vendored Pakete wurden nicht Quellzeile für Quellzeile auditiert. Der bereits vor dem
Audit veränderte `pnpm-lock.yaml` wurde weder zurückgesetzt noch in diesen Audit-Commit
aufgenommen.

## 2. Reproduzierbare Check-Matrix

| Check | Ergebnis | Bedeutung |
|---|---|---|
| `pnpm exec tsc --noEmit` | PASS | TypeScript kompiliert im strikten Einzelcheck. |
| `pnpm lint` | FAIL | ESLint 10 findet keine Flat-Config; das Qualitätsgate ist wirkungslos. |
| `pnpm build` | FAIL | Der Build lädt Google Fonts aus dem Netz und ist nicht hermetisch. |
| `pnpm audit --prod` | BLOCKED | Registry-Audit-Endpunkt antwortete mit HTTP 403; keine Aussage zur Vulnerability-Freiheit. |
| `cargo fmt --manifest-path nova/Cargo.toml --all -- --check` | FAIL | Umfangreiche Formatabweichungen in NOVA. |
| `cargo test --manifest-path nova/Cargo.toml --workspace --all-targets` | FAIL | `nova-probe` initialisiert `ClientMsg::Fire` ohne das Pflichtfeld `weapon`. |
| `cargo clippy --manifest-path nova/Cargo.toml --workspace --all-targets -- -D warnings` | FAIL | Bereits `nova-core` scheitert an drei Clippy-Verstößen; weitere Compilerwarnungen sind vorhanden. |
| `bash -n deploy/remote-install.sh deploy/setup.sh deploy/watchdog.sh` | PASS | Alle versionierten Shellskripte sind syntaktisch gültig. |
| statischer Secret-Pattern-Scan über Git-Dateien | PASS | Keine bekannten Private-Key-, AWS- oder GitHub-Token-Muster gefunden. |
| `cmake -S stw-engine -B /tmp/stw-audit-build` | BLOCKED | SDL2-Development-Paket bzw. CMake-Paketkonfiguration fehlt auf diesem Host. |

## 3. Kritische Fehler — sofort beheben

### P0-1: Der Rust-Workspace ist nicht vollständig baubar

`nova-probe` verwendet ein veraltetes Protokollkonstrukt. Dadurch scheitert der
Workspace-Test bereits beim Kompilieren. Ein Probe-/Smoke-Test, der nicht kompiliert,
kann keine Deployment-Evidenz liefern. Zusätzlich sind im erfassten Workspace keine
echten `#[test]`-Fälle vorhanden.

**Fix:** `weapon` explizit setzen, Protokollkonstruktoren zentralisieren und einen
CI-Test für jede `ClientMsg`-Variante ergänzen. Danach Workspace-Tests für native und
WASM-Ziele erzwingen.

### P0-2: Produktions-Build und Lint sind keine verlässlichen Gates

ESLint ist als Script eingetragen, besitzt aber keine `eslint.config.*`. Next.js
unterdrückt außerdem explizit TypeScript-Buildfehler. Der Build hängt von zwei zur
Buildzeit heruntergeladenen Google Fonts ab und fällt ohne Zugriff darauf aus.

**Fix:** Flat-Config einchecken, `ignoreBuildErrors` entfernen, Fonts lokal hosten und
`pnpm install --frozen-lockfile`, Typecheck, Lint und Build in CI ausführen.

### P0-3: Der Node-WebSocket-Server vertraut sicherheitsrelevanten Clientdaten

Der Server übernimmt Clientpositionen ohne Prüfung, akzeptiert beliebige unbekannte
Nachrichtentypen und broadcastet sie, persistiert clientgemeldete Scores/Heatmaps und
Range-Werte und authentifiziert WebSocket-Upgrades nicht. Die Trefferprüfung validiert
nur Schaden, Rate und optional Distanz auf Basis ebenfalls clientgemeldeter Positionen.
Damit sind „server-authoritative“ und Leaderboard-Integrität nicht gegeben.

Es fehlen außerdem Origin-Prüfung, globale Nachrichten-/Byte-Ratenlimits,
Schema-Validierung, Backpressure-Handling und eine maximale Raumzahl. Ein Angreifer
kann beliebig viele Raum-Keys erzeugen und `rooms` dauerhaft wachsen lassen.

**Fix:** Eingaben mit strikt diskriminierten Zod-Schemas und Byte-Limits validieren,
Origins allowlisten, Token-Bucket-Limits pro IP/Socket setzen, unbekannte Nachrichten
verwerfen, leere Räume löschen und Scores ausschließlich aus serverseitigem Matchstate
ableiten. Für internetexponierten Betrieb TLS/WSS erzwingen.

### P0-4: Keine CI und keine automatisierte Qualitätsbaseline

Es gibt keine `.github/workflows`-Datei. Gegenwärtig kann jeder der oben genannten
Fehler unbemerkt gemergt werden. Die geforderte 100-%-Abdeckung kritischer Pfade ist
nicht messbar; Coverage-Tooling und Schwellenwerte fehlen vollständig.

**Fix:** Pflichtworkflow mit reproduzierbarer Node-/Rust-Toolchain, Format, Lint,
Typecheck, Unit-/Integrationstests, Coverage, Dependency-Audit und Build-Artefakten.

## 4. Potenzielle Risiken

1. **Blockierende synchrone Persistenz:** `scores.json` wird pro Nachricht synchron
   vollständig gelesen und geschrieben. Fehler werden verschluckt. Das kann den
   Event-Loop blockieren, Daten bei parallelen Updates verlieren und Disk-I/O-Spam
   erlauben.
2. **Unvalidierte numerische Werte:** `state.x/z`, Farben, Heatmap-Koordinaten und
   WebRTC-Signaling werden nicht auf Endlichkeit, Bereich oder Struktur geprüft.
   `NaN`, extreme Werte und große Signaling-Objekte können State und Clients belasten.
3. **Rust-Server-Ressourcen:** Der NOVA-WebSocket-Listener startet einen OS-Thread pro
   Verbindung, verwendet unbeschränkte Channels/Queues und akzeptiert Handshakes ohne
   Origin- oder Größenlimit. Mutex-Poisoning führt durch zahlreiche `unwrap()` zu
   Prozessabbrüchen.
4. **Fehler werden maskiert:** Score-I/O und Service-Worker-Cachefehler werden bewusst
   ignoriert; strukturierte Logs, Metriken, Traces, Request-IDs und Error-Tracking fehlen.
5. **Deployment ohne TLS-Härtung:** Die Nginx-Vorlage lauscht nur auf Port 80 und setzt
   weder Security Header noch Request-/Connection-Limits.
6. **Generierte Webartefakte doppelt versioniert:** Zwei WASM/JS-Bundles liegen in
   `nova/web` und `nova/web8090`. Ohne reproduzierbaren Generator- und Drift-Check droht
   Source-/Artefakt-Divergenz.
7. **Abhängigkeitsprüfung unvollständig:** Der npm-Audit war durch HTTP 403 blockiert;
   RustSec (`cargo audit`) und Lizenz-/SBOM-Gates sind nicht konfiguriert.

## 5. Performance-Schwachstellen

1. Die React-Spieldateien sind monolithisch (`real-game.tsx`: 4.661 Zeilen,
   `online-game.tsx`: 1.687, `play-demo.tsx`: 1.386). Das erschwert gezieltes
   Code-Splitting, Profiling, Tests und Render-Isolation.
2. Alle Next-Images sind global `unoptimized`; responsive Transformation und moderne
   Formatauswahl werden damit verschenkt. Das 1,8-MB-App-Icon und große Key-Art-Dateien
   erhöhen Transfer, Decode-Zeit und Cache-Druck.
3. Der Service Worker nutzt Cache-First für alle same-origin GET-Assets, besitzt aber
   weder Größen-/Eintragslimits noch eine differenzierte Cache-Strategie. Caches können
   wachsen und API-artige GET-Antworten veralten.
4. Synchrone JSON-Dateioperationen liegen direkt im heißen WebSocket-Pfad.
5. Der Rust-Server klont für jeden Empfänger vollständige Player-/Event-Snapshots und
   serialisiert sowohl binär als auch JSON, selbst wenn nur eines der Formate gebraucht
   wird. Das skaliert unnötig mit Spielern und Snapshotfrequenz.
6. Eine belastbare Aussage zu „< 1 s“ ist nicht möglich: Lighthouse/Web-Vitals,
   Bundle-Budgets, Server-Lasttests und GPU-Frame-Timings fehlen.

## 6. Verstöße gegen Platin-Standards

- **0 Bugs / 0 Warnungen:** Nicht erfüllt; Build, Lint, Rust-Tests, Format und Clippy
  schlagen fehl.
- **100 % kritische Testabdeckung:** Nicht erfüllt; keine messbare JS/TS-Abdeckung und
  keine Rust-Unit-Tests im Scan gefunden.
- **Bulletproof Security:** Nicht erfüllt; keine Authentisierung, Schema-/Origin-Gates,
  robuste Rate-Limits oder belastbare serverseitige Score-Autorität.
- **Produktionsreifes Observability:** Nicht erfüllt; nur freie `console.log`-/`println!`-
  Texte, kein zentraler Fehlerkanal und keine SLOs.
- **Reproduzierbarkeit:** Nicht erfüllt; Remote-Fonts, fehlende CI, versionierte
  Buildartefakte ohne Drift-Gate und nicht dokumentierte native Systemabhängigkeiten.
- **Warnungsfreiheit:** Nicht erfüllt; deprecated Tungstenite-Methoden, tote Felder,
  ungenutzte Imports/Variablen und Formatabweichungen.

## 7. Best-Practice-Architekturabweichungen

Das Repository bündelt Marketing-Site, browserbasiertes Spiel, zwei native Engines,
Server, generierte Webartefakte und Deploymentskripte ohne explizite Produktgrenzen
oder gemeinsame Quality-Gates. Im Node-Server sind HTTP-Hosting, Transport, Matchrooms,
Combatvalidierung, Leaderboards, Heatmaps, Voice-Signaling und Dateipersistenz in einer
einzigen 241-Zeilen-Datei gekoppelt. Im Rust-Client liegen Audio, Rendering, Input,
Netcode und Plattformintegration überwiegend in einer 1.689-Zeilen-Datei.

**Zielbild:** Monorepo-Grenzen dokumentieren; Transportadapter, Domänenmodell,
Persistence, Match-Simulation und Präsentation trennen; generierte Artefakte eindeutig
kennzeichnen; gemeinsame Protokollschemas versionieren; Web- und NOVA-Server nicht als
gleichzeitig autoritative Produktionspfade behandeln.

## 8. Drei-Iterationen-Kriegsplan

### Iteration 1 — Build wieder wahr machen (P0, Ziel: alle Baseline-Gates grün)

1. `nova-probe` reparieren; Rust formatieren; Warnungen, deprecated Calls und Clippy-
   Fehler auf null bringen.
2. ESLint Flat-Config ergänzen, TypeScript-Fehlerunterdrückung entfernen und lokale
   Fonts einführen.
3. CI-Matrix mit frozen Lockfile, Typecheck, Lint, Next-Build, Cargo fmt/test/clippy,
   ShellCheck und Artefakt-Driftprüfung einführen.
4. Kritische Unit-Tests für Protokoll, Movement, Fire-Rate, Damage, Score und
   Serialisierungsgrenzen ergänzen; Coverage zunächst messen und als nicht sinkendes
   Gate festhalten.
5. C++-Systemabhängigkeiten dokumentieren und einen containerisierten CMake-Build
   bereitstellen.

**Exit:** Null Compiler-/Lintwarnungen, jeder Workspace kompiliert, reproduzierbarer
Offline-Webbuild, Pflicht-CI grün.

### Iteration 2 — Autorität und Resilienz erzwingen (P0/P1, Ziel: Angriffsfläche drastisch reduzieren)

1. Node- und Rust-Eingänge mit Größenlimit, Schema, endlichen Zahlen,
   Wertebereichen, Origin-Allowlist und Token-Bucket schützen.
2. Node-Combat/Score vollständig serverautoritativ machen; unbekannte Events strikt
   ablehnen; Rooms und Sessions mit Lifecycle-/Idle-GC versehen.
3. Score-Persistenz in einen asynchronen Repository-Adapter mit atomaren Transaktionen,
   Migrationen und Fehlertelemetrie verschieben.
4. Transport-, Domain- und Persistence-Integrationstests sowie Fuzz-/Propertytests für
   binäre und JSON-Protokolle hinzufügen.
5. TLS, Security Header, Connection-/Body-Limits und Secret-Management als gehärtete
   Deploymentdefaults liefern.

**Exit:** Negativtests für Spoofing, Flooding, malformed input und Replay grün; keine
clientautoritiven Rankings; keine unbeschränkten Queues oder Räume.

### Iteration 3 — Performance, Wartbarkeit und Betriebsreife (P1/P2, Ziel: messbare Platin-SLOs)

1. Monolithische Komponenten und Rust-Client nach Domain/Subsystem zerlegen und große
   Routen dynamisch laden; Images optimieren und Budgets für JS/WASM/Medien definieren.
2. Snapshotserialization pro Transport nur einmal durchführen, Backpressure ergänzen
   und Lasttests für 16/64/128 Verbindungen etablieren.
3. Service-Worker-Caches begrenzen/versionieren und Offline-/Update-E2E-Tests ergänzen.
4. Strukturierte Logs, RED-/USE-Metriken, Crash-Reporting, Health/Readiness und
   Alarmgrenzen einführen.
5. Lighthouse/Web-Vitals, Bundlegröße, Server-p95/p99, Paketverlust, GPU-Framezeit und
   Memory-Leaks als Release-Gates messen; erst anhand dieser Daten ein `<1 s`-SLO
   verbindlich festlegen.
6. Coverage der kritischen Domainpfade auf 100 % erhöhen; Gesamtcoverage mit
   begründetem, ratcheting Mindestwert schützen.

**Exit:** Dokumentierte SLOs und Architekturentscheidungen, grüne Security-/Load-/E2E-
Suites, reproduzierbare Releases, nachgewiesene kritische 100-%-Coverage.

## 9. Definition of Done für „Platin“

„Platin“ darf erst vergeben werden, wenn nicht nur Implementierungen vorhanden sind,
sondern CI-Artefakte folgende Evidenz liefern: grüne Builds auf allen Zielplattformen,
null Warnungen, 100 % Branch-Coverage der als kritisch inventarisierten Domainpfade,
bestandene Abuse-/Fuzz-/Lasttests, SBOM und Vulnerability-Gates, Web-Vitals innerhalb
vereinbarter Budgets sowie ein erfolgreicher Restore-/Rollback-Test. Dieser Audit ist
die Bestandsaufnahme und der priorisierte Weg dorthin — keine Behauptung, dass diese
Evidenz bereits vorliegt.
