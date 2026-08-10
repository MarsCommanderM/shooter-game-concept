"use client";

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { ARENA_SIZE, pillarRegistry, type PillarRuntime } from "@/lib/game/shared";

const NEON = "#39ff14";
const WALL_H = 6;

interface PillarDef {
  id: number;
  x: number;
  z: number;
  radius: number;
  height: number;
  hp: number;
}

function makePillars(): PillarDef[] {
  const defs: PillarDef[] = [];
  let id = 0;
  // A ring of cover pillars plus some scattered blocks.
  const ringPositions = [
    [-18, -18], [18, -18], [-18, 18], [18, 18],
    [0, -26], [0, 26], [-26, 0], [26, 0],
    [-10, 6], [12, -8], [8, 14], [-14, -6],
    [22, 10], [-22, -12], [4, 22], [-6, -22],
  ];
  for (const [x, z] of ringPositions) {
    const radius = 1.6 + Math.random() * 0.8;
    defs.push({ id: id++, x, z, radius, height: 3 + Math.random() * 2, hp: 60 });
  }
  return defs;
}

export function Arena() {
  const pillarDefs = useMemo(makePillars, []);
  const meshRefs = useRef<(THREE.Mesh | null)[]>([]);

  // Register pillars into the runtime registry once.
  useMemo(() => {
    pillarRegistry.clear();
    for (const d of pillarDefs) {
      const p: PillarRuntime = {
        id: d.id,
        position: new THREE.Vector3(d.x, d.height / 2, d.z),
        radius: d.radius,
        height: d.height,
        hp: d.hp,
        maxHp: d.hp,
        alive: true,
        hitFlash: 0,
      };
      pillarRegistry.set(d.id, p);
    }
  }, [pillarDefs]);

  // Sync visual state (destroyed pillars vanish, hit ones flash).
  useFrame((_, delta) => {
    for (let i = 0; i < pillarDefs.length; i++) {
      const mesh = meshRefs.current[i];
      if (!mesh) continue;
      const rt = pillarRegistry.get(pillarDefs[i].id);
      if (!rt) continue;
      if (!rt.alive) {
        mesh.visible = false;
        continue;
      }
      const mat = mesh.material as THREE.MeshStandardMaterial;
      if (rt.hitFlash > 0) {
        rt.hitFlash = Math.max(0, rt.hitFlash - delta * 4);
        mat.emissiveIntensity = 0.06 + rt.hitFlash * 2;
        // Shrink slightly as it takes damage.
        const dmg = 1 - rt.hp / rt.maxHp;
        mesh.scale.setScalar(1 - dmg * 0.25);
      }
    }
  });

  return (
    <group>
      {/* Ground */}
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0, 0]} receiveShadow>
        <planeGeometry args={[ARENA_SIZE * 2, ARENA_SIZE * 2]} />
        <meshStandardMaterial color="#0c1410" roughness={0.95} metalness={0.1} />
      </mesh>

      {/* Grid overlay */}
      <gridHelper
        args={[ARENA_SIZE * 2, 40, NEON, "#1c3a24"]}
        position={[0, 0.02, 0]}
      />

      {/* Perimeter walls */}
      {[
        { pos: [0, WALL_H / 2, -ARENA_SIZE] as const, size: [ARENA_SIZE * 2, WALL_H, 1] as const },
        { pos: [0, WALL_H / 2, ARENA_SIZE] as const, size: [ARENA_SIZE * 2, WALL_H, 1] as const },
        { pos: [-ARENA_SIZE, WALL_H / 2, 0] as const, size: [1, WALL_H, ARENA_SIZE * 2] as const },
        { pos: [ARENA_SIZE, WALL_H / 2, 0] as const, size: [1, WALL_H, ARENA_SIZE * 2] as const },
      ].map((w, i) => (
        <mesh key={i} position={w.pos} castShadow receiveShadow>
          <boxGeometry args={w.size} />
          <meshStandardMaterial
            color="#2e332c"
            emissive={NEON}
            emissiveIntensity={0.03}
            roughness={0.9}
          />
        </mesh>
      ))}

      {/* Destructible pillars (biomass-covered cover) */}
      {pillarDefs.map((d, i) => (
        <mesh
          key={d.id}
          ref={(el) => {
            meshRefs.current[i] = el;
          }}
          position={[d.x, d.height / 2, d.z]}
          castShadow
          receiveShadow
        >
          <cylinderGeometry args={[d.radius, d.radius * 1.15, d.height, 12]} />
          <meshStandardMaterial
            color="#4a4f46"
            emissive={NEON}
            emissiveIntensity={0.06}
            roughness={0.85}
            metalness={0.1}
          />
        </mesh>
      ))}
    </group>
  );
}
