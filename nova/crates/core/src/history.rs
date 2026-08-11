//! TransformHistory für Lag Compensation — capped, rewinds nur Hitboxes.

#[derive(Clone, Copy, Debug)]
pub struct TransformSample {
    pub tick: u64,
    pub pos: [f32; 3],
    pub yaw: f32,
}

pub struct TransformHistory {
    samples: std::collections::VecDeque<TransformSample>,
    cap: usize,
}

impl TransformHistory {
    pub fn new(cap: usize) -> Self {
        Self { samples: std::collections::VecDeque::new(), cap }
    }
    pub fn push(&mut self, s: TransformSample) {
        self.samples.push_back(s);
        if self.samples.len() > self.cap {
            self.samples.pop_front();
        }
    }
    /// Position zum (oder nächstgelegenen) Tick — für Rewind-Raycast.
    pub fn at(&self, tick: u64) -> Option<TransformSample> {
        self.samples.iter().find(|s| s.tick >= tick).copied()
            .or_else(|| self.samples.back().copied())
    }
}
