//! Netzwerkprotokoll (JSON über UDP für Milestone 01, später binär/seq-basiert).
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum ClientMsg {
    Join { name: String },
    /// Input mit Sequence-Number für Server-Reconciliation (Bauplan §8)
    Input { id: u32, seq: u32, wish: [f32; 2], yaw: f32, sprint: bool },
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum ServerMsg {
    Welcome { id: u32, tick: u64 },
    Snapshot { tick: u64, players: Vec<PlayerState> },
}

#[derive(Serialize, Deserialize, Clone, Debug, Default)]
pub struct PlayerState {
    pub id: u32,
    pub name: String,
    pub pos: [f32; 3],
    pub yaw: f32,
    pub last_seq: u32,
}
