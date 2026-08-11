import type { Metadata } from "next";
import { PlayDemo } from "@/components/play-demo";

export const metadata: Metadata = {
  title: "SAVE THE WORLD // Spielbare Web-Demo",
  description:
    "Raycaster-Prototyp der SAVE-THE-WORLD-Arena: alle sechs Multiplayer-Modi gegen Bots – Team-Deathmatch, Hauptquartier, Frei für alle, Sabotage, Capture the Flag und Herrschaft.",
};

export default function PlayPage() {
  return <PlayDemo />;
}
