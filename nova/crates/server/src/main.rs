//! NOVA Dedicated Server — headless, keine GPU, 60 Hz autoritative Simulation.
//! Dual-Transport: UDP (native Clients) + WebSocket (Browser/WASM-Clients),
//! dasselbe Protokoll, dieselbe Autorität.
//! ./nova-server --port 27015 --tickrate 60 --max-players 16
use std::collections::{HashMap, VecDeque};
use std::net::{SocketAddr, TcpListener, UdpSocket};
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use tungstenite::accept;
use tungstenite::Message;

use nova_core::ecs::{self, World};
use nova_core::protocol::{ClientMsg, ServerMsg};
use nova_core::{FIXED_DT, SPRINT_SPEED, TICK_RATE, WALK_SPEED};

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
enum Conn {
    Udp(SocketAddr),
    Ws(u64),
}

enum NetEvent {
    Msg(ClientMsg),
    Bye,
}

struct Session {
    id: u32,
    name: String,
    entity: u32,
    conn: Conn,
    wish: [f32; 2],
    yaw: f32,
    sprint: bool,
    last_seq: u32,
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let port = arg(&args, "--port").unwrap_or(27015);
    let tickrate = arg(&args, "--tickrate").unwrap_or(TICK_RATE);
    let max_players = arg(&args, "--max-players").unwrap_or(16);
    let ws_port = port + 1;

    let sock = UdpSocket::bind(format!("0.0.0.0:{}", port)).expect("UDP-Bind fehlgeschlagen");
    sock.set_nonblocking(true).expect("nonblocking");
    let dt = 1.0 / tickrate as f32;

    // ---- WebSocket-Listener (Browser-Clients) ----
    let incoming: Arc<Mutex<VecDeque<(Conn, NetEvent)>>> = Arc::new(Mutex::new(VecDeque::new()));
    let ws_senders: Arc<Mutex<HashMap<u64, mpsc::Sender<String>>>> = Arc::new(Mutex::new(HashMap::new()));
    {
        let incoming = incoming.clone();
        let ws_senders = ws_senders.clone();
        std::thread::spawn(move || {
            let listener = TcpListener::bind(format!("0.0.0.0:{}", ws_port)).expect("WS-Bind");
            let counter = Arc::new(Mutex::new(1u64));
            println!("NOVA-WS online: port={}", ws_port);
            for stream in listener.incoming() {
                let Ok(stream) = stream else { continue };
                let Ok(mut ws) = accept(stream) else { continue };
                let id = { let mut c = counter.lock().unwrap(); let v = *c; *c += 1; v };
                let (tx, rx) = mpsc::channel::<String>();
                ws_senders.lock().unwrap().insert(id, tx);
                let incoming = incoming.clone();
                let ws_senders = ws_senders.clone();
                std::thread::spawn(move || {
                    let _ = ws.get_mut().set_read_timeout(Some(Duration::from_millis(5)));
                    loop {
                        // Outbox (Main-Loop -> Browser)
                        match rx.recv_timeout(Duration::from_millis(2)) {
                            Ok(text) => {
                                if ws.write_message(Message::Text(text)).is_err() {
                                    break;
                                }
                            }
                            Err(mpsc::RecvTimeoutError::Timeout) => {}
                            Err(_) => break,
                        }
                        // Inbox (Browser -> Main-Loop)
                        match ws.read_message() {
                            Ok(Message::Text(t)) => {
                                if let Ok(m) = serde_json::from_str::<ClientMsg>(&t) {
                                    incoming.lock().unwrap().push_back((Conn::Ws(id), NetEvent::Msg(m)));
                                }
                            }
                            Ok(Message::Ping(p)) => { let _ = ws.write_message(Message::Pong(p)); }
                            Ok(Message::Close(_)) => break,
                            Err(e) => {
                                let io_would = matches!(e, tungstenite::Error::Io(ref io) if io.kind() == std::io::ErrorKind::WouldBlock);
                                if !io_would { break; }
                            }
                            _ => {}
                        }
                    }
                    incoming.lock().unwrap().push_back((Conn::Ws(id), NetEvent::Bye));
                    ws_senders.lock().unwrap().remove(&id);
                });
            }
        });
    }

    let mut world = World::new();
    let mut sessions: HashMap<Conn, Session> = HashMap::new();
    let mut next_id = 1u32;
    let mut tick: u64 = 0;
    let mut last_stats = Instant::now();

    println!(
        "NOVA-SERVER online: udp={} ws={} tickrate={} max_players={} (headless, keine GPU nötig)",
        port, ws_port, tickrate, max_players
    );

    let mut buf = [0u8; 2048];
    let mut acc = Instant::now();

    let send = |conn: &Conn, text: String, ws_senders: &Arc<Mutex<HashMap<u64, mpsc::Sender<String>>>>, sock: &UdpSocket| {
        match conn {
            Conn::Udp(addr) => { let _ = sock.send_to(text.as_bytes(), addr); }
            Conn::Ws(id) => {
                if let Some(tx) = ws_senders.lock().unwrap().get(id) { let _ = tx.send(text); }
            }
        }
    };

    loop {
        let frame = Duration::from_secs_f32(dt);
        if acc.elapsed() < frame {
            std::thread::sleep(frame - acc.elapsed());
            continue;
        }
        acc = Instant::now();
        tick += 1;

        // ---- Inputs: UDP + WS-Queue ----
        let mut events: Vec<(Conn, NetEvent)> = Vec::new();
        while let Ok((n, from)) = sock.recv_from(&mut buf) {
            match serde_json::from_slice(&buf[..n]) {
                Ok(m) => events.push((Conn::Udp(from), NetEvent::Msg(m))),
                Err(_) => {}
            }
        }
        events.extend(incoming.lock().unwrap().drain(..));

        for (conn, ev) in events {
            match ev {
                NetEvent::Bye => {
                    if let Some(s) = sessions.remove(&conn) {
                        println!("[tick {}] - Spieler {} getrennt", tick, s.id);
                    }
                }
                NetEvent::Msg(msg) => match msg {
                    ClientMsg::Join { name } => {
                        if sessions.contains_key(&conn) { continue; }
                        if sessions.len() >= max_players as usize { continue; }
                        let id = next_id;
                        next_id += 1;
                        let entity = world.spawn(id, [0.0, 0.0, 0.0]);
                        sessions.insert(
                            conn,
                            Session { id, name: name.chars().take(12).collect(), entity, conn, wish: [0.0, 0.0], yaw: 0.0, sprint: false, last_seq: 0 },
                        );
                        let w = serde_json::to_string(&ServerMsg::Welcome { id, tick }).unwrap();
                        send(&conn, w, &ws_senders, &sock);
                        println!("[tick {}] + Spieler {} via {:?}", tick, id, conn);
                    }
                    ClientMsg::Input { id, seq, wish, yaw, sprint } => {
                        if let Some(s) = sessions.get_mut(&conn) {
                            if s.id != id { continue; }
                            let len = (wish[0] * wish[0] + wish[1] * wish[1]).sqrt();
                            s.wish = if len > 1.0 { [wish[0] / len, wish[1] / len] } else { wish };
                            s.yaw = yaw;
                            s.sprint = sprint;
                            if seq > s.last_seq { s.last_seq = seq; }
                        }
                    }
                },
            }
        }

        // ---- Simulation (autoritativ) ----
        for s in sessions.values_mut() {
            let speed = if s.sprint { SPRINT_SPEED } else { WALK_SPEED };
            let (sy, cy) = (s.yaw.sin(), s.yaw.cos());
            let vx = (s.wish[0] * cy - s.wish[1] * sy) * speed;
            let vz = (-s.wish[0] * sy - s.wish[1] * cy) * speed;
            if let Some((tr, ve, _)) = world.get_mut(s.entity) {
                tr.yaw = s.yaw;
                ve.v = [vx, 0.0, vz];
            }
        }
        ecs::movement_system(&mut world, dt);

        // ---- Snapshot replizieren (UDP + WS) ----
        let players: Vec<_> = sessions
            .values()
            .filter_map(|s| {
                world.transform.get(s.entity as usize)?.as_ref().map(|t| {
                    let name = s.name.clone();
                    nova_core::protocol::PlayerState { id: s.id, name, pos: t.pos, yaw: t.yaw, last_seq: s.last_seq }
                })
            })
            .collect();
        let snap = serde_json::to_string(&ServerMsg::Snapshot { tick, players }).unwrap();
        for (conn, _) in sessions.iter() {
            send(conn, snap.clone(), &ws_senders, &sock);
        }

        if last_stats.elapsed() >= Duration::from_secs(5) {
            last_stats = Instant::now();
            let ws_count = sessions.values().filter(|s| matches!(s.conn, Conn::Ws(_))).count();
            println!("[tick {}] {} Spieler ({} via WS), dt={:.3}ms", tick, sessions.len(), ws_count, FIXED_DT * 1000.0);
        }
    }
}

fn arg(args: &[String], key: &str) -> Option<u32> {
    args.windows(2).find(|w| w[0] == key).and_then(|w| w[1].parse().ok())
}
