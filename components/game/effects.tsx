"use client";

import { useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { impacts, tracers } from "@/lib/game/shared";

const TRACER_LIFE = 90; // ms
const IMPACT_LIFE = 260; // ms
const MAX_VISUALS = 40;

/** Renders short-lived hitscan tracers and impact sparks from the shared queues. */
export function Effects() {
  const tracerRefs = useRef<(THREE.Mesh | null)[]>([]);
  const impactRefs = useRef<(THREE.Mesh | null)[]>([]);

  useFrame(() => {
    const now = performance.now();

    // Expire old entries.
    for (let i = tracers.length - 1; i >= 0; i--) {
      if (now - tracers[i].born > TRACER_LIFE) tracers.splice(i, 1);
    }
    for (let i = impacts.length - 1; i >= 0; i--) {
      if (now - impacts[i].born > IMPACT_LIFE) impacts.splice(i, 1);
    }

    // Position tracer meshes (thin cylinders between from/to).
    for (let i = 0; i < MAX_VISUALS; i++) {
      const mesh = tracerRefs.current[i];
      if (!mesh) continue;
      const t = tracers[i];
      if (!t) {
        mesh.visible = false;
        continue;
      }
      mesh.visible = true;
      const dir = new THREE.Vector3().subVectors(t.to, t.from);
      const len = dir.length();
      const mid = new THREE.Vector3().addVectors(t.from, t.to).multiplyScalar(0.5);
      mesh.position.copy(mid);
      mesh.scale.set(1, len, 1);
      mesh.quaternion.setFromUnitVectors(
        new THREE.Vector3(0, 1, 0),
        dir.normalize(),
      );
      const life = 1 - (now - t.born) / TRACER_LIFE;
      (mesh.material as THREE.MeshBasicMaterial).opacity = life;
    }

    // Impact sparks (expanding, fading spheres).
    for (let i = 0; i < MAX_VISUALS; i++) {
      const mesh = impactRefs.current[i];
      if (!mesh) continue;
      const im = impacts[i];
      if (!im) {
        mesh.visible = false;
        continue;
      }
      mesh.visible = true;
      mesh.position.copy(im.position);
      const life = 1 - (now - im.born) / IMPACT_LIFE;
      const s = 0.15 + (1 - life) * 0.6;
      mesh.scale.setScalar(s);
      const mat = mesh.material as THREE.MeshBasicMaterial;
      mat.color.set(im.color);
      mat.opacity = life;
    }
  });

  return (
    <group>
      {Array.from({ length: MAX_VISUALS }).map((_, i) => (
        <mesh
          key={`tracer-${i}`}
          ref={(el) => {
            tracerRefs.current[i] = el;
          }}
          visible={false}
        >
          <cylinderGeometry args={[0.03, 0.03, 1, 6]} />
          <meshBasicMaterial color="#aaff66" transparent opacity={1} toneMapped={false} />
        </mesh>
      ))}
      {Array.from({ length: MAX_VISUALS }).map((_, i) => (
        <mesh
          key={`impact-${i}`}
          ref={(el) => {
            impactRefs.current[i] = el;
          }}
          visible={false}
        >
          <sphereGeometry args={[1, 8, 8]} />
          <meshBasicMaterial color="#39ff14" transparent opacity={1} toneMapped={false} />
        </mesh>
      ))}
    </group>
  );
}
