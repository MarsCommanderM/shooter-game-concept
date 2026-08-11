//! NOVA Dedicated Server — headless, keine GPU, 60 Hz autoritative Simulation.
//! Läuft auf dem Contabo-VPS: ./nova-server --port 27015 --tickrate 60 --max-players 16
use std::collections::HashMap;
use std::net::{SocketAddr, UdpSocket};
use std::time::{Duration, Instant};

use nova_core::ecs::{self, World};
use nova_core::protocol::{ClientMsg, ServerMsg};
use nova_core::{FIXED_DT, SPRINT_SPEED, TICK_RATE, WALK_SPEED};

struct Session {
    id: u32,
    name: String,
    entity: u32,
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

    let sock = UdpSocket::bind(format!("0.0.0.0:{}", port)).expect("UDP-Bind fehlgeschlagen");
    sock.set_nonblocking(true).expect("nonblocking");
    let dt = 1.0 / tickrate as f32;

    let mut world = World::new();
    let mut sessions: HashMap<SocketAddr, Session> = HashMap::new();
    let mut next_id = 1u32;
    let mut tick: u64 = 0;
    let mut last_stats = Instant::now();

    println!(
        "NOVA-SERVER online: port={} tickrate={} max_players={} (headless, keine GPU nötig)",
        port, tickrate, max_players
    );

    let mut buf = [0u8; 2048];
    let mut acc = Instant::now();

    loop {
        // ---- fixed timestep ----
        let frame = Duration::from_secs_f32(dt);
        if acc.elapsed() < frame {
            std::thread::sleep(frame - acc.elapsed());
            continue;
        }
        acc = Instant::now();
        tick += 1;

        // ---- Inputs empfangen ----
        while let Ok((n, from)) = sock.recv_from(&mut buf) {
            let msg: ClientMsg = match serde_json::from_slice(&buf[..n]) {
                Ok(m) => m,
                Err(_) => continue,
            };
            match msg {
                ClientMsg::Join { name } => {
                    if sessions.len() >= max_players as usize {
                        continue;
                    }
                    let id = next_id;
                    next_id += 1;
                    let entity = world.spawn(id, [0.0, 0.0, 0.0]);
                    sessions.insert(
                        from,
                        Session { id, name: name.chars().take(12).collect(), entity, wish: [0.0, 0.0], yaw: 0.0, sprint: false, last_seq: 0 },
                    );
                    let w = ServerMsg::Welcome { id, tick };
                    let _ = sock.send_to(serde_json::to_string(&w).unwrap().as_bytes(), from);
                    println!("[tick {}] + Spieler {} ({}) aus {}", tick, id, from, sessions[&from].name);
                }
                ClientMsg::Input { id, seq, wish, yaw, sprint } => {
                    if let Some(s) = sessions.get_mut(&from) {
                        if s.id != id {
                            continue;
                        }
                        // Anti-Cheat: Wish-Vektor clampen (max. normiert)
                        let len = (wish[0] * wish[0] + wish[1] * wish[1]).sqrt();
                        s.wish = if len > 1.0 { [wish[0] / len, wish[1] / len] } else { wish };
                        s.yaw = yaw;
                        s.sprint = sprint;
                        if seq > s.last_seq {
                            s.last_seq = seq;
                        }
                    }
                }
            }
        }

        // ---- Simulation: Inputs -> Velocity (server-autoritativ) ----
        for s in sessions.values_mut() {
            let speed = if s.sprint { SPRINT_SPEED } else { WALK_SPEED };
            let (sy, cy) = (s.yaw.sin(), s.yaw.cos());
            // wish[0]=strafe(rechts), wish[1]=vor — identisch zur Client-Konvention
            let vx = (s.wish[0] * cy - s.wish[1] * sy) * speed;
            let vz = (-s.wish[0] * sy - s.wish[1] * cy) * speed;
            if let Some((tr, ve, _)) = world.get_mut(s.entity) {
                tr.yaw = s.yaw;
                ve.v = [vx, 0.0, vz];
            }
        }
        ecs::movement_system(&mut world, dt);

        // ---- Snapshot bauen & replizieren ----
        let players: Vec<_> = sessions
            .values()
            .filter_map(|s| {
                world.transform.get(s.entity as usize)?.as_ref().map(|t| {
                    let name = s.name.clone();
                    nova_core::protocol::PlayerState { id: s.id, name, pos: t.pos, yaw: t.yaw, last_seq: s.last_seq }
                })
            })
            .collect();
        let snap = ServerMsg::Snapshot { tick, players };
        let data = serde_json::to_string(&snap).unwrap();
        for addr in sessions.keys() {
            let _ = sock.send_to(data.as_bytes(), addr);
        }

        if last_stats.elapsed() >= Duration::from_secs(5) {
            last_stats = Instant::now();
            println!("[tick {}] {} Spieler online, dt={:.3}ms", tick, sessions.len(), FIXED_DT * 1000.0);
        }
    }
}

fn arg(args: &[String], key: &str) -> Option<u32> {
    args.windows(2)
        .find(|w| w[0] == key)
        .and_then(|w| w[1].parse().ok())
}
