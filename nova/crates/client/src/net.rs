//! NOVA-Netcode (Bauplan §7–§9): Client Prediction + Server-Reconciliation
//! + Snapshot-Interpolation. Transport austauschbar:
//! nativ = UDP, Browser/WASM = WebSocket (Port = UDP-Port + 1).
use std::collections::VecDeque;
use std::time::{Duration, Instant};

use nova_core::protocol::{ClientMsg, PlayerState, ServerMsg};

#[cfg(not(target_arch = "wasm32"))]
use std::net::UdpSocket;

#[cfg(target_arch = "wasm32")]
use std::cell::RefCell;
#[cfg(target_arch = "wasm32")]
use std::rc::Rc;
#[cfg(target_arch = "wasm32")]
use wasm_bindgen::prelude::*;
#[cfg(target_arch = "wasm32")]
use wasm_bindgen::JsCast;
#[cfg(target_arch = "wasm32")]
use web_sys::{MessageEvent, WebSocket};

#[derive(Clone)]
pub struct PendingInput {
    pub seq: u32,
    pub wish: [f32; 2], // [strafe, vor]
    pub yaw: f32,
    pub sprint: bool,
    pub predicted: [f32; 3],
}

pub struct Net {
    #[cfg(not(target_arch = "wasm32"))]
    sock: UdpSocket,
    #[cfg(target_arch = "wasm32")]
    ws: WebSocket,
    #[cfg(target_arch = "wasm32")]
    inbox: Rc<RefCell<VecDeque<String>>>,
    #[cfg(target_arch = "wasm32")]
    _onmsg: Closure<dyn FnMut(MessageEvent)>,
    pub id: u32,
    pub connected: bool,
    pending: VecDeque<PendingInput>,
    snaps: VecDeque<(Instant, Vec<PlayerState>)>,
}

impl Net {
    #[cfg(not(target_arch = "wasm32"))]
    pub fn connect(addr: &str) -> Result<Self, String> {
        let sock = UdpSocket::bind("0.0.0.0:0").map_err(|e| e.to_string())?;
        sock.set_nonblocking(true).map_err(|e| e.to_string())?;
        sock.connect(addr).map_err(|e| e.to_string())?;
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

    #[cfg(target_arch = "wasm32")]
    pub fn connect(addr: &str) -> Result<Self, String> {
        let (host, p) = addr.rsplit_once(':').ok_or("Adresse ohne Port")?;
        let wport: u32 = p.parse::<u32>().map_err(|_| "Port ungültig")? + 1;
        let url = format!("ws://{}:{}", host, wport);
        let ws = WebSocket::new(&url).map_err(|_| "WebSocket fehlgeschlagen")?;
        let inbox: Rc<RefCell<VecDeque<String>>> = Rc::new(RefCell::new(VecDeque::new()));
        let inbox2 = inbox.clone();
        let onmsg = Closure::wrap(Box::new(move |e: MessageEvent| {
            if let Some(txt) = e.data().as_string() {
                inbox2.borrow_mut().push_back(txt);
            }
        }) as Box<dyn FnMut(MessageEvent)>);
        ws.set_onmessage(Some(onmsg.as_ref().unchecked_ref()));
        // Join erst nach connect schicken
        let ws2 = ws.clone();
        let onopen = Closure::wrap(Box::new(move |_| {
            if let Ok(s) = serde_json::to_string(&ClientMsg::Join { name: "NOVA-PILOT".into() }) {
                let _ = ws2.send_with_str(&s);
            }
        }) as Box<dyn FnMut(MessageEvent)>);
        ws.set_onopen(Some(onopen.as_ref().unchecked_ref()));
        onopen.forget(); // lebt solange die Verbindung
        Ok(Net {
            ws,
            inbox,
            _onmsg: onmsg,
            id: 0,
            connected: false,
            pending: VecDeque::new(),
            snaps: VecDeque::new(),
        })
    }

    fn send(&self, m: &ClientMsg) {
        let Ok(s) = serde_json::to_string(m) else { return };
        #[cfg(not(target_arch = "wasm32"))]
        {
            let _ = self.sock.send(s.as_bytes());
        }
        #[cfg(target_arch = "wasm32")]
        {
            if self.ws.ready_state() == WebSocket::OPEN {
                let _ = self.ws.send_with_str(&s);
            }
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

    fn recv_next(&mut self) -> Option<ServerMsg> {
        #[cfg(not(target_arch = "wasm32"))]
        {
            let mut buf = [0u8; 8192];
            let n = self.sock.recv(&mut buf).ok()?;
            return serde_json::from_slice(&buf[..n]).ok();
        }
        #[cfg(target_arch = "wasm32")]
        {
            let t = self.inbox.borrow_mut().pop_front()?;
            return serde_json::from_str(&t).ok();
        }
    }

    /// Socket pollen; liefert ggf. Reconciliation-Korrektur:
    /// (autoritative Server-Position, offene Inputs zum Re-Simulieren)
    pub fn poll(&mut self) -> Option<([f32; 3], Vec<PendingInput>)> {
        let mut correction = None;
        while let Some(msg) = self.recv_next() {
            match msg {
                ServerMsg::Welcome { id, .. } => {
                    self.id = id;
                    self.connected = true;
                }
                ServerMsg::Snapshot { players, .. } => {
                    self.snaps.push_back((Instant::now(), players));
                    if self.snaps.len() > 90 {
                        self.snaps.pop_front();
                    }
                }
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
    /// lerp zwischen den Snapshots, die den Zeitpunkt umspannen.
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
