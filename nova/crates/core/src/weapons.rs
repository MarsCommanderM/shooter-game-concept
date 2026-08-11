//! Waffen als reine Daten — Client, Server, Bots und Replay lesen dieselben Werte.
//! Milestone 02: keine if/else-Waffenlogik in der Codebasis.

#[derive(Clone, Copy, Debug)]
pub struct WeaponDef {
    pub name: &'static str,
    pub damage: f32,
    pub damage_far: f32,
    pub falloff_start: f32,
    pub rounds_per_minute: f32,
    pub magazine_size: u16,
    pub reload_seconds: f32,
    pub max_range: f32,
    pub hip_spread: f32,
    pub ads_spread: f32,
    pub recoil_pitch: f32,
    pub recoil_yaw: f32,
}

pub const M16: WeaponDef = WeaponDef {
    name: "M16", damage: 26.0, damage_far: 18.0, falloff_start: 30.0,
    rounds_per_minute: 720.0, magazine_size: 30, reload_seconds: 1.9,
    max_range: 120.0, hip_spread: 0.018, ads_spread: 0.004,
    recoil_pitch: 0.45, recoil_yaw: 0.2,
};
pub const AK47: WeaponDef = WeaponDef {
    name: "AK-47", damage: 34.0, damage_far: 22.0, falloff_start: 35.0,
    rounds_per_minute: 600.0, magazine_size: 30, reload_seconds: 2.2,
    max_range: 140.0, hip_spread: 0.024, ads_spread: 0.006,
    recoil_pitch: 0.62, recoil_yaw: 0.28,
};
pub const MP5: WeaponDef = WeaponDef {
    name: "MP5", damage: 16.0, damage_far: 9.0, falloff_start: 18.0,
    rounds_per_minute: 800.0, magazine_size: 30, reload_seconds: 1.7,
    max_range: 60.0, hip_spread: 0.02, ads_spread: 0.005,
    recoil_pitch: 0.3, recoil_yaw: 0.15,
};
pub const MG4: WeaponDef = WeaponDef {
    name: "MG4", damage: 24.0, damage_far: 16.0, falloff_start: 40.0,
    rounds_per_minute: 650.0, magazine_size: 60, reload_seconds: 3.2,
    max_range: 160.0, hip_spread: 0.03, ads_spread: 0.008,
    recoil_pitch: 0.5, recoil_yaw: 0.22,
};
pub const PUMP: WeaponDef = WeaponDef {
    name: "PUMP", damage: 100.0, damage_far: 60.0, falloff_start: 12.0,
    rounds_per_minute: 75.0, magazine_size: 6, reload_seconds: 2.8,
    max_range: 40.0, hip_spread: 0.05, ads_spread: 0.01,
    recoil_pitch: 1.4, recoil_yaw: 0.5,
};

pub const WEAPONS: [WeaponDef; 5] = [M16, AK47, MP5, MG4, PUMP];

/// Feuerrate in Sekunden zwischen zwei Schüssen
pub const fn fire_interval(w: &WeaponDef) -> f32 {
    60.0 / w.rounds_per_minute
}

/// Damage mit Distanz-Falloff
pub fn damage_at(w: &WeaponDef, dist: f32) -> f32 {
    if dist <= w.falloff_start {
        w.damage
    } else {
        let t = ((dist - w.falloff_start) / (w.max_range - w.falloff_start)).clamp(0.0, 1.0);
        w.damage + (w.damage_far - w.damage) * t
    }
}
