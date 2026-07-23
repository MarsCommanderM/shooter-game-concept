import { SiteNav } from "@/components/site-nav";
import { HeroSection } from "@/components/hero-section";
import { FactsBar } from "@/components/facts-bar";
import { USPSection } from "@/components/usp-section";
import { SettingSection } from "@/components/setting-section";
import { MechanicsSection } from "@/components/mechanics-section";
import { CharacterSection } from "@/components/character-section";
import { ArsenalSection } from "@/components/arsenal-section";
import { EnemiesSection } from "@/components/enemies-section";
import { SiteFooter } from "@/components/site-footer";

export default function Page() {
  return (
    <>
      <SiteNav />
      <main>
        <HeroSection />
        <FactsBar />
        <USPSection />
        <SettingSection />
        <MechanicsSection />
        <CharacterSection />
        <ArsenalSection />
        <EnemiesSection />
      </main>
      <SiteFooter />
    </>
  );
}
