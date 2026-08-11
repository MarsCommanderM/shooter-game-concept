//! Stabile Identitäten — Milestone 02.
//! Keine Array-Positionen, keine Socket-Zuordnungen als Identität.

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, serde::Serialize, serde::Deserialize)]
pub struct EntityId(pub u32);

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, serde::Serialize, serde::Deserialize)]
pub struct ClientId(pub u32);

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, serde::Serialize, serde::Deserialize)]
pub struct MatchId(pub u64);

impl EntityId {
    /// Bitlayout: [generation:8 | index:24] — wiederverwendete Slots ändern die Generation
    pub fn new(gen: u8, index: u32) -> Self {
        EntityId(((gen as u32) << 24) | (index & 0x00FF_FFFF))
    }
    pub fn generation(self) -> u8 {
        (self.0 >> 24) as u8
    }
    pub fn index(self) -> u32 {
        self.0 & 0x00FF_FFFF
    }
}

/// Free-Liste-Allokator: vergibt Indizes, erhöht Generation bei Wiedervergabe.
pub struct IdAllocator {
    free: Vec<u32>,
    gens: Vec<u8>,
    next: u32,
}

impl IdAllocator {
    pub fn new() -> Self {
        Self { free: Vec::new(), gens: Vec::new(), next: 1 }
    }
    pub fn alloc(&mut self) -> EntityId {
        if let Some(idx) = self.free.pop() {
            self.gens[idx as usize] = self.gens[idx as usize].wrapping_add(1);
            EntityId::new(self.gens[idx as usize], idx)
        } else {
            let idx = self.next;
            self.next += 1;
            self.gens.resize(self.next as usize, 0);
            EntityId::new(0, idx)
        }
    }
    pub fn release(&mut self, id: EntityId) {
        self.free.push(id.index());
    }
}
