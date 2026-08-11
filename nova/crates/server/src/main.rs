//! NOVA Dedicated Server — headless, 60 Hz autoritativ, Dual-Transport (UDP + WS).
//! Combat: Hitscan mit Lag-Compensation (Position-Rewind ~150 ms),
//! serverseitige Occlusion gegen die gemeinsame Map (keine Wall-Bangs).
use std::collections::{HashMap, VecDeque};
use std::net::{SocketAddr, TcpListener, UdpSocket};
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use tungstenite::accept_hdr;
use tungstenite::Message;

use nova_core::ecs::{self, World};
use nova_core::maps;
use nova_core::protocol::{ClientMsg, GameEvent, ServerMsg};
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

const EYE: f32 = 1.5;
// Milestone 02: Rewind = RTT/2 + Interp-Delay; Rates/Damage aus nova_core::weapons
const INTERP_TICKS: u64 = 6; // ~100 ms Snapshot-Interpolation des Clients
const RESPAWN_TICKS: u64 = 180; // 3 s

struct Session {
    id: u32,
    name: String,
    entity: u32,
    conn: Conn,
    wish: [f32; 2],
    yaw: f32,
    sprint: bool,
    last_seq: u32,
    hp: u32,
    kills: u32,
    dead_until: u64,
    last_fire: u64,
    spawn_idx: usize,
    team: u8,
    buttons: u8,
    pitch: f32,
    weapon: u8,
    last_stick: u64,
    sim: nova_core::sim::PlayerSim,
    hist: nova_core::history::TransformHistory,
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let port = arg(&args, "--port").unwrap_or(27015);
    let tickrate = arg(&args, "--tickrate").unwrap_or(TICK_RATE);
    let max_players = arg(&args, "--max-players").unwrap_or(16);
    let ws_port = port + 1;

    let arena = maps::shard_city_lite();
    // Nur solide Boxen für Occlusion (Neon=1 & Pad=4 nicht solide); Kisten=zerstörbar
    let mut broken: Vec<bool> = vec![false; arena.len()];
    let is_solid = |b: &maps::BoxDef, br: bool| b.kind != 1 && b.kind != 4 && !br;

    let sock = UdpSocket::bind(format!("0.0.0.0:{}", port)).expect("UDP-Bind");
    sock.set_nonblocking(true).expect("nonblocking");
    let dt = 1.0 / tickrate as f32;

    // ---- WebSocket-Listener ----
    let ws_enc: Arc<Mutex<HashMap<u64, bool>>> = Arc::new(Mutex::new(HashMap::new()));
    let incoming: Arc<Mutex<VecDeque<(Conn, NetEvent)>>> = Arc::new(Mutex::new(VecDeque::new()));
    let ws_senders: Arc<Mutex<HashMap<u64, mpsc::Sender<Vec<u8>>>>> = Arc::new(Mutex::new(HashMap::new()));
    {
        let incoming = incoming.clone();
        let ws_senders = ws_senders.clone();
        let ws_enc = ws_enc.clone();
        std::thread::spawn(move || {
            let listener = TcpListener::bind(format!("0.0.0.0:{}", ws_port)).expect("WS-Bind");
            let counter = Arc::new(Mutex::new(1u64));
            println!("NOVA-WS online: port={}", ws_port);
            for stream in listener.incoming() {
                let Ok(stream) = stream else { continue };
                let use_json = std::sync::Arc::new(std::sync::Mutex::new(false));
                let uj = use_json.clone();
                let Ok(mut ws) = accept_hdr(stream, |req: &tungstenite::handshake::server::Request, res: tungstenite::handshake::server::Response| {
                    let q = req.uri().query().unwrap_or("");
                    *uj.lock().unwrap() = q.contains("enc=json");
                    Ok(res)
                }) else { continue };
                let use_json = *use_json.lock().unwrap();
                let id = { let mut c = counter.lock().unwrap(); let v = *c; *c += 1; v };
                ws_enc.lock().unwrap().insert(id, use_json);
                let (tx, rx) = mpsc::channel::<Vec<u8>>();
                ws_senders.lock().unwrap().insert(id, tx);
                let incoming = incoming.clone();
                let ws_senders = ws_senders.clone();
                std::thread::spawn(move || {
                    let _ = ws.get_mut().set_read_timeout(Some(Duration::from_millis(5)));
                    loop {
                        match rx.recv_timeout(Duration::from_millis(2)) {
                            Ok(bytes) => { if ws.write_message(Message::Binary(bytes)).is_err() { break; } }
                            Err(mpsc::RecvTimeoutError::Timeout) => {}
                            Err(_) => break,
                        }
                        match ws.read_message() {
                            Ok(Message::Text(t)) if use_json => {
                                if let Ok(m) = serde_json::from_str::<ClientMsg>(&t) {
                                    incoming.lock().unwrap().push_back((Conn::Ws(id), NetEvent::Msg(m)));
                                }
                            }
                            Ok(Message::Binary(v)) => {
                                if let Ok(m) = bincode::deserialize::<ClientMsg>(&v) {
                                    incoming.lock().unwrap().push_back((Conn::Ws(id), NetEvent::Msg(m)));
                                }
                            }
                            Ok(Message::Ping(p)) => { let _ = ws.write_message(Message::Pong(p)); }
                            Ok(Message::Close(_)) => break,
                            Err(e) => {
                                let would = matches!(e, tungstenite::Error::Io(ref io) if io.kind() == std::io::ErrorKind::WouldBlock);
                                if !would { break; }
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

    println!("NOVA-SERVER online: udp={} ws={} tick={} players_max={} map=shard_city_lite (headless)", port, ws_port, tickrate, max_players);

    let mut buf = [0u8; 2048];
    let mut acc = Instant::now();
    let mut events: Vec<GameEvent> = Vec::new();
    let mut scores = [0u32; 2];
    let mut crate_hp: Vec<f32> = arena.iter().map(|b| if b.kind == 2 { 100.0 } else { 0.0 }).collect();

    loop {
        let frame = Duration::from_secs_f32(dt);
        if acc.elapsed() < frame {
            std::thread::sleep(frame - acc.elapsed());
            continue;
        }
        acc = Instant::now();
        tick += 1;
        events.clear();

        // ---- Inputs ----
        let mut net_events: Vec<(Conn, NetEvent)> = Vec::new();
        while let Ok((n, from)) = sock.recv_from(&mut buf) {
            if let Ok(m) = bincode::deserialize::<ClientMsg>(&buf[..n]) {
                net_events.push((Conn::Udp(from), NetEvent::Msg(m)));
            }
        }
        net_events.extend(incoming.lock().unwrap().drain(..));

        for (conn, ev) in net_events {
            match ev {
                NetEvent::Bye => { if let Some(s) = sessions.remove(&conn) { println!("[{}] - {} left", tick, s.name); } }
                NetEvent::Msg(msg) => match msg {
                    ClientMsg::Join { name } => {
                        if sessions.contains_key(&conn) || sessions.len() >= max_players as usize { continue; }
                        let id = next_id; next_id += 1;
                        let (mut c0, mut c1) = (0u32, 0u32);
                        for ss in sessions.values() { if ss.team == 0 { c0 += 1 } else { c1 += 1 } }
                        let team: u8 = if c0 <= c1 { 0 } else { 1 };
                        // Team-Spawns: Team 0 = +X (seitlich), Team 1 = -X
                        let spawn_idx = if team == 0 { if c0 % 2 == 0 { 0 } else { 1 } } else { if c1 % 2 == 0 { 2 } else { 3 } };
                        let sp = maps::SPAWNS[spawn_idx];
                        let entity = world.spawn(id, sp);
                        let sim0 = nova_core::sim::PlayerSim { pos: sp, vel: [0.0, 0.0, 0.0], yaw: if (id % 2) == 0 { std::f32::consts::FRAC_PI_2 } else { -std::f32::consts::FRAC_PI_2 }, pitch: 0.0 };
                        sessions.insert(conn, Session {
                            id, name: name.chars().take(12).collect(), entity, conn,
                            wish: [0.0, 0.0], yaw: if team == 0 { std::f32::consts::FRAC_PI_2 } else { -std::f32::consts::FRAC_PI_2 },
                            sprint: false, last_seq: 0,
                            hp: 100, kills: 0, dead_until: 0, last_fire: 0, spawn_idx, team, buttons: 0, pitch: 0.0, last_stick: 0, weapon: 0,
                            sim: sim0, hist: nova_core::history::TransformHistory::new(90),
                        });
                        let w_bin = bincode::serialize(&ServerMsg::Welcome { id, tick, team }).unwrap();
                        let w_json = serde_json::to_string(&ServerMsg::Welcome { id, tick, team }).unwrap();
                        let cj = ws_enc.lock().unwrap().get(&match conn { Conn::Ws(i) => i, _ => 0 }).copied().unwrap_or(false);
                        send(&conn, w_bin, w_json, cj, &ws_senders, &sock);
                        println!("[{}] + {} team={} via {:?}", tick, id, team, conn);
                    }
                    ClientMsg::Input { id, seq, wish, yaw, pitch, sprint, buttons, last_server_tick } => {
                        let fire_req2 = {
                        if let Some(s) = sessions.get_mut(&conn) {
                            if s.id != id { None } else {
                            let len = (wish[0] * wish[0] + wish[1] * wish[1]).sqrt();
                            s.wish = if len > 1.0 { [wish[0] / len, wish[1] / len] } else { wish };
                            s.yaw = yaw; s.pitch = pitch;
                            s.sprint = sprint || (buttons & nova_core::protocol::BTN_SPRINT) != 0;
                            s.last_stick = last_server_tick;
                            // FIRE als Command-Button mit serverseitiger Rate
                            let fire_now = (buttons & nova_core::protocol::BTN_FIRE) != 0;
                            let was_fire = (s.buttons & nova_core::protocol::BTN_FIRE) != 0;
                            s.buttons = buttons;
                            if seq > s.last_seq { s.last_seq = seq; }
                            if (buttons & nova_core::protocol::BTN_FIRE) != 0 {
                                // dir serverseitig aus yaw/pitch (keine клиентen-dir nötig)
                                let fire_dir = {
                                    let (sy, cy) = (yaw.sin(), yaw.cos());
                                    let (sp, cp) = (pitch.sin(), pitch.cos());
                                    [-sy * cp, sp, -cy * cp]
                                };
                                let rtt = tick.saturating_sub(last_server_tick).min(60);
                                Some((fire_dir, rtt / 2 + INTERP_TICKS, s.weapon))
                            } else { None }
                                }
                            } else { None }
                        };
                        if let Some((dir, rewind, wp)) = fire_req2 {
                            if let Some(evts) = handle_fire(&mut sessions, &world, &arena, &mut broken, &mut crate_hp, is_solid, id, conn, dir, tick, &mut scores, wp, rewind) {
                                events.extend(evts);
                            }
                        }
                    }
                    ClientMsg::Fire { id, seq, dir, weapon } => {
                        if let Some(s) = sessions.get_mut(&conn) { s.weapon = weapon.min(4); if seq > s.last_seq { s.last_seq = seq; } }
                        let s2 = sessions.get(&conn);
                        let rtt = s2.map(|s| tick.saturating_sub(s.last_stick).min(60)).unwrap_or(12);
                        let rewind = rtt / 2 + INTERP_TICKS;
                        let hit = handle_fire(&mut sessions, &world, &arena, &mut broken, &mut crate_hp, is_solid, id, conn, dir, tick, &mut scores, weapon.min(4), rewind);
                        if let Some(evts) = hit { events.extend(evts); }
                    }
                },
            }
        }

        // ---- Simulation: shared sim::step = Authority (client prediction identisch) ----
        let alive_before: Vec<(u32, u8, [f32; 3])> = sessions.values().filter(|o| o.hp > 0).map(|o| (o.id, o.team, o.sim.pos)).collect();
        for s in sessions.values_mut() {
            if s.hp == 0 {
                if tick >= s.dead_until {
                    s.hp = 100;
                    // Spawn-Sicherheitsbewertung: Team-Spawn mit max. Min-Distanz zu Feinden
                    let team_spawns: [usize; 2] = if s.team == 0 { [0, 1] } else { [2, 3] };
                    let mut best_sp = maps::SPAWNS[s.spawn_idx];
                    let mut best_score = -1.0f32;
                    for si in team_spawns {
                        let sp = maps::SPAWNS[si];
                        let min_enemy = alive_before
                            .iter()
                            .filter(|(oid, oteam, _)| *oid != s.id && *oteam != s.team)
                            .map(|(_, _, op)| ((op[0] - sp[0]).powi(2) + (op[2] - sp[2]).powi(2)).sqrt())
                            .fold(999.0f32, f32::min);
                        if min_enemy > best_score { best_score = min_enemy; best_sp = sp; }
                    }
                    s.sim.pos = best_sp;
                    s.sim.vel = [0.0, 0.0, 0.0];
                    s.sim.yaw = if s.team == 0 { std::f32::consts::FRAC_PI_2 } else { -std::f32::consts::FRAC_PI_2 };
                } else {
                    continue;
                }
            }
            let inp = nova_core::sim::InputSample { wish: s.wish, sprint: s.sprint };
            nova_core::sim::step(&mut s.sim, &inp, dt, &arena);
        }
        // Spieler-Collision: sanft auseinander (sim kennt keine anderen Spieler)
        let poss: Vec<(Conn, [f32; 3], bool)> = sessions.iter().map(|(c, s)| (*c, s.sim.pos, s.hp > 0)).collect();
        for i in 0..poss.len() {
            for j in (i + 1)..poss.len() {
                if !poss[i].2 || !poss[j].2 { continue; }
                let (ci, pi, _) = poss[i];
                let (cj, pj, _) = poss[j];
                let dx = pi[0] - pj[0];
                let dz = pi[2] - pj[2];
                let d2 = (dx * dx + dz * dz).sqrt();
                if d2 < 0.9 && d2 > 1e-4 {
                    let push = (0.9 - d2) * 0.5;
                    let nx = dx / d2;
                    let nz = dz / d2;
                    for (c, sx, sz) in [(ci, nx, nz), (cj, -nx, -nz)] {
                        if let Some(s) = sessions.get_mut(&c) {
                            s.sim.pos[0] += sx * push;
                            s.sim.pos[2] += sz * push;
                        }
                    }
                }
            }
        }

        // Spieler-Collision: sanft auseinander schieben (keine Menschen-Wände)
        let poss: Vec<(Conn, [f32; 3], bool)> = sessions.iter().map(|(c, s)| {
            let p = world.transform.get(s.entity as usize).and_then(|t| t.as_ref()).map(|t| t.pos).unwrap_or([0.0; 3]);
            (*c, p, s.hp > 0)
        }).collect();
        for i in 0..poss.len() {
            for j in (i + 1)..poss.len() {
                if !poss[i].2 || !poss[j].2 { continue; }
                let (ci, pi, _) = poss[i];
                let (cj, pj, _) = poss[j];
                let dx = pi[0] - pj[0];
                let dz = pi[2] - pj[2];
                let d2 = (dx * dx + dz * dz).sqrt();
                if d2 < 0.9 && d2 > 1e-4 {
                    let push = (0.9 - d2) * 0.5;
                    let nx = dx / d2;
                    let nz = dz / d2;
                    for (c, sx, sz) in [(ci, nx, nz), (cj, -nx, -nz)] {
                        if let Some(s) = sessions.get_mut(&c) {
                            if let Some((tr, _, _)) = world.get_mut(s.entity) {
                                tr.pos[0] += sx * push;
                                tr.pos[2] += sz * push;
                            }
                        }
                    }
                }
            }
        }

        // History für Rewind + World-Spiegel (Occlusion-Kompat)
        for s in sessions.values_mut() {
            s.hist.push(nova_core::history::TransformSample { tick, pos: s.sim.pos, yaw: s.yaw });
            if let Some((tr, ve, _)) = world.get_mut(s.entity) {
                tr.pos = s.sim.pos;
                tr.yaw = s.yaw;
                ve.v = s.sim.vel;
            }
        }

        // ---- Match-Reset nach First-to-21 ----
        if scores[0] >= nova_core::protocol::WIN_SCORE || scores[1] >= nova_core::protocol::WIN_SCORE {
            scores = [0, 0];
            for s in sessions.values_mut() {
                s.hp = 100;
                s.dead_until = 0;
                let sp = maps::SPAWNS[s.spawn_idx];
                if let Some((tr, ve, he)) = world.get_mut(s.entity) {
                    tr.pos = sp;
                    ve.v = [0.0, 0.0, 0.0];
                    he.hp = 100.0;
                }
                s.yaw = if s.team == 0 { std::f32::consts::FRAC_PI_2 } else { -std::f32::consts::FRAC_PI_2 };
            }
            println!("[{}] MATCH-RESET: neue Runde", tick);
        }

        // ---- Snapshot ----
        let players: Vec<_> = sessions.values().filter_map(|s| {
            Some(nova_core::protocol::PlayerState {
                id: s.id, name: s.name.clone(), pos: s.sim.pos, yaw: s.yaw,
                last_seq: s.last_seq, hp: s.hp, kills: s.kills, team: s.team,
            })
        }).collect();
        if tick % 3 == 0 {
            // 20 Hz Replikation bei 60 Hz Simulation + Ack pro Client
            for (conn, sess) in sessions.iter() {
                let ack = sess.last_seq;
                let snap_bin = bincode::serialize(&ServerMsg::Snapshot { tick, ack, players: players.clone(), events: events.clone(), scores }).unwrap();
                let snap_json = serde_json::to_string(&ServerMsg::Snapshot { tick, ack, players: players.clone(), events: events.clone(), scores }).unwrap();
                let cj = match conn { Conn::Ws(i) => ws_enc.lock().unwrap().get(i).copied().unwrap_or(false), _ => false };
                send(conn, snap_bin, snap_json, cj, &ws_senders, &sock);
            }
        }

        if last_stats.elapsed() >= Duration::from_secs(5) {
            last_stats = Instant::now();
            println!("[{}] {} Spieler, map=shard_city_lite", tick, sessions.len());
        }
    }
}

/// Hitscan mit Lag-Comp: Ziele werden an ihrer Position vor REWIND_TICKS getestet,
/// Occlusion gegen solide Map-Boxen, PUMP: <12m = 100, sonst 60.
fn handle_fire(
    sessions: &mut HashMap<Conn, Session>,
    world: &World,
    arena: &[maps::BoxDef],
    broken: &mut Vec<bool>,
    crate_hp: &mut Vec<f32>,
    is_solid: fn(&maps::BoxDef, bool) -> bool,
    shooter_id: u32,
    conn: Conn,
    dir: [f32; 3],
    tick: u64,
    scores: &mut [u32; 2],
    weapon: u8,
    rewind_ticks: u64,
) -> Option<Vec<GameEvent>> {
    let wdef = nova_core::weapons::WEAPONS[(weapon as usize).min(4)];
    let rate_t = (nova_core::weapons::fire_interval(&wdef) * 60.0) as u64;
    let (origin, shooter_team) = {
        let s = sessions.get(&conn)?;
        if s.id != shooter_id || s.hp == 0 { return None; }
        if s.last_fire != 0 && tick.saturating_sub(s.last_fire) < rate_t.max(3) { return None; } // Anti-Cheat-Rate pro Waffe
        (s.sim.pos, s.team)
    };
    let d = norm3(dir);
    if d[0].abs() + d[1].abs() + d[2].abs() < 1e-4 { return None; }
    let o = [origin[0], origin[1] + EYE, origin[2]];

    // Occlusion: nächste solide Wand entlang des Strahls (zerstörte Kisten ausgeblendet)
    let mut wall_t = 80.0f32;
    let mut crate_hit: Option<(u32, f32)> = None;
    for (bi, b) in arena.iter().enumerate() {
        if !is_solid(b, broken[bi]) { continue; }
        if let Some(t) = ray_box_t(o, d, b) {
            if t < wall_t {
                wall_t = t;
                if b.kind == 2 { crate_hit = Some((bi as u32, t)); } else { crate_hit = None; }
            }
        }
    }

    // Ziele: Rewind-Position
    let targets: Vec<(u32, [f32; 3])> = sessions.values()
        .filter(|s| s.id != shooter_id && s.hp > 0 && s.team != shooter_team)
        .filter_map(|s| {
            let want = tick.saturating_sub(rewind_ticks);
            let p = s.hist.at(want)?.pos;
            Some((s.id, p))
        })
        .collect();

    let mut best: Option<(u32, f32, [f32; 3])> = None;
    for (tid, p) in targets {
        // Ray vs Capsule (approx: Zylinder r=0.55, y 0..1.9) am Rewind-Punkt
        if let Some(t) = ray_vs_player(o, d, p) {
            if t < wall_t && best.map(|(_, bt, _)| t < bt).unwrap_or(true) {
                let hit = [o[0] + d[0] * t, o[1] + d[1] * t, o[2] + d[2] * t];
                best = Some((tid, t, hit));
            }
        }
    }

    let s = sessions.get_mut(&conn)?;
    s.last_fire = tick;
    let mut out = Vec::new();
    if let Some((tid, t, hit)) = best {
        let dmg = nova_core::weapons::damage_at(&wdef, t).round() as u32;
        let victim_conn = sessions.iter().find(|(_, v)| v.id == tid).map(|(c, _)| *c);
        if let Some(vc) = victim_conn {
            if let Some(v) = sessions.get_mut(&vc) {
                v.hp = v.hp.saturating_sub(dmg);
                if v.hp == 0 {
                    v.dead_until = tick + RESPAWN_TICKS;
                    if let Some(sh) = sessions.get_mut(&conn) { sh.kills += 1; }
                    scores[shooter_team as usize] += 1;
                    out.push(GameEvent::Kill { killer: shooter_id, victim: tid });
                    if scores[shooter_team as usize] >= nova_core::protocol::WIN_SCORE {
                        out.push(GameEvent::MatchEnd { winner: shooter_team, score: *scores });
                    }
                }
            }
        }
        out.push(GameEvent::Damage { from: shooter_id, to: tid, dmg, hit });
    } else if let Some((bi, _t)) = crate_hit {
        // Kiste zerlegen
        let bu = bi as usize;
        crate_hp[bu] -= 50.0;
        if crate_hp[bu] <= 0.0 && !broken[bu] {
            broken[bu] = true;
            out.push(GameEvent::Break { bi });
        }
    }
    Some(out)
}

fn ray_vs_player(o: [f32; 3], d: [f32; 3], p: [f32; 3]) -> Option<f32> {
    // horizontaler Kreis r=0.55
    let ox = o[0] - p[0];
    let oz = o[2] - p[2];
    let a = d[0] * d[0] + d[2] * d[2];
    if a < 1e-8 { return None; }
    let b = 2.0 * (ox * d[0] + oz * d[2]);
    let c = ox * ox + oz * oz - 0.55 * 0.55;
    let disc = b * b - 4.0 * a * c;
    if disc < 0.0 { return None; }
    let t = (-b - disc.sqrt()) / (2.0 * a);
    if t < 0.0 { return None; }
    let y = o[1] + d[1] * t;
    if y < 0.0 || y > 1.95 { return None; }
    Some(t)
}

fn ray_box_t(o: [f32; 3], d: [f32; 3], b: &maps::BoxDef) -> Option<f32> {
    let mut tmin = 0.0f32;
    let mut tmax = 200.0f32;
    for i in 0..3 {
        if d[i].abs() < 1e-8 {
            if (o[i] - b.pos[i]).abs() > b.half[i] { return None; }
        } else {
            let inv = 1.0 / d[i];
            let mut t1 = (b.pos[i] - b.half[i] - o[i]) * inv;
            let mut t2 = (b.pos[i] + b.half[i] - o[i]) * inv;
            if t1 > t2 { std::mem::swap(&mut t1, &mut t2); }
            tmin = tmin.max(t1);
            tmax = tmax.min(t2);
            if tmin > tmax { return None; }
        }
    }
    Some(tmin)
}

fn norm3(v: [f32; 3]) -> [f32; 3] {
    let l = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if l < 1e-6 { [0.0, 0.0, 0.0] } else { [v[0] / l, v[1] / l, v[2] / l] }
}

fn send(conn: &Conn, payload_bin: Vec<u8>, payload_json: String, is_json: bool, ws_senders: &Arc<Mutex<HashMap<u64, mpsc::Sender<Vec<u8>>>>>, sock: &UdpSocket) {
    match conn {
        Conn::Udp(addr) => { let _ = sock.send_to(&payload_bin, addr); }
        Conn::Ws(id) => {
            if let Some(tx) = ws_senders.lock().unwrap().get(id) {
                if is_json { let _ = tx.send(payload_json.into_bytes()); } else { let _ = tx.send(payload_bin); }
            }
        }
    }
}

fn arg(args: &[String], key: &str) -> Option<u32> {
    args.windows(2).find(|w| w[0] == key).and_then(|w| w[1].parse().ok())
}
