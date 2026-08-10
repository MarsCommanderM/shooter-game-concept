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
const FIRE_INTERVAL = 0.09; // seconds between shots
const RELOAD_TIME = 1.4;
const GUN_DAMAGE = 26;
const MAX_RANGE = 160;
const EYE_HEIGHT = 0.62; // added to playerPosition.y (=1) -> ~1.62 eye
const BASE_FOV = 62;
const SPRINT_FOV = 70;
const MOUSE_SENS = 0.0022;

const UP = new THREE.Vector3(0, 1, 0);
// Local mounting point of the weapon relative to the camera.
const GUN_OFFSET = new THREE.Vector3(0.24, -0.26, -0.78);
// Muzzle tip in gun-local space (forward = -Z).
const MUZZLE_LOCAL = new THREE.Vector3(0, 0.04, -0.72);

export function Player() {
  const gun = useRef<THREE.Group>(null);
  const muzzleLight = useRef<THREE.PointLight>(null);
  const muzzleFlashMesh = useRef<THREE.Mesh>(null);
  const { camera } = useThree();

  // Look angles.
  const yaw = useRef(0);
  const pitch = useRef(0);
  const lastShot = useRef(0);
  const reloadUntil = useRef(0);

  // Juice state.
  const muzzleFlash = useRef(0);
  const recoilKick = useRef(0); // gun backward kick
  const recoilPitch = useRef(0); // view climb
  const recoilYaw = useRef(0);
  const bobT = useRef(0);
  const roll = useRef(0);

  // Reusable temporaries.
  const forward = useRef(new THREE.Vector3());
  const right = useRef(new THREE.Vector3());
  const move = useRef(new THREE.Vector3());
  const eye = useRef(new THREE.Vector3());
  const target = useRef(new THREE.Vector3());
  const muzzle = useRef(new THREE.Vector3());
  const shotDir = useRef(new THREE.Vector3());
  const hitPoint = useRef(new THREE.Vector3());
  const pillarCenter = useRef(new THREE.Vector3());

  useFrame((_, rawDelta) => {
    const delta = Math.min(rawDelta, 0.05);
    const now = performance.now() / 1000;
    const store = gameStore.get();

    // --- Mouse look ---
    const { dx, dy } = consumeMouseDelta();
    yaw.current -= dx * MOUSE_SENS;
    pitch.current -= dy * MOUSE_SENS;
    pitch.current = Math.max(-1.2, Math.min(1.2, pitch.current));

    // Decay recoil/juice.
    recoilKick.current = THREE.MathUtils.damp(recoilKick.current, 0, 9, delta);
    recoilPitch.current = THREE.MathUtils.damp(recoilPitch.current, 0, 8, delta);
    recoilYaw.current = THREE.MathUtils.damp(recoilYaw.current, 0, 8, delta);
    muzzleFlash.current = Math.max(0, muzzleFlash.current - delta * 22);

    // Effective look (base + recoil climb).
    const effPitch = Math.max(
      -1.3,
      Math.min(1.3, pitch.current + recoilPitch.current),
    );
    const effYaw = yaw.current + recoilYaw.current;

    // Horizontal basis from yaw only (movement stays on ground plane).
    forward.current.set(Math.sin(yaw.current), 0, Math.cos(yaw.current)).normalize();
    right.current.crossVectors(forward.current, UP).normalize();

    // --- Movement ---
    move.current.set(0, 0, 0);
    if (input.forward) move.current.add(forward.current);
    if (input.backward) move.current.sub(forward.current);
    if (input.right) move.current.sub(right.current);
    if (input.left) move.current.add(right.current);

    const moving = move.current.lengthSq() > 0 && playerState.alive;
    const sprinting = input.sprint && input.forward;
    if (moving) {
      move.current.normalize();
      const speed = sprinting ? SPRINT_SPEED : BASE_SPEED;
      playerPosition.addScaledVector(move.current, speed * delta);
    }

    // Arena bounds.
    const limit = ARENA_SIZE - 2;
    playerPosition.x = Math.max(-limit, Math.min(limit, playerPosition.x));
    playerPosition.z = Math.max(-limit, Math.min(limit, playerPosition.z));

    // Pillar collision.
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

    // --- Aim direction (screen center) ---
    const cp = Math.cos(effPitch);
    shotDir.current
      .set(Math.sin(effYaw) * cp, Math.sin(effPitch), Math.cos(effYaw) * cp)
      .normalize();
    playerAim.copy(shotDir.current);

    // --- Weapon bob + camera roll ---
    const strafe = (input.left ? 1 : 0) - (input.right ? 1 : 0);
    roll.current = THREE.MathUtils.damp(roll.current, strafe * 0.03, 6, delta);
    let bobX = 0;
    let bobY = 0;
    if (moving) {
      bobT.current += delta * (sprinting ? 15 : 10);
      const amp = sprinting ? 1.5 : 1;
      bobX = Math.cos(bobT.current) * 0.012 * amp;
      bobY = Math.abs(Math.sin(bobT.current)) * 0.016 * amp;
    } else {
      // idle sway
      bobT.current += delta * 1.5;
      bobX = Math.cos(bobT.current) * 0.003;
      bobY = Math.sin(bobT.current * 0.8) * 0.003;
    }

    // --- First-person camera ---
    eye.current.copy(playerPosition);
    eye.current.y += EYE_HEIGHT + bobY * 0.4;
    camera.position.copy(eye.current);
    target.current.copy(eye.current).add(shotDir.current);
    camera.lookAt(target.current);
    if (roll.current !== 0) camera.rotateZ(roll.current);

    // FOV kick when sprinting.
    const cam = camera as THREE.PerspectiveCamera;
    const targetFov = sprinting && moving ? SPRINT_FOV : BASE_FOV;
    cam.fov = THREE.MathUtils.damp(cam.fov, targetFov, 8, delta);
    cam.updateProjectionMatrix();

    // --- Weapon viewmodel transform ---
    if (gun.current) {
      gun.current.position.copy(camera.position);
      gun.current.quaternion.copy(camera.quaternion);
      gun.current.translateX(GUN_OFFSET.x + bobX);
      gun.current.translateY(GUN_OFFSET.y + bobY);
      gun.current.translateZ(GUN_OFFSET.z + recoilKick.current * 0.16);
      gun.current.rotateX(recoilKick.current * 0.5);
    }

    // Muzzle flash visuals.
    if (muzzleLight.current) {
      muzzleLight.current.intensity = muzzleFlash.current * 7;
    }
    if (muzzleFlashMesh.current) {
      const f = muzzleFlash.current;
      muzzleFlashMesh.current.visible = f > 0.05;
      const s = 0.18 + f * 0.28;
      muzzleFlashMesh.current.scale.set(s, s, s);
      muzzleFlashMesh.current.rotation.z = Math.random() * Math.PI;
      (muzzleFlashMesh.current.material as THREE.MeshBasicMaterial).opacity = f;
    }

    // --- Reload handling ---
    if (input.reload) {
      input.reload = false;
      if (!store.reloading && store.ammo < store.maxAmmo && store.ammo >= 0) {
        gameStore.set({ reloading: true });
        reloadUntil.current = now + RELOAD_TIME;
      }
    }
    if (store.reloading && now >= reloadUntil.current) {
      gameStore.set({ ammo: store.maxAmmo, reloading: false });
    }

    // --- Shooting ---
    const wantsFire = input.shooting || input.firePressed;
    input.firePressed = false; // consume tap latch
    if (
      wantsFire &&
      !store.reloading &&
      store.ammo > 0 &&
      now - lastShot.current >= FIRE_INTERVAL &&
      playerState.alive
    ) {
      lastShot.current = now;
      fire(store.ammo);
    }
    if (!store.reloading && store.ammo === 0 && playerState.alive) {
      gameStore.set({ reloading: true });
      reloadUntil.current = now + RELOAD_TIME;
    }
  });

  function fire(currentAmmo: number) {
    gameStore.set({ ammo: currentAmmo - 1 });

    // Recoil impulse.
    recoilKick.current = Math.min(1.2, recoilKick.current + 0.55);
    recoilPitch.current = Math.min(0.09, recoilPitch.current + 0.016);
    recoilYaw.current += (Math.random() - 0.5) * 0.01;
    muzzleFlash.current = 1;

    // Muzzle world position (from the gun barrel tip).
    if (gun.current) {
      muzzle.current.copy(MUZZLE_LOCAL);
      gun.current.localToWorld(muzzle.current);
    } else {
      muzzle.current.copy(camera.position);
    }

    // Ray from the eye along the aim (so it lands under the crosshair).
    const origin = camera.position;
    const dir = shotDir.current;

    let closest = MAX_RANGE;
    let hitEnemyId: number | null = null;
    let hitPillarId: number | null = null;

    for (const e of enemyRegistry.values()) {
      if (!e.alive) continue;
      const t = raySphere(origin, dir, e.position, e.radius);
      if (t !== null && t < closest) {
        closest = t;
        hitEnemyId = e.id;
        hitPillarId = null;
      }
    }
    for (const p of pillarRegistry.values()) {
      if (!p.alive) continue;
      pillarCenter.current.set(p.position.x, origin.y, p.position.z);
      const t = raySphere(origin, dir, pillarCenter.current, p.radius);
      if (t !== null && t < closest) {
        closest = t;
        hitPillarId = p.id;
        hitEnemyId = null;
      }
    }

    hitPoint.current.copy(origin).addScaledVector(dir, closest);
    addTracer(muzzle.current, hitPoint.current);

    if (hitEnemyId !== null) {
      const e = enemyRegistry.get(hitEnemyId);
      if (e) {
        e.hp -= GUN_DAMAGE;
        e.hitFlash = 1;
        addImpact(hitPoint.current, "#ff4d4d");
        gameStore.set({ hitMarkerAt: performance.now() });
      }
    } else if (hitPillarId !== null) {
      const p = pillarRegistry.get(hitPillarId);
      if (p) {
        p.hp -= GUN_DAMAGE;
        p.hitFlash = 1;
        addImpact(hitPoint.current, "#39ff14");
        if (p.hp <= 0) p.alive = false;
      }
    }
  }

  return (
    <group ref={gun} scale={0.8}>
      {/* Receiver / body */}
      <mesh position={[0, 0, -0.1]} castShadow>
        <boxGeometry args={[0.11, 0.15, 0.5]} />
        <meshStandardMaterial
          color="#12181a"
          emissive="#39ff14"
          emissiveIntensity={0.12}
          metalness={0.7}
          roughness={0.35}
        />
      </mesh>
      {/* Barrel */}
      <mesh position={[0, 0.04, -0.5]} rotation={[Math.PI / 2, 0, 0]} castShadow>
        <cylinderGeometry args={[0.045, 0.05, 0.5, 10]} />
        <meshStandardMaterial color="#0c1012" metalness={0.85} roughness={0.25} />
      </mesh>
      {/* Bio-core (glowing) */}
      <mesh position={[0, 0.1, -0.02]}>
        <boxGeometry args={[0.045, 0.045, 0.18]} />
        <meshStandardMaterial
          color="#39ff14"
          emissive="#39ff14"
          emissiveIntensity={1.5}
          toneMapped={false}
        />
      </mesh>
      {/* Grip */}
      <mesh position={[0, -0.14, 0.08]} rotation={[0.35, 0, 0]} castShadow>
        <boxGeometry args={[0.08, 0.2, 0.09]} />
        <meshStandardMaterial color="#0c1012" metalness={0.6} roughness={0.4} />
      </mesh>
      {/* Sight rail */}
      <mesh position={[0, 0.1, -0.18]}>
        <boxGeometry args={[0.02, 0.03, 0.12]} />
        <meshStandardMaterial color="#39ff14" emissive="#39ff14" emissiveIntensity={1.4} toneMapped={false} />
      </mesh>

      {/* Muzzle flash */}
      <mesh
        ref={muzzleFlashMesh}
        position={[MUZZLE_LOCAL.x, MUZZLE_LOCAL.y, MUZZLE_LOCAL.z]}
        visible={false}
      >
        <sphereGeometry args={[1, 8, 8]} />
        <meshBasicMaterial color="#e9ffcf" transparent opacity={0} toneMapped={false} />
      </mesh>
      <pointLight
        ref={muzzleLight}
        position={[MUZZLE_LOCAL.x, MUZZLE_LOCAL.y, MUZZLE_LOCAL.z]}
        color="#c8ff9a"
        intensity={0}
        distance={9}
      />
    </group>
  );
}
