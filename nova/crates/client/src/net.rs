//! NOVA-Netcode (Bauplan §7–§9): Client Prediction + Server-Reconciliation
//! + Snapshot-Interpolation. Transport austauschbar:
//! nativ = UDP, Browser/WASM = WebSocket (Port = UDP-Port + 1).
use std::collections::VecDeque;
use std::time::Duration;
use crate::timec::Instant;

use nova_core::protocol::{ClientMsg, GameEvent, PlayerState, ServerMsg};

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
    pub pitch: f32,
    pub sprint: bool,
    pub buttons: u8,
    pub predicted: [f32; 3],
}

pub struct Net {
    #[cfg(not(target_arch = "wasm32"))]
    sock: UdpSocket,
    #[cfg(target_arch = "wasm32")]
    ws: WebSocket,
    #[cfg(target_arch = "wasm32")]
    inbox: Rc<RefCell<VecDeque<Vec<u8>>>>,
    #[cfg(target_arch = "wasm32")]
    _onmsg: Closure<dyn FnMut(MessageEvent)>,
    pub id: u32,
    pub connected: bool,
    pending: VecDeque<PendingInput>,
    snaps: VecDeque<(Instant, Vec<PlayerState>)>,
    events: Vec<GameEvent>,
    my_hp: u32,
    my_team: u8,
    last_tick: u64,
    ack: u32,
    server_pos: Option<[f32; 3]>,
    scores: [u32; 2],
    names: Vec<(u32, String, u8)>,
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
            events: Vec::new(),
            my_hp: 100,
            my_team: 0,
            last_tick: 0,
            ack: 0,
            server_pos: None,
            scores: [0, 0],
            names: Vec::new(),
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
        ws.set_binary_type(web_sys::BinaryType::Arraybuffer);
        let inbox: Rc<RefCell<VecDeque<Vec<u8>>>> = Rc::new(RefCell::new(VecDeque::new()));
        let inbox2 = inbox.clone();
        let onmsg = Closure::wrap(Box::new(move |e: MessageEvent| {
            if let Ok(ab) = e.data().dyn_into::<js_sys::ArrayBuffer>() {
                let u8a = js_sys::Uint8Array::new(&ab);
                let mut v = vec![0u8; u8a.length() as usize];
                u8a.copy_to(&mut v);
                inbox2.borrow_mut().push_back(v);
            }
        }) as Box<dyn FnMut(MessageEvent)>);
        ws.set_onmessage(Some(onmsg.as_ref().unchecked_ref()));
        // Join erst nach connect schicken
        let ws2 = ws.clone();
        let onopen = Closure::wrap(Box::new(move |_: web_sys::Event| {
            if let Ok(v) = bincode::serialize(&ClientMsg::Join { name: "NOVA-PILOT".into() }) {
                let arr = js_sys::Uint8Array::from(&v[..]);
                let _ = ws2.send_with_array_buffer(&arr.buffer());
            }
        }) as Box<dyn FnMut(web_sys::Event)>);
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
            events: Vec::new(),
            my_hp: 100,
            my_team: 0,
            last_tick: 0,
            ack: 0,
            server_pos: None,
            scores: [0, 0],
            names: Vec::new(),
        })
    }

    fn send(&self, m: &ClientMsg) {
        let Ok(v) = bincode::serialize(m) else { return };
        #[cfg(not(target_arch = "wasm32"))]
        {
            let _ = self.sock.send(&v);
        }
        #[cfg(target_arch = "wasm32")]
        {
            if self.ws.ready_state() == WebSocket::OPEN {
                let arr = js_sys::Uint8Array::from(&v[..]);
                let _ = self.ws.send_with_array_buffer(&arr.buffer());
            }
        }
    }

    pub fn send_input(&mut self, p: PendingInput) {
        let msg = ClientMsg::Input {
            id: self.id, seq: p.seq, wish: p.wish, yaw: p.yaw, pitch: p.pitch,
            sprint: p.sprint, buttons: p.buttons, last_server_tick: self.last_tick,
        };
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
            return bincode::deserialize(&buf[..n]).ok();
        }
        #[cfg(target_arch = "wasm32")]
        {
            let v = self.inbox.borrow_mut().pop_front()?;
            return bincode::deserialize(&v).ok();
        }
    }

    /// Socket pollen; liefert ggf. Reconciliation-Korrektur:
    /// (autoritative Server-Position, offene Inputs zum Re-Simulieren)
    pub fn poll(&mut self) -> Option<([f32; 3], Vec<PendingInput>)> {
        let mut correction = None;
        while let Some(msg) = self.recv_next() {
            match msg {
                ServerMsg::Welcome { id, team, .. } => {
                    self.id = id;
                    self.my_team = team;
                    self.connected = true;
                }
                ServerMsg::Snapshot { players, events, scores, .. } => {
                    self.scores = scores;
                    self.events.extend(events);
                    if let Some(me) = players.iter().find(|p| p.id == self.id) {
                        self.my_hp = me.hp;
                    }
                    self.names = players.iter().map(|p| (p.id, p.name.clone(), p.team)).collect();
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

    pub fn send_fire(&mut self, seq: u32, dir: [f32; 3], weapon: u8) {
        self.send(&ClientMsg::Fire { id: self.id, seq, dir, weapon });
    }

    pub fn drain_events(&mut self) -> Vec<GameEvent> {
        std::mem::take(&mut self.events)
    }

    pub fn my_hp(&self) -> u32 {
        self.my_hp
    }

    pub fn name_of(&self, id: u32) -> String {
        self.names.iter().find(|(i, _, _)| *i == id).map(|(_, n, _)| n.clone()).unwrap_or_else(|| format!("#{}", id))
    }

    pub fn team_of(&self, id: u32) -> u8 {
        self.names.iter().find(|(i, _, _)| *i == id).map(|(_, _, t)| *t).unwrap_or(1)
    }

    pub fn my_team(&self) -> u8 {
        self.my_team
    }

    pub fn take_server_pos(&mut self) -> Option<([f32; 3], u32)> {
        self.server_pos.take().map(|p| (p, self.ack))
    }

    pub fn scores(&self) -> [u32; 2] {
        self.scores
    }

    /// Snapshot-Interpolation (§9): Render ~100 ms in der Vergangenheit,
    /// lerp zwischen den Snapshots, die den Zeitpunkt umspannen.
    pub fn remotes(&self, delay: Duration) -> Vec<(u32, [f32; 3], f32, u8)> {
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
            if pb.id == self.id || pb.hp == 0 {
                continue;
            }
            match a.1.iter().find(|p| p.id == pb.id) {
                Some(pa) => out.push((pb.id, lerp3(pa.pos, pb.pos, f), pa.yaw + (pb.yaw - pa.yaw) * f, pb.team)),
                None => out.push((pb.id, pb.pos, pb.yaw, pb.team)),
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
