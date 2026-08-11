//! Netzwerkprotokoll — binär via bincode (Bandbreite!), UDP + WS(Binary-Frames).
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum ClientMsg {
    Join { name: String },
    Input {
        id: u32,
        seq: u32,
        wish: [f32; 2],
        yaw: f32,
        #[serde(default)]
        pitch: f32,
        #[serde(default)]
        sprint: bool,
        /// Bitflags: 1=SPRINT 2=FIRE (Commands statt Zustände)
        #[serde(default)]
        buttons: u8,
        /// letzter vom Client empfangener Server-Tick (RTT-Messung)
        #[serde(default)]
        last_server_tick: u64,
    },
    /// Hitscan (Legacy/Compat): Server entscheidet mit Lag-Comp-Rewind
    Fire { id: u32, seq: u32, dir: [f32; 3], weapon: u8 },
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum GameEvent {
    Damage { from: u32, to: u32, dmg: u32, hit: [f32; 3] },
    Kill { killer: u32, victim: u32 },
    MatchEnd { winner: u8, score: [u32; 2] },
    Break { bi: u32 },
}

/// [Feuerrate s, Dmg nah, Dmg fern, Nah-Reichweite m]
pub const WEAPONS: [(f32, u32, u32, f32); 5] = [
    (0.11, 26, 18, 30.0),  // M16
    (0.14, 34, 22, 35.0),  // AK-47
    (0.075, 16, 9, 18.0),  // MP5
    (0.10, 24, 16, 40.0),  // MG4
    (0.80, 100, 60, 12.0), // PUMP
];
pub const WEAPON_NAMES: [&str; 5] = ["M16", "AK-47", "MP5", "MG4", "PUMP"];

pub const WIN_SCORE: u32 = 21;
pub const BTN_SPRINT: u8 = 1;
pub const BTN_FIRE: u8 = 2;

#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum ServerMsg {
    Welcome { id: u32, tick: u64, team: u8 },
    Snapshot {
        tick: u64,
        /// höchster pro Client verarbeiteter Input (Ack für Reconciliation)
        ack: u32,
        players: Vec<PlayerState>,
        events: Vec<GameEvent>,
        scores: [u32; 2],
    },
}

#[derive(Serialize, Deserialize, Clone, Debug, Default)]
pub struct PlayerState {
    pub id: u32,
    pub name: String,
    pub pos: [f32; 3],
    pub yaw: f32,
    pub last_seq: u32,
    pub hp: u32,
    pub kills: u32,
    pub team: u8,
}
