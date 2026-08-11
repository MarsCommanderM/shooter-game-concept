//! NOVA-Core: gemeinsame Typen für Client UND Server (game_core-Prinzip).
//! Dieselbe Simulation läuft headless auf dem Dedicated Server und als
//! Prediction-Spielebild auf dem Client.
pub mod ecs;
pub mod protocol;

pub use glam;

/// Autoritative Server-Tickrate (Bauplan-Vorgabe)
pub const TICK_RATE: u32 = 60;
pub const FIXED_DT: f32 = 1.0 / TICK_RATE as f32;

/// Anti-Cheat: maximale Lauf-/Sprint-Geschwindigkeit, die der Server akzeptiert
pub const WALK_SPEED: f32 = 5.2;
pub const SPRINT_SPEED: f32 = 8.6;
pub const ARENA_HALF: f32 = 48.0;
