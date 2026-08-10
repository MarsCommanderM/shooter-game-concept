import type { Metadata } from "next";
import { OnlineGame } from "@/components/online-game";

export const metadata: Metadata = {
  title: "WIRRWARR // ONLINE",
  description:
    "Echtes Online-Multiplayer: FFA-Deathmatch über WebSocket-Server (server.mjs). Öffne die Seite in zwei Tabs oder Fenstern und kämpfe live.",
};

export default function OnlinePage() {
  return <OnlineGame />;
}
