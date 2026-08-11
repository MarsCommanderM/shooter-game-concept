//! NOVA-Netcode (Bauplan §7–§9): Client Prediction + Server-Reconciliation
//! + Snapshot-Interpolation für Remote-Player.
use std::collections::VecDeque;
use std::net::UdpSocket;
use std::time::{Duration, Instant};

use nova_core::protocol::{ClientMsg, PlayerState, ServerMsg};

#[derive(Clone)]
pub struct PendingInput {
    pub seq: u32,
    pub wish: [f32; 2], // [strafe, vor]
    pub yaw: f32,
    pub sprint: bool,
    pub predicted: [f32; 3],
}

pub struct Net {
    sock: UdpSocket,
    pub id: u32,
    pub connected: bool,
    pending: VecDeque<PendingInput>,
    snaps: VecDeque<(Instant, Vec<PlayerState>)>,
}

impl Net {
    pub fn connect(addr: &str) -> std::io::Result<Self> {
        let sock = UdpSocket::bind("0.0.0.0:0")?;
        sock.set_nonblocking(true)?;
        sock.connect(addr)?;
        let n = Net {
            sock,
            id: 0,
            connected: false,
            pending: VecDeque::new(),
            snaps: VecDeque::new(),
        };
        n.send(&ClientMsg::Join { name: "NOVA-PILOT".into() });
        Ok(n)
    }

    fn send(&self, m: &ClientMsg) {
        if let Ok(s) = serde_json::to_string(m) {
            let _ = self.sock.send(s.as_bytes());
        }
    }

    pub fn send_input(&mut self, p: PendingInput) {
        let msg = ClientMsg::Input { id: self.id, seq: p.seq, wish: p.wish, yaw: p.yaw, sprint: p.sprint };
        self.pending.push_back(p);
        if self.pending.len() > 128 {
            self.pending.pop_front();
        }
        self.send(&msg);
    }

    /// Socket pollen; liefert ggf. Reconciliation-Korrektur:
    /// (autoritative Server-Position, Liste der noch offenen Inputs zum Re-Simulieren)
    pub fn poll(&mut self) -> Option<([f32; 3], Vec<PendingInput>)> {
        let mut buf = [0u8; 8192];
        let mut correction = None;
        while let Ok(n) = self.sock.recv(&mut buf) {
            match serde_json::from_slice(&buf[..n]) {
                Ok(ServerMsg::Welcome { id, .. }) => {
                    self.id = id;
                    self.connected = true;
                }
                Ok(ServerMsg::Snapshot { players, .. }) => {
                    self.snaps.push_back((Instant::now(), players));
                    if self.snaps.len() > 90 {
                        self.snaps.pop_front();
                    }
                }
                Err(_) => continue,
            }
        }
        // Reconciliation gegen freshest Snapshot (lastProcessedInput-Prinzip)
        if let Some((_, players)) = self.snaps.back() {
            if let Some(me) = players.iter().find(|p| p.id == self.id) {
                let last = me.last_seq;
                let mut popped: Option<PendingInput> = None;
                while let Some(front) = self.pending.front() {
                    if front.seq <= last {
                        popped = self.pending.pop_front();
                    } else {
                        break;
                    }
                }
                if let Some(p) = popped {
                    if dist3(p.predicted, me.pos) > 0.25 {
                        correction = Some((me.pos, self.pending.iter().cloned().collect()));
                    }
                }
            }
        }
        correction
    }

    /// Snapshot-Interpolation (§9): Render ~100 ms in der Vergangenheit,
    /// lerp zwischen den beiden Snapshots, die diesen Zeitpunkt umspannen.
    pub fn remotes(&self, delay: Duration) -> Vec<(u32, [f32; 3], f32)> {
        let t = Instant::now().checked_sub(delay).unwrap_or_else(Instant::now);
        let snaps: Vec<&(Instant, Vec<PlayerState>)> = self.snaps.iter().collect();
        if snaps.len() < 2 {
            return Vec::new();
        }
        let mut pair = None;
        for i in 0..snaps.len() - 1 {
            if snaps[i].0 <= t && t <= snaps[i + 1].0 {
                pair = Some(i);
                break;
            }
        }
        let (a, b) = match pair {
            Some(i) => (snaps[i], snaps[i + 1]),
            None => {
                if t < snaps[0].0 {
                    return Vec::new();
                }
                let l = snaps.len();
                (snaps[l - 2], snaps[l - 1])
            }
        };
        let span = (b.0 - a.0).as_secs_f32().max(1e-4);
        let f = ((t - a.0).as_secs_f32() / span).clamp(0.0, 1.0);
        let mut out = Vec::new();
        for pb in &b.1 {
            if pb.id == self.id {
                continue;
            }
            match a.1.iter().find(|p| p.id == pb.id) {
                Some(pa) => out.push((pb.id, lerp3(pa.pos, pb.pos, f), pa.yaw + (pb.yaw - pa.yaw) * f)),
                None => out.push((pb.id, pb.pos, pb.yaw)),
            }
        }
        out
    }
}

fn dist3(a: [f32; 3], b: [f32; 3]) -> f32 {
    ((a[0] - b[0]).powi(2) + (a[1] - b[1]).powi(2) + (a[2] - b[2]).powi(2)).sqrt()
}
fn lerp3(a: [f32; 3], b: [f32; 3], f: f32) -> [f32; 3] {
    [a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, a[2] + (b[2] - a[2]) * f]
}
