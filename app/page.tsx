import { SiteNav } from "@/components/site-nav";
import { HeroSection } from "@/components/hero-section";
import { USPSection } from "@/components/usp-section";
import { SettingSection } from "@/components/setting-section";
import { MechanicsSection } from "@/components/mechanics-section";
import { CharacterSection } from "@/components/character-section";
import { MultiplayerSection } from "@/components/multiplayer-section";
import { ArsenalSection } from "@/components/arsenal-section";
import { MapsSection } from "@/components/maps-section";
import { EvolutionSection } from "@/components/evolution-section";
import { HudSection } from "@/components/hud-section";
import { SoundSection } from "@/components/sound-section";
import { LiveOpsSection } from "@/components/liveops-section";
import { RoadmapSection } from "@/components/roadmap-section";
import { SiteFooter } from "@/components/site-footer";

export default function Page() {
  return (
    <>
      <SiteNav />
      <main>
        <HeroSection />
        <USPSection />
        <SettingSection />
        <MechanicsSection />
        <CharacterSection />
        <MultiplayerSection />
        <ArsenalSection />
        <MapsSection />
        <EvolutionSection />
        <HudSection />
        <SoundSection />
        <LiveOpsSection />
        <RoadmapSection />
      </main>
      <SiteFooter />
    </>
  );
}
