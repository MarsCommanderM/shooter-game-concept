import type { Metadata } from "next";
import { RealGame } from "@/components/real-game";

export const metadata: Metadata = {
  title: "SAVE THE WORLD // Vertical Slice (3D)",
  description:
    "Der echte 3D-Prototyp von SAVE THE WORLD: Three.js-Engine mit Destruction, Gunplay und Bot-KI – Phase 1 der Roadmap.",
};

export default function GamePage() {
  return <RealGame />;
}
