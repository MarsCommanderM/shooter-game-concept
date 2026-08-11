# NOVA Milestone 02 — Design (vor Code)

Stand: definiert vor Implementierung, wie gefordert. Grenze strikt:
**core = Regeln/Daten/Simulation · client = Prediction/Rendering · server = Authority/Validation.**

## 1. IDs (nova-core::ids)
```rust
pub struct EntityId(pub u32);   // Bitlayout: [gen:8 | index:24]
pub struct ClientId(pub u32);   // 1:1 an Transport-Session, überlebt Reconnect nicht
pub struct MatchId(pub u64);    // Server-Startzeit + Zähler, für Replays/Stats
```
- Kein Spieler wird je über Array-Position oder Socket identifiziert.
- `World` adressiert ausschließlich via `EntityId`.
- Snapshot trägt `EntityId`, nicht Session-Index.

## 2. Modulstruktur
```
nova-core
├── ids.rs         EntityId/ClientId/MatchId, IdAllocator (Free-Liste)
├── sim.rs         simulate_player_movement()  ← CLIENT & SERVER identisch
│                  collision_rules() (Arena-Clamp, Push-Out)
├── weapons.rs     WeaponDef-Daten, damage_at(), fire_interval()
├── protocol.rs    Pakettypen (versioniert), Buttons-Flags
├── maps.rs        Arena-Geometrie (Client-Render & Server-Occlusion)
└── history.rs     TransformHistory (Rewind-Samples, capped 90)

nova-server
├── authority      Input-Validierung (Wish-Clamp, Rate, Buttons)
├── lagcomp        rewind = RTT/2 + INTERP; Hitbox-Rewind, Raycast, Restore
├── spawn.rs       Team-Spawns + Sicherheitsbewertung (Distanz zu Feinden)
├── match_state    Scores, Win-Score, Reset
└── replication    20 Hz Snapshots + Ack, künftig Interest-Filter

nova-client (wasm) / nova-web (three)
├── prediction     sim::step lokal mit eigenem Input
├── reconcile      Ack-Position vs. Predicted → Snap + Resim pending
├── interpolation  100-ms-Buffer, lerp/slerp, kurze Extrapolation (≤250 ms)
└── effects        Tracer, Sounds, HUD
```

## 3. Pakettypen (versioniert, additive = kompatibel)
```rust
// Header in jedem Paket:
// { v: u8 }  — aktuell v=2; JSON additive via serde(default), bincode = gleiche Version auf beiden Seiten
ClientMsg::Input { id, seq, wish, yaw, pitch, sprint, buttons, last_server_tick }
ClientMsg::Fire    (Legacy-Compat, rate-limitiert, fliegt in M03)
ServerMsg::Welcome { id, tick, team }
ServerMsg::Snapshot { tick, ack, players: [PlayerState], events, scores }
PlayerState { id: EntityId, name, pos, yaw, hp, kills, team }
GameEvent::Damage/Kill/MatchEnd/Break
```
Binary (M03): gleiche Typen, bincode → danach Quantisierung (i16 level-relativ) + Delta-Masks.
JSON bleibt Debug-Format für `/novaweb`.

## 4. Simulations-Kontrakt
```rust
pub fn step(s: &mut PlayerSim, inp: &InputSample, dt: f32, arena: &[maps::BoxDef]);
```
- Beschleunigung/Dämpfung/Capsule-Clamp exakt identisch client/server → Reconciliation muss nur
  Packet-Loss/RTT-Fehler korrigieren, keine Modell-Drifts.
- Server validiert VOR step: |wish| ≤ 1, Rate pro WeaponDef, Buttons-Whitelist.

## 5. Spawn-Sicherheitsbewertung
Respawn wählt aus Team-Spawns den mit maximalem `min_dist(Feinde)`,
Tie-Break: Abstand zur eigenen Squad-Gruppe. Keine Invulnerability;
„Line-of-Death"-Regel: Spawn liegt nie in direkter Sichtachse eines Gates (Map-Design).

## 6. Was NICHT kommt (bewusst)
Raytracing, GI, eigene Physik, eigenes Audio-Backend, MMO-Backend, 100-Player.
NOVA bleibt FPS-Engine für SAVE THE WORLD.
