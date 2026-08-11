//! Gemeinsame Spieler-Simulation — läuft identisch auf Client (Prediction) und Server (Authority).
//! Damit ist Client-Movement == Server-Movement per Konstruktion.

use crate::maps;

pub const WALK_SPEED: f32 = 5.2;
pub const SPRINT_SPEED: f32 = 8.6;
pub const ACCEL: f32 = 46.0;
pub const DAMP: f32 = 12.0;
pub const PLAYER_RADIUS: f32 = 0.4;
pub const EYE_HEIGHT: f32 = 1.6;

#[derive(Clone, Copy, Debug, serde::Serialize, serde::Deserialize)]
pub struct PlayerSim {
    pub pos: [f32; 3],
    pub vel: [f32; 3],
    pub yaw: f32,
    pub pitch: f32,
}

#[derive(Clone, Copy, Debug)]
pub struct InputSample {
    pub wish: [f32; 2], // [strafe, vor], bereits serverseitig geclampt
    pub sprint: bool,
}

/// Ein autoritativer Sim-Schritt. dt ist auf Server FIXED_DT, auf Client Frame-dt.
pub fn step(s: &mut PlayerSim, inp: &InputSample, dt: f32, arena: &[maps::BoxDef]) {
    let (sy, cy) = (s.yaw.sin(), s.yaw.cos());
    let mut wx = 0.0f32;
    let mut wz = 0.0f32;
    if inp.wish[1] != 0.0 || inp.wish[0] != 0.0 {
        wx += inp.wish[0] * cy - inp.wish[1] * sy;
        wz += -inp.wish[0] * sy - inp.wish[1] * cy;
        let l = (wx * wx + wz * wz).sqrt();
        if l > 1.0 {
            wx /= l;
            wz /= l;
        }
    }
    let max_sp = if inp.sprint { SPRINT_SPEED } else { WALK_SPEED };
    s.vel[0] += wx * ACCEL * dt;
    s.vel[2] += wz * ACCEL * dt;
    let damp = if wx != 0.0 || wz != 0.0 { 1.0 - 1.4 * dt } else { 1.0 - DAMP * dt };
    s.vel[0] *= damp.max(0.0);
    s.vel[2] *= damp.max(0.0);
    let sp = (s.vel[0] * s.vel[0] + s.vel[2] * s.vel[2]).sqrt();
    if sp > max_sp && sp > 0.0 {
        s.vel[0] *= max_sp / sp;
        s.vel[2] *= max_sp / sp;
    }
    s.pos[0] += s.vel[0] * dt;
    s.pos[2] += s.vel[2] * dt;
    collide(s, arena);
}

/// Collision-Regeln (shared): horizontales Raus-Schieben aus soliden Boxen + Arena-Clamp.
pub fn collide(s: &mut PlayerSim, arena: &[maps::BoxDef]) {
    for b in arena {
        if b.kind == 1 || b.kind == 4 || b.half[1] < 0.2 {
            continue; // Neon/Pads/flacher Boden sind nicht solide
        }
        let dx = s.pos[0] - b.pos[0];
        let dz = s.pos[2] - b.pos[2];
        let px = b.half[0] + PLAYER_RADIUS - dx.abs();
        let pz = b.half[2] + PLAYER_RADIUS - dz.abs();
        if px > 0.0 && pz > 0.0 && s.pos[1] < b.pos[1] + b.half[1] {
            if px < pz {
                s.pos[0] += px * (if dx >= 0.0 { 1.0 } else { -1.0 });
                s.vel[0] = 0.0;
            } else {
                s.pos[2] += pz * (if dz >= 0.0 { 1.0 } else { -1.0 });
                s.vel[2] = 0.0;
            }
        }
    }
    let h = maps::ARENA_HALF - 1.0;
    s.pos[0] = s.pos[0].clamp(-h, h);
    s.pos[2] = s.pos[2].clamp(-h, h);
    s.pos[1] = 0.0;
}
