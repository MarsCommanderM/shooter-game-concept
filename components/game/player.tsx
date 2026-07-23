"use client";

import { useRef } from "react";
import { useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";
import {
  ARENA_SIZE,
  addImpact,
  addTracer,
  consumeMouseDelta,
  enemyRegistry,
  input,
  pillarRegistry,
  playerAim,
  playerPosition,
  playerState,
  raySphere,
} from "@/lib/game/shared";
import { gameStore } from "@/lib/game/store";

const PLAYER_RADIUS = 0.5;
const BASE_SPEED = 9;
const SPRINT_SPEED = 14;
const FIRE_INTERVAL = 0.1; // seconds between shots
const RELOAD_TIME = 1.4;
const GUN_DAMAGE = 26;
const MAX_RANGE = 140;
const GUN_HEIGHT = 1.4;

const UP = new THREE.Vector3(0, 1, 0);

export function Player() {
  const group = useRef<THREE.Group>(null);
  const muzzleGlow = useRef<THREE.PointLight>(null);
  const { camera } = useThree();

  // Camera orbit angles.
  const yaw = useRef(0);
  const pitch = useRef(0.12);
  const lastShot = useRef(0);
  const reloadUntil = useRef(0);
  const muzzleFlash = useRef(0);

  // Reusable temporaries (avoid per-frame allocations).
  const forward = useRef(new THREE.Vector3());
  const right = useRef(new THREE.Vector3());
  const move = useRef(new THREE.Vector3());
  const offset = useRef(new THREE.Vector3());
  const camTarget = useRef(new THREE.Vector3());
  const muzzle = useRef(new THREE.Vector3());
  const tmp = useRef(new THREE.Vector3());

  useFrame((_, rawDelta) => {
    const delta = Math.min(rawDelta, 0.05); // clamp to avoid tunneling on lag
    const now = performance.now() / 1000;

    // --- Camera look (mouse) ---
    const { dx, dy } = consumeMouseDelta();
    yaw.current -= dx * 0.0022;
    pitch.current -= dy * 0.0022;
    pitch.current = Math.max(-0.55, Math.min(0.45, pitch.current));

    // Horizontal basis vectors from yaw.
    forward.current.set(Math.sin(yaw.current), 0, Math.cos(yaw.current)).normalize();
    right.current.crossVectors(forward.current, UP).normalize();

    // --- Movement ---
    move.current.set(0, 0, 0);
    if (input.forward) move.current.add(forward.current);
    if (input.backward) move.current.sub(forward.current);
    if (input.right) move.current.sub(right.current);
    if (input.left) move.current.add(right.current);

    if (move.current.lengthSq() > 0) {
      move.current.normalize();
      const speed = input.sprint ? SPRINT_SPEED : BASE_SPEED;
      playerPosition.addScaledVector(move.current, speed * delta);
    }

    // Arena bounds.
    const limit = ARENA_SIZE - 2;
    playerPosition.x = Math.max(-limit, Math.min(limit, playerPosition.x));
    playerPosition.z = Math.max(-limit, Math.min(limit, playerPosition.z));

    // Pillar collision (push player out).
    for (const p of pillarRegistry.values()) {
      if (!p.alive) continue;
      const dxp = playerPosition.x - p.position.x;
      const dzp = playerPosition.z - p.position.z;
      const distSq = dxp * dxp + dzp * dzp;
      const minDist = p.radius + PLAYER_RADIUS;
      if (distSq < minDist * minDist && distSq > 1e-5) {
        const dist = Math.sqrt(distSq);
        const push = (minDist - dist) / dist;
        playerPosition.x += dxp * push;
        playerPosition.z += dzp * push;
      }
    }
    playerPosition.y = 1;

    // --- Aim direction (through screen center) ---
    offset.current
      .set(
        Math.sin(yaw.current) * Math.cos(pitch.current),
        Math.sin(pitch.current),
        Math.cos(yaw.current) * Math.cos(pitch.current),
      )
      .normalize();
    playerAim.copy(offset.current);

    // --- Update player mesh ---
    if (group.current) {
      group.current.position.copy(playerPosition);
      group.current.rotation.y = yaw.current;
    }

    // --- Third-person camera ---
    const camDist = 8;
    camTarget.current.copy(playerPosition).addScaledVector(UP, 2.1);
    camera.position
      .copy(camTarget.current)
      .addScaledVector(offset.current, -camDist);
    // Keep camera above ground.
    camera.position.y = Math.max(camera.position.y, 1.2);
    camera.lookAt(camTarget.current);

    // --- Shooting ---
    const store = gameStore.get();
    // Reload handling.
    if (store.reloading && now >= reloadUntil.current) {
      gameStore.set({ ammo: store.maxAmmo, reloading: false });
    }
    if (
      input.shooting &&
      !store.reloading &&
      store.ammo > 0 &&
      now - lastShot.current >= FIRE_INTERVAL &&
      playerState.alive
    ) {
      lastShot.current = now;
      fire(store.ammo);
      muzzleFlash.current = 0.05;
    }
    // Auto reload when empty.
    if (!store.reloading && store.ammo === 0) {
      gameStore.set({ reloading: true });
      reloadUntil.current = now + RELOAD_TIME;
    }

    // Muzzle flash light.
    if (muzzleGlow.current) {
      muzzleFlash.current = Math.max(0, muzzleFlash.current - delta);
      muzzleGlow.current.intensity = muzzleFlash.current > 0 ? 6 : 0;
    }
  });

  function fire(currentAmmo: number) {
    gameStore.set({ ammo: currentAmmo - 1 });

    // Muzzle origin: player gun height, slightly forward.
    muzzle.current
      .copy(playerPosition)
      .setY(playerPosition.y + GUN_HEIGHT - 1)
      .addScaledVector(playerAim, 0.6);

    // Slight spread.
    const spread = 0.012;
    tmp.current
      .copy(playerAim)
      .add(
        new THREE.Vector3(
          (Math.random() - 0.5) * spread,
          (Math.random() - 0.5) * spread,
          (Math.random() - 0.5) * spread,
        ),
      )
      .normalize();

    let closest = MAX_RANGE;
    let hitEnemyId: number | null = null;
    let hitPillarId: number | null = null;

    // Enemies.
    for (const e of enemyRegistry.values()) {
      if (!e.alive) continue;
      const t = raySphere(muzzle.current, tmp.current, e.position, e.radius);
      if (t !== null && t < closest) {
        closest = t;
        hitEnemyId = e.id;
        hitPillarId = null;
      }
    }
    // Pillars (block shots + take damage).
    for (const p of pillarRegistry.values()) {
      if (!p.alive) continue;
      const center = new THREE.Vector3(p.position.x, muzzle.current.y, p.position.z);
      const t = raySphere(muzzle.current, tmp.current, center, p.radius);
      if (t !== null && t < closest) {
        closest = t;
        hitPillarId = p.id;
        hitEnemyId = null;
      }
    }

    const hitPoint = muzzle.current.clone().addScaledVector(tmp.current, closest);
    addTracer(muzzle.current, hitPoint);

    if (hitEnemyId !== null) {
      const e = enemyRegistry.get(hitEnemyId);
      if (e) {
        e.hp -= GUN_DAMAGE;
        e.hitFlash = 1;
        addImpact(hitPoint, "#ff4d4d");
      }
    } else if (hitPillarId !== null) {
      const p = pillarRegistry.get(hitPillarId);
      if (p) {
        p.hp -= GUN_DAMAGE;
        p.hitFlash = 1;
        addImpact(hitPoint, "#39ff14");
        if (p.hp <= 0) p.alive = false;
      }
    }
  }

  return (
    <group ref={group}>
      {/* Body */}
      <mesh position={[0, 0, 0]} castShadow>
        <capsuleGeometry args={[0.45, 1.0, 4, 12]} />
        <meshStandardMaterial
          color="#2a3f30"
          emissive="#39ff14"
          emissiveIntensity={0.15}
          roughness={0.5}
          metalness={0.3}
        />
      </mesh>
      {/* Head */}
      <mesh position={[0, 0.95, 0]} castShadow>
        <sphereGeometry args={[0.3, 16, 16]} />
        <meshStandardMaterial color="#3a5240" roughness={0.6} />
      </mesh>
      {/* Biomass arm / weapon indicator pointing forward (+Z local) */}
      <mesh position={[0.35, 0.2, 0.6]} rotation={[Math.PI / 2, 0, 0]}>
        <cylinderGeometry args={[0.09, 0.09, 1.1, 8]} />
        <meshStandardMaterial
          color="#101a12"
          emissive="#39ff14"
          emissiveIntensity={0.4}
          metalness={0.6}
          roughness={0.3}
        />
      </mesh>
      {/* Forward marker so orientation is readable */}
      <mesh position={[0, 0.2, 0.7]}>
        <boxGeometry args={[0.12, 0.12, 0.12]} />
        <meshStandardMaterial color="#39ff14" emissive="#39ff14" emissiveIntensity={1.5} />
      </mesh>
      <pointLight ref={muzzleGlow} position={[0.35, 0.2, 1.2]} color="#aaff88" intensity={0} distance={8} />
    </group>
  );
}
