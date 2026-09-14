//! Headless-Probe: verifiziert Binärprotokoll + Teams + Combat gegen einen Live-Server.
use std::net::UdpSocket;
use std::time::{Duration, Instant};

use nova_core::protocol::{ClientMsg, ServerMsg};

fn main() {
    let addr = std::env::args().nth(1).unwrap_or_else(|| "127.0.0.1:27015".into());
    let sock = UdpSocket::bind("0.0.0.0:0").unwrap();
    sock.connect(&addr).unwrap();
    sock.set_read_timeout(Some(Duration::from_millis(500))).unwrap();
    let send = |m: &ClientMsg| sock.send(&bincode::serialize(m).unwrap()).unwrap();

    send(&ClientMsg::Join { name: "PROBE_A".into() });
    let sock2 = UdpSocket::bind("0.0.0.0:0").unwrap();
    sock2.connect(&addr).unwrap();
    sock2.set_read_timeout(Some(Duration::from_millis(500))).unwrap();
    sock2.send(&bincode::serialize(&ClientMsg::Join { name: "PROBE_B".into() }).unwrap()).unwrap();

    let mut team_a = None;
    let mut got_dmg = false;
    let mut scores = [0u32; 2];
    let start = Instant::now();
    let mut fired = false;
    while start.elapsed() < Duration::from_secs(3) {
        if !fired && start.elapsed() > Duration::from_millis(700) {
            fired = true;
            // A (Team0, +X) feuert Richtung -X ... B sitzt auch +X? Team-Spawn: A team0 idx0 (+40,4), B team1 idx2 (-40,-4)
            // Richtung zu B:
            let dx = -80.0f32; let dz = -8.0;
            let l = (dx * dx + dz * dz).sqrt();
            send(&ClientMsg::Fire { id: 1, seq: 1, dir: [dx / l, 0.0, dz / l], weapon: 0 });
        }
        let mut buf = [0u8; 8192];
        if let Ok(n) = sock.recv(&mut buf) {
            if let Ok(m) = bincode::deserialize::<ServerMsg>(&buf[..n]) {
                match m {
                    ServerMsg::Welcome { team, .. } => team_a = Some(team),
                    ServerMsg::Snapshot { events, scores: sc, players, .. } => {
                        scores = sc;
                        for ev in events {
                            if matches!(ev, nova_core::protocol::GameEvent::Damage { .. }) { got_dmg = true; }
                        }
                        let _ = players;
                    }
                }
            }
        }
        std::thread::sleep(Duration::from_millis(16));
    }
    println!("PROBE: team_a={:?} dmg={} scores={:?}", team_a, got_dmg, scores);
}
