"use client";

import dynamic from "next/dynamic";

const GameScene = dynamic(
  () => import("@/components/game/game-scene").then((m) => m.GameScene),
  {
    ssr: false,
    loading: () => (
      <div className="flex h-screen w-full items-center justify-center bg-background">
        <p className="font-mono text-sm tracking-widest uppercase text-primary animate-pulse-neon glow-neon-sm">
          Lade Simulation...
        </p>
      </div>
    ),
  }
);

export default function PlayPage() {
  return (
    <div className="h-screen w-full overflow-hidden bg-background">
      <GameScene />
    </div>
  );
}
