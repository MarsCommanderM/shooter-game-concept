import * as THREE from "three";

export const ARENA_SIZE = 60; // half-extent of playable area

/** Live player position, written by Player each frame, read by enemies. */
export const playerPosition = new THREE.Vector3(0, 1, 0);
/** Direction the player is aiming (normalized, on the XZ plane mostly). */
export const playerAim = new THREE.Vector3(0, 0, -1);
export const playerState = { alive: true };

/** Raw input state, mutated by the input hook, read in the render loop. */
export const input = {
  forward: false,
  backward: false,
  left: false,
  right: false,
  sprint: false,
  shooting: false,
  reload: false,
  dodge: false,
  // accumulated mouse delta since last frame (pointer lock)
  mouseDX: 0,
  mouseDY: 0,
};

export function consumeMouseDelta() {
  const dx = input.mouseDX;
  const dy = input.mouseDY;
  input.mouseDX = 0;
  input.mouseDY = 0;
  return { dx, dy };
}

export interface EnemyRuntime {
  id: number;
  position: THREE.Vector3;
  radius: number;
  hp: number;
  maxHp: number;
  alive: boolean;
  hitFlash: number;
  lastAttack: number;
  speed: number;
  kind: "grunt" | "brute";
}

export interface PillarRuntime {
  id: number;
  position: THREE.Vector3;
  radius: number;
  height: number;
  hp: number;
  maxHp: number;
  alive: boolean;
  hitFlash: number;
}

/** Non-reactive registries for the physics/raycast loop. */
export const enemyRegistry = new Map<number, EnemyRuntime>();
export const pillarRegistry = new Map<number, PillarRuntime>();

/** Tracer events queued by the player, consumed by the bullet renderer. */
export interface Tracer {
  id: number;
  from: THREE.Vector3;
  to: THREE.Vector3;
  born: number;
}
export const tracers: Tracer[] = [];
let tracerId = 0;

export function addTracer(from: THREE.Vector3, to: THREE.Vector3) {
  tracers.push({
    id: tracerId++,
    from: from.clone(),
    to: to.clone(),
    born: performance.now(),
  });
}

/** Impact particle bursts. */
export interface Impact {
  id: number;
  position: THREE.Vector3;
  born: number;
  color: string;
}
export const impacts: Impact[] = [];
let impactId = 0;

export function addImpact(position: THREE.Vector3, color: string) {
  impacts.push({ id: impactId++, position: position.clone(), born: performance.now(), color });
}

export function resetWorld() {
  playerPosition.set(0, 1, 0);
  playerAim.set(0, 0, -1);
  playerState.alive = true;
  enemyRegistry.clear();
  // Restore pillars instead of clearing: the Arena registers them only once.
  for (const p of pillarRegistry.values()) {
    p.hp = p.maxHp;
    p.alive = true;
    p.hitFlash = 0;
  }
  tracers.length = 0;
  impacts.length = 0;
  input.forward = input.backward = input.left = input.right = false;
  input.sprint = input.shooting = false;
  input.reload = input.dodge = false;
  input.mouseDX = input.mouseDY = 0;
}

/**
 * Ray vs sphere intersection. Returns the distance along the ray to the first
 * hit, or null. Used for hitscan shooting.
 */
export function raySphere(
  origin: THREE.Vector3,
  dir: THREE.Vector3,
  center: THREE.Vector3,
  radius: number,
): number | null {
  const oc = origin.clone().sub(center);
  const b = oc.dot(dir);
  const c = oc.dot(oc) - radius * radius;
  const disc = b * b - c;
  if (disc < 0) return null;
  const sqrt = Math.sqrt(disc);
  const t1 = -b - sqrt;
  const t2 = -b + sqrt;
  if (t1 > 0) return t1;
  if (t2 > 0) return t2;
  return null;
}
