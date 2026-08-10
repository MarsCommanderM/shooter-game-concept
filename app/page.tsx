import { SiteNav } from "@/components/site-nav";
import { HeroSection } from "@/components/hero-section";
import { USPSection } from "@/components/usp-section";
import { SettingSection } from "@/components/setting-section";
import { MechanicsSection } from "@/components/mechanics-section";
import { CharacterSection } from "@/components/character-section";
import { MultiplayerSection } from "@/components/multiplayer-section";
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
      </main>
      <SiteFooter />
    </>
  );
}
