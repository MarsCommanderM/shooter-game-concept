//! ECS light (Bauplan-Entscheidung): Komponenten als dichte Arrays,
//! Systeme als freie Funktionen. Skaliert besser als verschachtelte Klassen.
#[derive(Clone, Copy, Default, Debug)]
pub struct Transform {
    pub pos: [f32; 3],
    pub yaw: f32,
}

#[derive(Clone, Copy, Default, Debug)]
pub struct Velocity {
    pub v: [f32; 3],
}

#[derive(Clone, Copy, Default, Debug)]
pub struct Health {
    pub hp: f32,
}

#[derive(Clone, Copy, Default, Debug)]
pub struct NetworkIdentity {
    pub id: u32,
}

pub struct World {
    pub transform: Vec<Option<Transform>>,
    pub velocity: Vec<Option<Velocity>>,
    pub health: Vec<Option<Health>>,
    pub net: Vec<Option<NetworkIdentity>>,
    pub alive_count: usize,
}

impl World {
    pub fn new() -> Self {
        Self {
            transform: Vec::new(),
            velocity: Vec::new(),
            health: Vec::new(),
            net: Vec::new(),
            alive_count: 0,
        }
    }

    pub fn spawn(&mut self, id: u32, pos: [f32; 3]) -> u32 {
        let e = self.transform.len() as u32;
        self.transform.push(Some(Transform { pos, yaw: 0.0 }));
        self.velocity.push(Some(Velocity::default()));
        self.health.push(Some(Health { hp: 100.0 }));
        self.net.push(Some(NetworkIdentity { id }));
        self.alive_count += 1;
        e
    }

    pub fn get_mut(&mut self, e: u32) -> Option<(&mut Transform, &mut Velocity, &mut Health)> {
        let e = e as usize;
        if e >= self.transform.len() {
            return None;
        }
        let tr = self.transform.get_mut(e)?.as_mut()?;
        let ve = self.velocity.get_mut(e)?.as_mut()?;
        let he = self.health.get_mut(e)?.as_mut()?;
        Some((tr, ve, he))
    }
}

/// MovementSystem: Integration + Boden + Arena-Clamp (server-autoritative Physik-Basis)
pub fn movement_system(w: &mut World, dt: f32) {
    for e in 0..w.transform.len() {
        let (tr, ve, _he) = match (
            w.transform.get_mut(e).and_then(|o| o.as_mut()),
            w.velocity.get_mut(e).and_then(|o| o.as_mut()),
            w.health.get_mut(e).and_then(|o| o.as_mut()),
        ) {
            (Some(a), Some(b), Some(c)) => (a, b, c),
            _ => continue,
        };
        tr.pos[0] += ve.v[0] * dt;
        tr.pos[2] += ve.v[2] * dt;
        tr.pos[1] = 0.0; // Boden (Milestone 01: kein Springen serverseitig)
        let half = crate::ARENA_HALF;
        tr.pos[0] = tr.pos[0].clamp(-half, half);
        tr.pos[2] = tr.pos[2].clamp(-half, half);
        let _ = ve;
    }
}
