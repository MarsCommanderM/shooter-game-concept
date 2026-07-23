"use client";

import { Canvas } from "@react-three/fiber";
import { Suspense } from "react";
import { Arena } from "./arena";
import { Player } from "./player";
import { Enemies } from "./enemies";
import { Effects } from "./effects";

export function GameScene() {
  return (
    <Canvas
      shadows
      camera={{ fov: 60, near: 0.1, far: 200, position: [0, 6, 10] }}
      gl={{ antialias: true, powerPreference: "high-performance" }}
      dpr={[1, 1.75]}
    >
      <color attach="background" args={["#0a0f0a"]} />
      <fog attach="fog" args={["#0a0f0a", 25, 70]} />

      {/* Lighting - moody with toxic green ambience */}
      <ambientLight intensity={0.35} color="#7dffb0" />
      <hemisphereLight
        intensity={0.4}
        color="#88ff9c"
        groundColor="#1a1410"
      />
      <directionalLight
        position={[15, 25, 10]}
        intensity={1.1}
        color="#d9ffe6"
        castShadow
        shadow-mapSize={[2048, 2048]}
        shadow-camera-left={-40}
        shadow-camera-right={40}
        shadow-camera-top={40}
        shadow-camera-bottom={-40}
        shadow-camera-near={1}
        shadow-camera-far={80}
        shadow-bias={-0.0004}
      />

      <Suspense fallback={null}>
        <Arena />
        <Player />
        <Enemies />
        <Effects />
      </Suspense>
    </Canvas>
  );
}
