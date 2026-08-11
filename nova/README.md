# NOVA — eigene Engine für SAVE THE WORLD

**Rust + wgpu + WGSL + ECS light + headless Dedicated Server.**
Eine Codebasis, drei Targets: PC (Vulkan/DX12/Metal), Web (WASM+WebGPU), Mobile (Vulkan/Metal).
Kein Unreal, kein Unity — aber dieselbe Architektur-Disziplin.

## Meilenstein 01 — ✅ ERREICHT
- [x] Rust-Workspace (core / client / server)
- [x] winit-Fenster + Game-Loop
- [x] wgpu-Initialisierung (Backend-agnostisch → WebGPU-pfadfähig)
- [x] WGSL-Shader: **PBR (Cook-Torrance GGX) + ACES-Tonemapping**
- [x] 3D-Kamera + **FPS-Controller** (WASD, Shift-Sprint, Pointer-Lock-Mauslook)
- [x] glTF-2.0-Loading (gltf-crate, metallic-roughness) + Fallback-Geometrie
- [x] Depth-Buffer (Depth32Float, LEQUAL)
- [x] **Directional Shadows**: 2048² Depth-Pass (Front-Culling) + PCF 3×3
- [x] **Dedicated Server**: headless, 60 Hz fixed timestep, UDP, authoritative,
      Anti-Cheat-Wish-Clamp, Sequence-Tracking (Basis für Reconciliation)
- [x] Server läuft ohne GPU (bewiesen auf Contabo-VPS, siehe unten)
- [x] WebAssembly-Build: `cargo check --target wasm32-unknown-unknown -p nova-client` ✅ grün

## Crates (Bauplan-Trennung Engine / Game / Server)
```
nova/
├── crates/core      → game_core: ECS light, Protokoll, Balancing-Konstanten
│                      (läuft in Client UND Server — identische Simulation)
├── crates/client    → Renderer (wgpu/WGSL), Input, Kamera, glTF
└── crates/server    → headless: nova-server --port 27015 --tickrate 60 --max-players 16
```

## Bauen & Starten
```bash
# Client (PC mit GPU)
cargo run -p nova-client

# Dedicated Server (VPS, keine GPU nötig)
cargo build -p nova-server --release
./target/release/nova-server --port 27015 --tickrate 60 --max-players 16
```

## Netcode-Fahrplan (aus dem Bauplan)
1. ✅ 60-Hz-Simulation + Snapshots + Input-Seqs (2 Spieler im Snapshot verifiziert)
2. ✅ Client Prediction + Server-Reconciliation (pending-Queue, snap+re-sim bei >0.25 m Abweichung)
3. ✅ Snapshot-Interpolation für Remote-Player (100 ms Render-Delay, lerp zwischen T0/T1,
      Remote-Player werden als farbige Capsule-Boxes interpoliert gerendert)
4. ⏭ Lag-Compensation (Rewind für Hits)
5. ⏭ binäres Protokoll statt JSON (Bandbreite)
6. ✅ **Web-Bridge**: Server hört zusätzlich WebSocket auf UDP-Port+1 (27016),
   derselbe Protokoll-Stack; Client-Transport cfg-getrennt (nativ UDP / WASM WebSocket).
   Extern verifiziert gegen Contabo-VPS: UDP + WS parallel, je 60 Hz.
   ⏭ als Nächstes: WASM-Client + HTML-Shell auf die Domain deployen (wgpu webgl-Feature für Safari).

## Qualitätsprofile (Renderer skaliert, Spiel bleibt gleich)
WEB_LOW/WEB_HIGH · MOBILE_LOW/MOBILE_HIGH · PC_LOW/HIGH/ULTRA —
umgesetzt über Shadow-Map-Größe, Partikel-Dichte, Render-Scale (dynamic resolution mobile).

## Warum nicht Unity/Unreal?
Weil der Unterschied nicht in der Marke, sondern in den Subsystemen entsteht
(PBR, IBL, CSM, TAA, Netcode-Autorität) — und genau die bauen wir hier selbst,
in einem Renderer, der nativ UND im Browser läuft. C++-STW-Engine (`stw-engine/`)
war der Phase-0-Beweis; NOVA ist der Production-Pfad.
