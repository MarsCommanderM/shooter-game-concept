# STW Produktions-Roadmap bis Release

**Pflichtlektüre für jeden Agenten vor jeder Aufgabe.** Diese Datei sagt, *was*
als Nächstes gebaut wird und *wann* etwas als fertig gilt. Sie ersetzt nicht:

- [`AGENTS.md`](../AGENTS.md): Repo-Vertrag, Preflight, Branch-Regeln, Beweisstandard (hat Vorrang)
- [`docs/STW_PRODUCT_VISION.md`](STW_PRODUCT_VISION.md): *was* STW sein soll (einzige Vision, hat Vorrang)
- [`stw-o3de/Docs/README.md`](../stw-o3de/Docs/README.md): Project Visual Forge, visuelle Abnahme
- [`stw-o3de/Docs/PerformanceBudgets/README.md`](../stw-o3de/Docs/PerformanceBudgets/README.md): Performance-Gates

Bei Widerspruch gilt: AGENTS.md > Produktvision > diese Roadmap. Wenn die
Roadmap der Vision widerspricht, wird die Roadmap korrigiert, nie die Vision
abgesenkt.

Stand der Statusangaben: **2026-09-30**, geprüft gegen den Code auf
`brauny/stw-game-production` plus PR #9/#10/#11 (per `git grep`/Dateiliste,
nicht aus Erinnerung). Jede Statuszeile ist eine Momentaufnahme. Vor dem Start
eines Arbeitspakets den Status gegen den aktuellen Remote-HEAD neu prüfen.

---

## 0. Regeln für Agenten (kurz)

1. **Ein Arbeitspaket (AP) auf einmal.** Nimm das oberste offene AP der
   niedrigsten offenen Phase, dessen Abhängigkeiten erfüllt sind. Keine
   Phase überspringen, außer ein AP ist ausdrücklich als `parallel` markiert.
2. **Vor dem Start:** AGENTS.md-Preflight, Status des AP gegen den Code
   prüfen, nach Vorarbeit auf *allen* Branches suchen (`git log --all
   --grep`, `git branch -a`). Doppelte Arbeit ist in diesem Repo schon
   mehrfach passiert.
3. **Qualitätsmaßstab ist AAA, realistisch-cinematisch.** Blockout, Greybox,
   Engine-Samples und Platzhalter sind nie „fertig“. Klassifizierung nach
   Vision (`TEMPORARY_DEBUG` … `PRODUCTION_READY`) ist Pflicht.
4. **Fertig heißt bewiesen:** exakte SHA, T4-Gate `RESULT=PASS` auf genau
   dieser SHA, neuer Akzeptanz-Marker *im Gate-Skript verpflichtend geprüft*
   (nicht nur geloggt), bei visueller Arbeit ein angesehener Frame-Capture,
   bei Performance-Arbeit Messprotokoll nach PerformanceBudgets.
5. **Jede Änderung über Branch + PR gegen `brauny/stw-game-production`.**
   Nie Force-Push, nie History-Rewrite, Merge in Produktion nur mit
   ausdrücklicher Freigabe des Owners.
6. **Nach Abschluss eines AP:** Status in dieser Datei im selben PR
   aktualisieren (Status, SHA, Gate-Run-Pfad bzw. CI-Link).
7. **Host-Hygiene:** kein Gate parallel zu einer Live-Spielsitzung oder
   lokaler LLM-Inferenz auf demselben Host (eine T4-GPU, OOM-empfindlich).
8. **Offene Owner-Entscheidungen (Abschnitt 9) nie selbst raten.** Fragen.

**Koordination mehrerer Agenten:** Hub unter
`/teamspace/studios/this_studio/stw-agent-hub/.agents/` (Branch
`codex/stw-agent-hub`, auf GitHub gespiegelt). Regeln dort in `HUB.md`,
Aufträge in `assignments/`, Gate-Lock in `claims/gate.lock`. Claude ist
Orchestrator, Codex Engineer (Owner-Anweisung 2026-09-30).

Status-Werte: `OFFEN`, `IN_ARBEIT`, `FERTIG` (mit Beleg), `BLOCKIERT` (mit Grund),
`OWNER` (wartet auf Owner-Entscheidung).

---

## 1. Ist-Stand in einem Satz

Technisch tragfähiges Fundament (server-autoritativer Multiplayer über
Prozessgrenzen bewiesen, Bewegung/Traversal, Waffen, Gegner-KI-Grundschleife,
Menü, HUD, Team Deathmatch, zerstörbare Kisten, 80-Marker-T4-Gate).
**Inhaltlich überwiegend `TECHNICAL_VALIDATION` / `BLOCKOUT`**: keine
Produktions-Charaktere, keine Produktions-Waffenmodelle, kein Ragdoll, Gegner
ist das O3DE-Sample „Rin“, eine einzige Map, keine Performance-Telemetrie.

---

## Phase 0: Integrität und Hygiene (jetzt)

Ziel: sauberer, eindeutiger Produktionsstand, auf dem ohne Altlasten
weitergebaut werden kann.

| AP | Inhalt | Abnahme | Status |
|---|---|---|---|
| P0.1 | PR #9 (MP-Integrität), #11 (Hit-Evidence), #10 (Quality-Banner) in Produktion | kombinierte SHA T4-Gate PASS, Fast-Forward-Push | FERTIG: `b0c39841` (Merge #9 + #11 + #10) lokal T4 PASS (`stw-o3de-gate/player-slice-20260930T174641Z`) und GitHub-CI T4 success (Actions-Run 36758686737); Fast-Forward-Push 2026-09-30 18:27 UTC |
| P0.2 | Diese Roadmap in Produktion, verlinkt aus AGENTS.md | PR gemergt | FERTIG (PR #12) |
| P0.3 | Gate-Lücke schließen: `MAIN_MENU_ACCEPTANCE` und `DESTRUCTIBLE_ACCEPTANCE` werden nur geloggt, `task.sh` prüft sie nicht (beide am 2026-09-30 im Game.log `result=PASS`, aber nicht gate-erzwungen) | beide Marker in `task.sh` Pflicht; Negativtest | FERTIG: `2be2f14` (PR #13), 15 Pflichtprüfungen, Offline-Negativtest, lokal T4 PASS `stw-o3de-gate/player-slice-20260930T182024Z` |
| P0.4 | Recovery-Freeze Issue #5 / PR #4 zur Owner-Entscheidung aufbereiten. Befund 2026-09-30: die gesicherten Recovery-Branches (`recovery/stw-weekend-full-2026-09-14`, `recovery/stw-weekend-reconciled-2026-09-14`) enthalten **keinen** echten Ragdoll-Code, nur denselben Zustandsautomaten mit Kommentar „ragdoll ownership remains unavailable“ wie Produktion. Multiplayer-Anteil ist in Produktion überholt und gate-bewiesen. | Beweis-Kommentar in Issue #5; Schließen = Owner | OWNER |
| P0.5 | Repo-Hygiene: veraltete PRs #1/#2/#3 (gegen `main`), #7/#8 (überholte Recovery) bewerten; roter `Vercel`-Check stammt vom archivierten Web-Prototyp | Owner entscheidet schließen/behalten; Vercel-Integration trennen | OWNER |
| P0.6 | Lokale Studio-Aufräumarbeit: veraltete Worktrees (`stw-gate-source-worktree-*`, `stw-reconcile-production`, `stw-phase0-production`, kaputter `stw-industrial-yard`) dokumentiert entfernen, nur nach Sicherung | Liste + Sicherung + Owner-OK | OWNER |

## Phase 1: Vertical Slice „Industrial Yard“ in echter Produktionsqualität

Ziel: **ein** Sektor, der aussieht, sich anfühlt und läuft wie das fertige
Spiel. Das ist das Referenzmaß für alles Weitere (entspricht Blueprint
Phase 1 und Visual-Forge-Phasen 02–05, 07–09). Solange diese Phase nicht
abgenommen ist, wird kein neuer Inhalt in die Breite gebaut.

| AP | Inhalt | Abhängig | Abnahme | Status (2026-09-30) |
|---|---|---|---|---|
| P1.1 | **Performance-Telemetrie vervollständigen:** CPU/GPU-Frame-Zeit werden bereits gemessen (`PERFORMANCE_PROFILE`, `cpu_source=MainThreadCpuTime`, `gpu_source=PassTimestampExtent`; die Doku sagt fälschlich UNAVAILABLE). Es fehlen Draw Calls, VRAM/RAM-Peak, lückenloser CSV-Export, ≥3 Läufe, Formprüfung gegen `policy.json`. Befund `b0c3984`: Frame p95 37,6 ms bei CPU p95 10,7 / GPU p95 12,0 ms, Ursache der Differenz unbekannt | – | Messprotokoll nach PerformanceBudgets besteht Formprüfung; Differenz Frame vs. CPU/GPU erklärt | IN_ARBEIT (Hub A-001, Codex, Stufe 1 Design) |
| P1.2 | **First-Person-Arme** (`STW_FP_01`) für alle Waffenprofile, nicht nur `STW_RIFLE_02`; Hand-Sockets, ADS/Reload/Inspect-Animationen | – | Frame-Captures in Hüfte/ADS/Reload angesehen; Marker `ATOM_FIRSTPERSON_ARMS_MESH` im Gate Pflicht | TEILWEISE (nur Rifle 02, im Gate nicht Pflicht) |
| P1.3 | **Produktions-Waffenmodelle** (10 Profile unter `Assets/Weapons/`) nach Waffenbudget (FP 85k Tris, 8 Slots, 3 LODs) | P1.2 | je Waffe Visual-Forge-Review `PRODUCTION_CANDIDATE` | OFFEN (alle Platzhalter) |
| P1.4 | **Produktions-Spielercharakter** (ersetzt `STW_CHARACTER_01`) mit Rig, LODs, Collider | – | Visual-Forge-Review, anatomische Silhouette | OFFEN |
| P1.5 | **Produktions-Gegner** (ersetzt O3DE-Sample `STW_ENEMY_01_RIN`) | P1.4 (gleiche Pipeline) | Review; Rin vollständig entfernt | OFFEN |
| P1.6 | **Ragdoll nativ:** PhysX-Ragdoll-Konfiguration am Charakter-Asset, Runtime-Adapter, Übergang Animation→Physik→Reset, repliziert | P1.4 | Vision-Kriterien Ragdoll erfüllt; Gate-Marker für Übergang rein/raus; MP-Gate zeigt replizierten Zustand | NOT_YET_IMPLEMENTED |
| P1.7 | **Lighting-Framework:** Runtime-Anbindung `lighting_recipes.json` (heute `NOT_YET_IMPLEMENTED`). **Vorher** Prior-Art lesen: `codex/stw-cinematic-light-step1`, `codex/stw-sun-shadows`, `codex/stw-diag-sun-4lux`; Baseline-Frame zuerst ansehen. Versuch 2026-09-25 (100.000 lux) wurde verworfen. | – | drei geprüfte Szenen (Day/Night/Overcast), Frames angesehen | OFFEN |
| P1.8 | **Material-Library:** finale StandardPBR-Familien, Industrial-Yard-Oberflächen auf Produktionsniveau | P1.7 | Lichtvergleich-Kontaktbogen | OFFEN |
| P1.9 | **Bodycam-Rig physisch:** körpermontierte Kamera mit Lag/Sway (heute kopfgebunden), Reduced-Motion-Profil wählbar (Barrierefreiheit) | – | Test + Frame-Sequenz; bleibt reine Präsentation (Vision-Autoritätskette) | TEILWEISE (Linsen-Stack fertig) |
| P1.10 | **Audio:** positionales Waffen-/Umgebungs-/Schritt-Audio, Hall je Raum (heute keine Schritte, kein Reverb) | – | Hörprobe + Marker | OFFEN |
| P1.11 | **VFX:** Mündungsfeuer, Einschläge nach Material, Rauch/Staub (Mündungs-Cue heute prozedural) | P1.8 | Visual-Forge-Phase 06 | OFFEN |
| P1.12 | **Zerstörung Stufe 2:** zerstörbare Wände/Deckung, server-seitig synchron (Kisten A–D fertig) | – | MP-Gate: Zustand auf beiden Clients identisch | OFFEN |
| P1.13 | **Gegner-KI Stufe 2:** Navigation über NavMesh (RecastNavigation-Gem vorhanden, von STW ungenutzt), Deckung, Flankieren | P1.5 | Gate-Marker, keine Durch-Wand-Pfade | OFFEN |
| P1.14 | **8-Spieler-Abnahme** auf dem Slice (Visual-Forge-Phase 07) | P1.1 | 8 echte Clients, MP-Gate, Performance-Protokoll | OFFEN (bewiesen: 1 Server + 2 Clients) |
| P1.15 | **Performance-Abnahme** Slice: p95 ≤ 16,67 ms, 1080p High, 8 Spieler, 3 Läufe | P1.1–P1.14 | PerformanceBudgets vollständig | OFFEN |
| P1.16 | **Echter Spieltest mit dem Owner** (Maus/Tastatur + Gamepad, Gamepad an echter Hardware bisher unbestätigt) | P1.2–P1.13 | Owner-Feedback dokumentiert, Befunde als APs | OFFEN |

**Phase-1-Abnahme:** alle APs FERTIG, Slice-Inhalte mindestens
`PRODUCTION_CANDIDATE`, Owner-Freigabe. Erst dann Phase 2.

## Phase 2: Kern-Gameplay vollständig

Ziel: alle Spielmodi und Systeme, die ein vollständiges Match brauchen.

| AP | Inhalt | Status |
|---|---|---|
| P2.1 | Spielmodus **Herrschaft** (Capture-Points) | OFFEN (nur Menü-Button) |
| P2.2 | Spielmodus **Hauptquartier** (HQ-Ziel) | OFFEN (nur Menü-Button) |
| P2.3 | Spielmodus **Verteidigung** | OFFEN (nur Menü-Button) |
| P2.4 | Spielmodus **Sabotage** (Plant/Defuse-Timer) | OFFEN (nur Menü-Button) |
| P2.5 | Team Deathmatch auf echte Netzwerk-Clients (heute lokal-autoritativ geprüft) inkl. Scoreboard, Match-Ende, Rundenwechsel | TEILWEISE |
| P2.6 | Rollen/Klassen (Infanterie → Spezialist → Heavy, Blueprint II) als Loadout-Regeln, server-autoritativ | OFFEN |
| P2.7 | Video-Einstellungen (Auflösung, Vollbild, VSync, Qualitätsprofile der PerformanceBudgets); heute nur Kontrast | OFFEN |
| P2.8 | Einstellungs-Persistenz vollständig (Tastenbelegung fertig; Sound/Kontrast/Video/Sprache offen) | TEILWEISE |
| P2.9 | Kampagne: Inhalt und Umfang festlegen (heute leerer Menü-Screen) | OWNER |

## Phase 3: Welt und Combined Arms (Blueprint Phase 2+3)

| AP | Inhalt | Status |
|---|---|---|
| P3.1 | Sektor-Streaming (8–16 Sektoren, je eigene Level-Entity, eigene Light Probes) | OFFEN |
| P3.2 | Zone 1 „Perimeter“ (Häfen/Logistik/Highways) | OFFEN |
| P3.3 | Zone 2 „Urban Mixed“ | OFFEN |
| P3.4 | Zone 3 „Core“ (Hochhäuser, U-Bahn, vertikal) | OFFEN |
| P3.5 | Vein-System: Fahrzeug-Arterien, Infanterie-Venen, vertikale Knoten | OFFEN |
| P3.6 | Fahrzeuge (Jeep, Panzer inkl. Innenraum/Periskop), server-autoritative Physik | OFFEN (nichts vorhanden) |
| P3.7 | Interest Management / Replikationsfenster für 50+ Spieler | OFFEN |
| P3.8 | 50+-Spieler-Stresstest | OFFEN |

Jede neue Zone wird gegen den abgenommenen Phase-1-Slice als Qualitätsmaß
geprüft (Visual-Forge-Phase 10 „Scaling“: keine Regression).

## Phase 4: Meta-Progression (Blueprint Phase 3)

| AP | Inhalt | Status |
|---|---|---|
| P4.1 | Merit-System (Punkte, Beförderung, Degradierung), server-autoritativ, persistent | OFFEN |
| P4.2 | Wochen-Kampagnenzyklus (Infiltration → Durchbruch → Totaler Krieg) | OFFEN |
| P4.3 | Spieler-Persistenz / Accounts / Backend | OWNER (Backend-Wahl) |
| P4.4 | Prestige-Anzeigen (Abzeichen), Rang-UI | OFFEN |

## Phase 5: Release-Infrastruktur

| AP | Inhalt | Status |
|---|---|---|
| P5.1 | Dedizierte Server-Infrastruktur, Deployment, Server-Browser/Matchmaking | OFFEN |
| P5.2 | Anti-Cheat (server-seitige Validierung existiert für PvP-Treffer; Client-Schutz fehlt) | OFFEN |
| P5.3 | Crash-Reporting und Telemetrie | OFFEN |
| P5.4 | Lokalisierung: String-Tabellen, alle hartkodierten deutschen Texte umstellen, Sprachwahl in Einstellungen, 5 Sprachen | OWNER (welche 5; erst nach „alle Punkte“) |
| P5.5 | Barrierefreiheit (Reduced Motion, Untertitel, Farbfehlsicht, Remapping vollständig inkl. Gamepad) | OFFEN |
| P5.6 | Packaging/Release-Build PC, Store-Integration | OWNER (Store/Plattform) |
| P5.7 | Rechtliches: Lizenzen aller Assets/Drittbibliotheken, Altersfreigabe, EULA/Datenschutz | OFFEN |
| P5.8 | Konsolen-Vorbereitung (PlayStation/Xbox laut Vision „potenziell“) | OWNER |

## Phase 6: Polish, Beta, Release

| AP | Inhalt |
|---|---|
| P6.1 | Cinematic-Polish über alle Zonen, Balance Panzer/Infanterie |
| P6.2 | Performance auf festgelegter Ziel-Hardware (siehe Owner-Entscheidung) |
| P6.3 | Geschlossene Beta, Bug-Triage, Stabilitätsziele (Crash-Rate) |
| P6.4 | Offene Beta / Last-Test |
| P6.5 | Release-Candidate: alle Inhalte `PRODUCTION_READY`, alle Gates grün, Owner-Freigabe |
| P6.6 | Release 1.0, danach Live-Betrieb |

---

## 7. Bekannte Fallstricke (bereits teuer bezahlt)

- **Build-Quelle:** Gate/Build kompilieren aus `stw-o3de-worktree/stw-o3de/`,
  das `task.sh` aus `GITHUB_WORKSPACE` spiegelt. Manuelles `cmake --build`
  ohne Spiegelung sieht Änderungen nicht.
- **Server-Modul:** `task.sh` baut nur `STWGameplay` (Client). Server-Code
  wird nur über `multiplayer_gate.sh` gebaut (seit PR #9 in `task.sh`
  verkettet). Bei „Hook feuert nie“ zuerst `.so`-Zeitstempel prüfen.
- **Marker ≠ Optik:** Gate-Marker beweisen „geladen, Handles gültig“, nicht
  korrektes Aussehen. Visuelle APs brauchen einen angesehenen Frame.
- **Selbst ergänzte Marker** müssen in `task.sh` als Pflicht eingetragen
  werden, sonst bleiben sie unbewiesen (siehe P0.3).
- **Unity-Build:** Namen in anonymen Namespaces müssen gem-weit eindeutig sein.
- **Asset-Suffixe:** der letzte `_suffix` im Dateinamen wählt das
  Kompressions-Preset (`_Metallic` → PBR-Preset). UI-Sprites `_UIL` nutzen.
- **EMotionFX ActorGroup:** kein `selectedRootBone` setzen; Knotenpfade
  mit `RootNode.` präfixen.
- **Lighting:** erst Baseline-Frame ansehen, dann Prior-Art-Branches lesen,
  dann ändern. Physikalische Lux-Werte passen nicht 1:1.
- **Auto-Mode:** Pushes auf den Produktionsbranch brauchen eine direkte,
  ausdrückliche Anweisung des Owners im Chat.

## 8. Gate-Referenz

- Einzelprozess + Multiplayer: `.github/lightning-t4/task.sh` (lokal mit
  `GITHUB_WORKSPACE`=separater Worktree, `GITHUB_SHA`=volle SHA,
  `GITHUB_REF=refs/heads/brauny/stw-game-production`,
  `GITHUB_REPOSITORY=MarsCommanderM/shooter-game-concept`)
- Evidenz: `stw-o3de-gate/player-slice-<UTC>/report.log` mit `RESULT=PASS`
- CI: `STW Lightning T4` (Self-hosted Runner, on demand:
  `cd /teamspace/studios/this_studio/.stw-github-runner && ./run.sh --once`),
  `STW Repository Contract`, `STW Visual Forge contracts`

## 9. Offene Owner-Entscheidungen

| # | Frage | Blockiert |
|---|---|---|
| E1 | Issue #5 / PR #4 schließen? (Beweislage siehe P0.4) | P0.4 |
| E2 | Ziel-Hardware (Mindest-/Empfohlene GPU) für Release | P1.15, P6.2 |
| E3 | Spielerzahl Release 1.0: 8 (Visual Forge) oder 50+ (Blueprint)? | P3.7, P3.8 |
| E4 | Art-Beschaffung: eigene DCC-Produktion, Outsourcing oder lizenzierte Assets? | P1.3–P1.5 |
| E5 | Kampagne: Singleplayer-Story, Koop oder nur MP-Wochenkampagne? | P2.9 |
| E6 | Welche 5 Sprachen? | P5.4 |
| E7 | Store/Plattform für Release (Steam, Epic, eigene) und Konsolen ja/nein | P5.6, P5.8 |
| E8 | Backend für Accounts/Persistenz | P4.3 |
