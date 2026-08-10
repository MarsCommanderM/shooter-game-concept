import type { Metadata } from "next";
import { PlayDemo } from "@/components/play-demo";

export const metadata: Metadata = {
  title: "WIRRWARR // Spielbare Web-Demo",
  description:
    "Raycaster-Prototyp der WIRRWARR-Arena: alle sechs Multiplayer-Modi gegen Bots – Team-Deathmatch, Hauptquartier, Frei für alle, Sabotage, Capture the Flag und Herrschaft.",
};

export default function PlayPage() {
  return <PlayDemo />;
}
