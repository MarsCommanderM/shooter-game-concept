# WIRRWARR — Engine- & Architektur-Entscheidung

> **UPDATE 2026-08-11: EIGENE ENGINE BESCHLOSSEN.** Statt Unity/Unreal-Port bauen wir die
> **STW-ENGINE** (`stw-engine/`): SDL2 + OpenGL 4.5 + GLM + glTF 2.0, Forward+, Renderer-Interface
> austauschbar (Web-Backend später). **Phase 1 (Engine-Kernel) kompiliert bereits**: Fenster, Game-Loop,
> Pointer-Lock-Input, glTF-Loader mit Binary-Cache, PBR-Shader (GGX + ACES). Siehe `stw-engine/README.md`.
> Der Browser-Prototyp bleibt Gameplay-Labor + Web-Backend-Referenz.

> Antwort auf die Frage „WebGL im Browser oder Pixel-Streaming?" und die Stack-Empfehlung (Unity/Unreal + Netcode + Audio + VFX-LOD).
> Stand: 2026-08-11 · Status: **Entscheidung getroffen, Phase 1 läuft live.**

---

## 1. Die Kernentscheidung: WebGL im Browser — kein Pixel-Streaming

| Kriterium | WebGL (Browser) | Pixel-Streaming |
|---|---|---|
| Unser Server (Contabo Cloud VPS 8) | ✅ läuft bereits live auf :3000 | ❌ **CPU-only-VPS, keine GPU → unmöglich** |
| iPhone (Safari, ohne App-Install) | ✅ läuft heute | ⚠️ nur über Umweg |
| Kosten | ✅ 0 extra | ❌ GPU-Instanz pro Spieler |
| Skalierung 6v6/10v10 | ✅ Dedicated-WS-Server reicht | ❌ GPU-Kosten explodieren |

**Fazit:** Solange die Infrastruktur kein GPU-Hosting bekommt, ist Browser-WebGL der einzige realistische Pfad — und er ist bereits deployed (`http://169.58.152.88:3000`).

---

## 2. Subsystem-Mapping: AAA-Liste → was WIRRWARR schon hat

| AAA-Baustein | WIRRWARR-Status |
|---|---|
| Rendering (PBR, Tonemapping, Bloom) | ✅ Three.js: ACES-Tonemapping, UnrealBloomPass, prozedurale Texturen |
| Himmel/Atmosphäre | ✅ Shader-Sky, Sterne, Planet, Sporen-Partikel, Fog |
| Schatten (Cascaded-artig) | ✅ PCFSoft-Directional-Shadow auf Qualitätsstufe HIGH |
| Character Controller | ✅ Slide, Crouch, Prone, Mantle-artige Support-Checks, Bob, Lande-Dip |
| Waffen-Feel (Recoil/ADS/Sway) | ✅ Recoil-Kick, FOV-Kick, Muzzle-Light, Tracer |
| Physik/Interaktion | ✅ zerstörbare Wände (Breschen), Biomass-Wuchs, Granaten mit Gravitation |
| KI | ✅ A*-Pathfinding, Personas (sniper/aggro/rush), adaptive Schwierigkeit, Bosse mit Phasen |
| Netcode | 🟡 WS-Relay-Server (`server.mjs`) — nächste Stufe: Server-Authoritative (s. §4) |
| Audio | ✅ **ab heute: HRTF-Spatial-Audio** (PannerNode) für Gegner-Schüsse & Explosionen, prozedurale SFX, Voice-Lines |
| HUD/UX | ✅ HUD mit Safe-Area-Fix (iOS), Killfeed, Kompass, Streamer-Modus, EN-Toggle |
| Spectator/Replay | ✅ Replay-Viewer, Killcams, Streak-Cams, Heatmaps |
| Anti-Cheat | 🟡 Server-authoritative Entscheidungen sind der Plan (s. §4) |
| Qualitäts-Drehknöpfe (Netz/Grafik/Sim) | ✅ Grafik: LOW/MED/HIGH (Pixel-Ratio, Schatten, Bloom, Partikel-Cap) |

---

## 3. Eskalationsstufen (der konkrete Fahrplan)

### Stufe 1 — JETZT: Web-Prototyp als Vertical Slice
Three.js + Next.js + WebSocket-Server. Alles was hier gebaut wird, ist **Design-Forschung**: Gunplay-Tuning, Maps, Modi, Balance, Feel. Das ist der Teil, der in jeder Engine gleich bleibt.
→ Läuft live, wird weiter poliert.

### Stufe 2 — NÄCHSTER MEILENSTEIN: Unity (URP) + Dedicated Server
Wenn der Prototyp das Gameplay bewiesen hat: Port nach **Unity URP** (Mobile + WebGL-Export laut Stack-Analyse der pragmatischste Weg).
- Netcode: **server-authoritativ** + Client-Prediction + Lag-Compensation/Rewind für Projektil-Impacts
- Hosting: eigener Authoritative-Server (Node bleibt als Relay/Meta-Layer denkbar) oder Photon Fusion mit Host-Migration für kleine Lobbys, Dedicated für 6v6/10v10
- Audio: FMOD oder Wwise mit Occlusion/Reverb-Zonen (das HRTF-Verhalten aus Stufe 1 ist die Referenz)
- Matchmaking: skillbasiert über unsere Divisionen (Bronze→Apex)

### Stufe 3 — OPTIONAL: Unreal (nur bei Native/Konsole)
Nur falls Zielplattform PC/Konsole mit Maximal-Grafik wird. Web via Unreal ist ohne Streaming nicht realistisch; Streaming scheitert an der GPU-Frage (§1).

---

## 4. Netcode-Zielbild für 6v6 / 10v10 (aus der Stack-Analyse übernommen)

1. **Server entscheidet alles** (Treffer, Spawns, Objektiv-State) → reduziert Cheat-Fläche.
2. **Client-Prediction + Reconciliation** fürs Movement (kein Gummi-Feeling).
3. **Projektile:** Server similiert Impact-Entscheidung mit Rewind (Hitbox-Zustand t-latency), Clients simulieren Trajektorie visuell nach (seed + Korrektur).
4. **Relevanz-Sets:** pro Client nur Entitäten im Umkreis/POI replizieren → Bandbreite für 10v10.
5. **Replay-Daten:** Projektil-Verläufe werden serverseitig geloggt (unsere Replay-Engine in Stufe 1 ist die Blaupause).
6. **Tick-Budget:** Ziel 30–60 Hz Server-Tick, 120+ Hz Client-Loop (Three.js läuft bereits ungedrosselt).

---

## 5. Was als Nächstes gebaut wird (Backlog, priorisiert)

- [ ] Server-authoritative Treffer-Validierung in `server.mjs` (Anti-Cheat-Basis)
- [ ] Bot-Radio-Chatter über Spatial-Audio (Richtungshören von Team-Calls)
- [ ] Killfeed mit Waffen-Icons
- [ ] Replay-Kameramodus (frei orbitbar, eSport-Broadcast-Vorbereitung)
- [ ] Netz-LOD / Relevanz-Sets im Online-Modus
- [ ] EN-Übersetzung der Story

---

*Regel für alle künftigen Features: erst in Stufe 1 beweisen, dass es Spaß macht — dann erst in die Ziel-Engine portieren. Gameplay-Forschung ist Engine-unabhängig.*
