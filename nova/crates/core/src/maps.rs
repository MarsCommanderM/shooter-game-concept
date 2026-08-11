//! Gemeinsame Map-Definitionen (Client rendert, Server simuliert/Occlusion).
//! „Shard City": echte City-Anmutung — Beton/Asphalt, Gebäude mit Fenstern,
//! Bäume, Autowracks, zerstörbare Kisten. Sightline-Regel: max. 2 lange Achsen.

#[derive(Clone, Copy, Debug)]
pub struct BoxDef {
    pub pos: [f32; 3],
    pub half: [f32; 3],
    /// 0=Beton, 1=Neon-Akzent, 2=Kiste (zerstörbar), 3=Metall, 4=Pad, 5=Tower,
    /// 6=Gebäude (Fenster), 7=Baum, 8=Autowrack
    pub kind: u8,
}

pub const ARENA_HALF: f32 = 48.0;

const fn b(x: f32, y: f32, z: f32, hx: f32, hy: f32, hz: f32, kind: u8) -> BoxDef {
    BoxDef { pos: [x, y, z], half: [hx, hy, hz], kind }
}

pub fn shard_city_lite() -> Vec<BoxDef> {
    let h = ARENA_HALF;
    let mut a = Vec::new();
    // Asphalt-Boden
    a.push(b(0.0, -0.2, 0.0, h * 2.0, 0.2, h * 2.0, 0));

    // ---- Gebäude-Ring (City statt Arena-Wand), mit Gate-Lücken ----
    // Nord & Süd: je 2 Gebäudeblöcke mit Lücke in der Mitte (Gates)
    a.push(b(-26.0, 5.0, h, 20.0, 5.0, 2.0, 6));
    a.push(b(26.0, 5.0, h, 20.0, 5.0, 2.0, 6));
    a.push(b(-26.0, 5.0, -h, 20.0, 5.0, 2.0, 6));
    a.push(b(26.0, 5.0, -h, 20.0, 5.0, 2.0, 6));
    // Ost & West
    a.push(b(h, 5.0, -26.0, 2.0, 5.0, 20.0, 6));
    a.push(b(h, 5.0, 26.0, 2.0, 5.0, 20.0, 6));
    a.push(b(-h, 5.0, -26.0, 2.0, 5.0, 20.0, 6));
    a.push(b(-h, 5.0, 26.0, 2.0, 5.0, 20.0, 6));
    // Gate-Pfeiler mit Neon (Callouts: „North Gate" etc.)
    for (x, z, sx, sz) in [(0.0, h, 1.5, 2.0), (0.0, -h, 1.5, 2.0), (h, 0.0, 2.0, 1.5), (-h, 0.0, 2.0, 1.5)] {
        a.push(b(x, 2.0, z, sx, 2.0, sz, 3));
        a.push(b(x, 4.1, z, sx + 0.1, 0.08, sz + 0.1, 1));
    }

    // ---- Core Tower (Plaza-Mitte, bricht Sichtlinien) ----
    a.push(b(0.0, 6.0, 0.0, 3.5, 6.0, 3.5, 5));
    a.push(b(0.0, 12.1, 0.0, 3.7, 0.1, 3.7, 1));

    // ---- Straßen: Betonbarrieren + Autowracks statt grüner Wände ----
    for (x, z, sx, sz) in [(6.0, 14.0, 0.5, 5.0), (-6.0, -14.0, 0.5, 5.0), (14.0, 6.0, 5.0, 0.5), (-14.0, -6.0, 5.0, 0.5)] {
        a.push(b(x, 0.55, z, sx, 0.55, sz, 0)); // Jersey-Barriers
    }
    // Autowracks (Cover mit Charakter)
    a.push(b(10.0, 0.75, 2.0, 2.2, 0.75, 1.0, 8));
    a.push(b(10.6, 1.35, 2.0, 1.1, 0.5, 0.9, 8));
    a.push(b(-10.0, 0.75, -2.0, 2.2, 0.75, 1.0, 8));
    a.push(b(-10.6, 1.35, -2.0, 1.1, 0.5, 0.9, 8));
    a.push(b(2.0, 0.75, -18.0, 1.0, 0.75, 2.2, 8));
    a.push(b(-2.0, 0.75, 18.0, 1.0, 0.75, 2.2, 8));

    // ---- Zerstörbare Kisten (Holz/Müll) an Hotspots ----
    for (x, z) in [(8.0, 8.0), (-8.0, -8.0), (8.0, -8.0), (-8.0, 8.0), (20.0, 4.0), (-20.0, -4.0), (4.0, 20.0), (-4.0, -20.0)] {
        a.push(b(x, 0.7, z, 1.0, 0.7, 1.0, 2));
    }

    // ---- Park-Ecken: Bäume (Stamm + Krone) ----
    for (x, z) in [(36.0, 12.0), (36.0, -12.0), (-36.0, 12.0), (-36.0, -12.0), (12.0, 36.0), (-12.0, -36.0), (24.0, 36.0), (-24.0, -36.0)] {
        a.push(b(x, 1.4, z, 0.25, 1.4, 0.25, 7));
        a.push(b(x, 3.6, z, 1.5, 1.3, 1.5, 9));
    }

    // ---- District-Module (Warehouse/Metro/Skybridge/Power) ----
    // Warehouse NE: Kisten-Stapel
    for (x, z) in [(30.0, 30.0), (34.0, 26.0), (26.0, 34.0)] {
        a.push(b(x, 0.9, z, 1.5, 0.9, 1.5, 2));
    }
    a.push(b(30.0, 2.2, 30.0, 3.0, 2.2, 0.5, 6));
    // Metro NW: Säulen
    for (x, z) in [(-30.0, 30.0), (-26.0, 30.0), (-34.0, 30.0), (-30.0, 26.0)] {
        a.push(b(x, 2.0, z, 0.5, 2.0, 0.5, 0));
    }
    // Skybridge SE: erhöhte Brücke
    a.push(b(30.0, 2.6, -30.0, 5.0, 0.2, 1.6, 3));
    a.push(b(26.0, 1.3, -30.0, 0.6, 1.3, 1.6, 0));
    a.push(b(34.0, 1.3, -30.0, 0.6, 1.3, 1.6, 0));
    // Power SW: Kessel + Ring
    a.push(b(-30.0, 1.4, -30.0, 3.0, 1.4, 3.0, 3));
    a.push(b(-30.0, 2.9, -30.0, 3.1, 0.06, 3.1, 1));
    for (x, z) in [(-24.0, -24.0), (-36.0, -24.0), (-24.0, -36.0), (-36.0, -36.0)] {
        a.push(b(x, 0.8, z, 1.0, 0.8, 1.0, 8));
    }

    // Straßenlaternen (Pfosten + Kopf)
    for (x, z) in [(6.0, 24.0), (-6.0, -24.0), (24.0, 6.0), (-24.0, -6.0), (6.0, -24.0), (-6.0, 24.0), (24.0, -6.0), (-24.0, 6.0)] {
        a.push(b(x, 2.0, z, 0.08, 2.0, 0.08, 10));
        a.push(b(x, 4.1, z, 0.22, 0.12, 0.22, 11));
    }

    // Spawn-Pads (seitlich)
    a.push(b(40.0, 0.06, 0.0, 3.0, 0.06, 3.0, 4));
    a.push(b(-40.0, 0.06, 0.0, 3.0, 0.06, 3.0, 4));
    a
}

/// Spawn-Punkte: seitlich versetzt (kein Spawn-Farm), Team 0 = +X, Team 1 = -X
pub const SPAWNS: [[f32; 3]; 4] = [
    [40.0, 0.0, 4.0],
    [40.0, 0.0, -4.0],
    [-40.0, 0.0, -4.0],
    [-40.0, 0.0, 4.0],
];

/// Ray vs AABB (Slab-Test) – serverseitige Occlusion
pub fn ray_hits_box(o: [f32; 3], d: [f32; 3], b: &BoxDef, max_t: f32) -> bool {
    let mut tmin = 0.0f32;
    let mut tmax = max_t;
    for i in 0..3 {
        if d[i].abs() < 1e-8 {
            if (o[i] - b.pos[i]).abs() > b.half[i] {
                return false;
            }
        } else {
            let inv = 1.0 / d[i];
            let mut t1 = (b.pos[i] - b.half[i] - o[i]) * inv;
            let mut t2 = (b.pos[i] + b.half[i] - o[i]) * inv;
            if t1 > t2 {
                std::mem::swap(&mut t1, &mut t2);
            }
            tmin = tmin.max(t1);
            tmax = tmax.min(t2);
            if tmin > tmax {
                return false;
            }
        }
    }
    true
}
