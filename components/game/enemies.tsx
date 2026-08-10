"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import {
  ARENA_SIZE,
  addImpact,
  type EnemyRuntime,
  enemyRegistry,
  pillarRegistry,
  playerPosition,
  playerState,
} from "@/lib/game/shared";
import { gameStore, useGameState } from "@/lib/game/store";

type EnemyKind = "grunt" | "brute";

interface SpawnDef {
  id: number;
  kind: EnemyKind;
  x: number;
  z: number;
}

const KIND_STATS: Record<EnemyKind, { hp: number; radius: number; speed: number; score: number; damage: number }> = {
  grunt: { hp: 50, radius: 0.8, speed: 4.2, score: 100, damage: 7 },
  brute: { hp: 150, radius: 1.5, speed: 2.6, score: 300, damage: 16 },
};

let uid = 1;

export function EnemyManager() {
  const phase = useGameState((s) => s.phase);
  const [spawns, setSpawns] = useState<SpawnDef[]>([]);
  const livingCount = useRef(0);
  const waveTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const started = useRef(false);

  const spawnWave = useCallback((waveNum: number) => {
    const gruntCount = 3 + waveNum * 2;
    const bruteCount = Math.max(0, Math.floor((waveNum - 1) / 2));
    const defs: SpawnDef[] = [];

    const make = (kind: EnemyKind) => {
      // Spawn on a ring around the arena, away from the player.
      const angle = Math.random() * Math.PI * 2;
      const dist = ARENA_SIZE - 6 - Math.random() * 8;
      defs.push({
        id: uid++,
        kind,
        x: Math.cos(angle) * dist,
        z: Math.sin(angle) * dist,
      });
    };

    for (let i = 0; i < gruntCount; i++) make("grunt");
    for (let i = 0; i < bruteCount; i++) make("brute");

    livingCount.current = defs.length;
    setSpawns(defs);
    gameStore.set({ wave: waveNum, enemiesRemaining: defs.length });
  }, []);

  // Start / reset waves based on phase.
  useEffect(() => {
    if (phase === "playing" && !started.current) {
      started.current = true;
      spawnWave(1);
    }
    if (phase === "menu" || phase === "gameover") {
      started.current = phase === "gameover" ? started.current : false;
      if (phase === "menu") {
        setSpawns([]);
        if (waveTimer.current) clearTimeout(waveTimer.current);
      }
    }
    return () => {
      if (phase === "menu" && waveTimer.current) clearTimeout(waveTimer.current);
    };
  }, [phase, spawnWave]);

  const handleDeath = useCallback(
    (id: number, kind: EnemyKind, pos: THREE.Vector3) => {
      addImpact(pos, "#39ff14");
      const stats = KIND_STATS[kind];
      const store = gameStore.get();
      livingCount.current -= 1;
      gameStore.set({
        score: store.score + stats.score,
        kills: store.kills + 1,
        enemiesRemaining: Math.max(0, livingCount.current),
        killMarkerAt: performance.now(),
      });
      setSpawns((prev) => prev.filter((s) => s.id !== id));

      if (livingCount.current <= 0 && gameStore.get().phase === "playing") {
        const nextWave = gameStore.get().wave + 1;
        waveTimer.current = setTimeout(() => {
          if (gameStore.get().phase === "playing") spawnWave(nextWave);
        }, 2600);
      }
    },
    [spawnWave],
  );

  // Reset the "started" gate when a fresh game begins from the orchestrator.
  useEffect(() => {
    if (phase === "playing") return;
    started.current = false;
  }, [phase]);

  return (
    <group>
      {spawns.map((s) => (
        <Enemy key={s.id} def={s} onDeath={handleDeath} />
      ))}
    </group>
  );
}

interface EnemyProps {
  def: SpawnDef;
  onDeath: (id: number, kind: EnemyKind, pos: THREE.Vector3) => void;
}

function Enemy({ def, onDeath }: EnemyProps) {
  const group = useRef<THREE.Group>(null);
  const core = useRef<THREE.Mesh>(null);
  const runtime = useRef<EnemyRuntime | null>(null);
  const dead = useRef(false);
  const toPlayer = useRef(new THREE.Vector3());
  const stats = KIND_STATS[def.kind];

  // Register runtime entry on mount.
  useEffect(() => {
    const rt: EnemyRuntime = {
      id: def.id,
      position: new THREE.Vector3(def.x, def.kind === "brute" ? 1.5 : 0.9, def.z),
      radius: stats.radius,
      hp: stats.hp,
      maxHp: stats.hp,
      alive: true,
      hitFlash: 0,
      lastAttack: 0,
      speed: stats.speed,
      kind: def.kind,
    };
    runtime.current = rt;
    enemyRegistry.set(def.id, rt);
    return () => {
      enemyRegistry.delete(def.id);
    };
  }, [def, stats]);

  useFrame((_, rawDelta) => {
    const delta = Math.min(rawDelta, 0.05);
    const rt = runtime.current;
    if (!rt || dead.current || !group.current) return;

    // Death check (hp reduced by player hitscan).
    if (rt.hp <= 0) {
      dead.current = true;
      rt.alive = false;
      onDeath(def.id, def.kind, rt.position);
      return;
    }

    const now = performance.now() / 1000;

    // Move toward the player on the XZ plane.
    toPlayer.current.subVectors(playerPosition, rt.position);
    toPlayer.current.y = 0;
    const dist = toPlayer.current.length();
    if (dist > 0.001) toPlayer.current.divideScalar(dist);

    const attackRange = rt.radius + 1.1;
    if (dist > attackRange && playerState.alive) {
      rt.position.addScaledVector(toPlayer.current, rt.speed * delta);
    }

    // Separation from other enemies (avoid stacking).
    for (const other of enemyRegistry.values()) {
      if (other.id === rt.id || !other.alive) continue;
      const dx = rt.position.x - other.position.x;
      const dz = rt.position.z - other.position.z;
      const dSq = dx * dx + dz * dz;
      const minD = rt.radius + other.radius;
      if (dSq < minD * minD && dSq > 1e-4) {
        const d = Math.sqrt(dSq);
        const push = ((minD - d) / d) * 0.5;
        rt.position.x += dx * push;
        rt.position.z += dz * push;
      }
    }

    // Push out of pillars.
    for (const p of pillarRegistry.values()) {
      if (!p.alive) continue;
      const dx = rt.position.x - p.position.x;
      const dz = rt.position.z - p.position.z;
      const dSq = dx * dx + dz * dz;
      const minD = p.radius + rt.radius;
      if (dSq < minD * minD && dSq > 1e-4) {
        const d = Math.sqrt(dSq);
        const push = (minD - d) / d;
        rt.position.x += dx * push;
        rt.position.z += dz * push;
      }
    }

    // Attack player on contact.
    if (dist <= attackRange && playerState.alive && now - rt.lastAttack > 0.8) {
      rt.lastAttack = now;
      const store = gameStore.get();
      const newHealth = Math.max(0, store.health - stats.damage);
      gameStore.set({ health: newHealth, damageAt: performance.now() });
      if (newHealth <= 0) {
        playerState.alive = false;
        gameStore.set({ phase: "gameover" });
      }
    }

    // Apply transform + hit flash + idle bob.
    group.current.position.copy(rt.position);
    const bob = Math.sin(now * 4 + def.id) * 0.08;
    group.current.position.y = rt.position.y + bob;

    if (core.current) {
      const mat = core.current.material as THREE.MeshStandardMaterial;
      if (rt.hitFlash > 0) rt.hitFlash = Math.max(0, rt.hitFlash - delta * 3);
      mat.emissiveIntensity = 0.6 + rt.hitFlash * 3;
      const hpFrac = rt.hp / rt.maxHp;
      mat.color.setRGB(0.1 + (1 - hpFrac) * 0.9, 0.9 * hpFrac + 0.1, 0.1);
      const pulse = 1 + Math.sin(now * 6 + def.id) * 0.05;
      core.current.scale.setScalar(pulse);
    }
  });

  const isBrute = def.kind === "brute";

  return (
    <group ref={group} position={[def.x, isBrute ? 1.5 : 0.9, def.z]}>
      <mesh ref={core} castShadow>
        <icosahedronGeometry args={[stats.radius, isBrute ? 2 : 1]} />
        <meshStandardMaterial
          color="#39ff14"
          emissive="#39ff14"
          emissiveIntensity={0.6}
          roughness={0.35}
          metalness={0.2}
          flatShading
        />
      </mesh>
      {/* Glow light */}
      <pointLight color="#39ff14" intensity={isBrute ? 3 : 1.4} distance={isBrute ? 10 : 6} />
      {/* Tendrils / spikes */}
      {isBrute && (
        <mesh rotation={[Math.PI, 0, 0]} position={[0, 0, 0]}>
          <coneGeometry args={[stats.radius * 0.8, stats.radius * 1.6, 6]} />
          <meshStandardMaterial color="#0f2417" emissive="#39ff14" emissiveIntensity={0.3} wireframe />
        </mesh>
      )}
    </group>
  );
}
