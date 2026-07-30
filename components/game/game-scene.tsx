"use client";

import { Canvas } from "@react-three/fiber";
import { Suspense, useCallback, useRef } from "react";
import { Arena } from "./arena";
import { Player } from "./player";
import { EnemyManager } from "./enemies";
import { Effects } from "./effects";
import { Hud } from "./hud";
import { Overlays } from "./overlays";
import { useInput } from "@/lib/game/use-input";
import { useGameState } from "@/lib/game/store";

export function GameScene() {
  const containerRef = useRef<HTMLDivElement>(null);
  const phase = useGameState((s) => s.phase);
  const playing = phase === "playing";

  useInput(playing, containerRef.current);

  // Re-acquire pointer lock when the player clicks the arena mid-game
  // (e.g. after pressing Esc).
  const handleClick = useCallback(() => {
    if (!playing) return;
    const canvas = containerRef.current?.querySelector("canvas");
    if (canvas && document.pointerLockElement !== canvas) {
      canvas.requestPointerLock();
    }
  }, [playing]);

  return (
    <div
      ref={containerRef}
      className="relative h-full w-full"
      onClick={handleClick}
    >
      <Canvas
        shadows
        camera={{ fov: 62, near: 0.1, far: 200, position: [0, 1.6, 0] }}
        gl={{ antialias: true, powerPreference: "high-performance" }}
        dpr={[1, 1.75]}
      >
        <color attach="background" args={["#0a0f0a"]} />
        <fog attach="fog" args={["#0a0f0a", 22, 68]} />

        {/* Lighting - moody with subtle toxic green ambience */}
        <ambientLight intensity={0.25} color="#c8d8c8" />
        <hemisphereLight intensity={0.3} color="#3d5c46" groundColor="#1a1410" />
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
          <EnemyManager />
          <Effects />
        </Suspense>
      </Canvas>
      <Hud />
      <Overlays />
    </div>
  );
}
