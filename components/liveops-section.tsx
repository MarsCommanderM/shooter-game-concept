"use client";

import { useEffect, useRef, useState } from "react";
import { CalendarRange, Medal, HeartHandshake, Gift } from "lucide-react";

interface Rank {
  id: string;
  name: string;
  range: string;
  desc: string;
  reward: string;
}

const RANKS: Rank[] = [
  {
    id: "schrott",
    name: "SCHROTT",
    range: "0 – 999 MMR",
    desc: "Der Einstieg. Jede*r fängt im Schrott an – und lernt die Karte durchs Sterben.",
    reward: "Basis-Waffenlack „Rost“",
  },
  {
    id: "stahl",
    name: "STAHL",
    range: "1000 – 1799 MMR",
    desc: "Solides Fundament: Rotationen sitzen, erste Breach-Routen werden bewusst genutzt.",
    reward: "Waffenanhänger „Funken“",
  },
  {
    id: "chitin",
    name: "CHITIN",
    range: "1800 – 2399 MMR",
    desc: "Hier beginnt der ernsthafte Krieg: koordinierte Squads, bewusste Zerstörung, Info-Game.",
    reward: "Exklusives Carapax-Profilbanner",
  },
  {
    id: "spore",
    name: "SPORE",
    range: "2400 – 2899 MMR",
    desc: "Die Elite streut ihre Taktik wie Sporen: jeder Winkel ist vorbereitet, jede Wand eine Option.",
    reward: "Animierter Waffenlack „Myzel“",
  },
  {
    id: "apex",
    name: "BIOMASS-APEX",
    range: "2900+ MMR · Top 500",
    desc: "Die Spitze der Nahrungskette. Sichtbarkeit auf der saisonalen Apex-Liste, Scrims gegen Pro-Teams.",
    reward: "Saison-exklusiver Apex-Rahmen + Titel",
  },
];

const SEASON_BEATS = [
  { week: "Woche 1", event: "Season-Start: neues Kriegsprotokoll, Ranked-Reset (Soft-Wipe)" },
  { week: "Woche 3", event: "Mid-Season-Event: Biomass-Anomalie verändert eine Karte temporär" },
  { week: "Woche 6", event: "Neue Waffe oder neue Karte – immer gameplay-relevant, nie paywalled" },
  { week: "Woche 9", event: "Apex-Showdown: Top-500-Turnier mit Spectator-Übertragung" },
  { week: "Woche 10", event: "Season-Finale: Rang-Belohnungen, Recap-Replay, Teaser der nächsten Season" },
];

export function LiveOpsSection() {
  const [activeRank, setActiveRank] = useState(2);
  const [isVisible, setIsVisible] = useState(false);
  const sectionRef = useRef<HTMLElement>(null);

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) setIsVisible(true);
      },
      { threshold: 0.05 }
    );
    if (sectionRef.current) observer.observe(sectionRef.current);
    return () => observer.disconnect();
  }, []);

  const rank = RANKS[activeRank] ?? RANKS[0];

  return (
    <section ref={sectionRef} id="liveops" className="relative py-20 md:py-32 px-6 scanlines">
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            11 // Live-Ops
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-12 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Seasons des Krieges,
          <span className="text-primary glow-neon-sm"> fair finanziert</span>
        </h2>

        <div className="grid lg:grid-cols-2 gap-10">
          {/* Ranked Ladder */}
          <div
            className={`transition-all duration-700 delay-200 ${
              isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
            }`}
          >
            <div className="flex items-center gap-3 mb-6">
              <span className="w-9 h-9 rounded-sm border border-primary/30 bg-primary/10 flex items-center justify-center">
                <Medal className="w-4 h-4 text-primary" strokeWidth={1.75} />
              </span>
              <h3 className="text-xl md:text-2xl font-bold text-foreground">Ranked-Ladder</h3>
            </div>

            <div className="space-y-2 mb-6">
              {RANKS.map((r, i) => (
                <button
                  key={r.id}
                  type="button"
                  onClick={() => setActiveRank(i)}
                  aria-pressed={i === activeRank}
                  className={`w-full flex items-center justify-between px-4 py-3 rounded-sm border transition-all min-h-[44px] ${
                    i === activeRank
                      ? "border-primary/60 bg-primary/10 box-glow-neon"
                      : "border-border bg-card hover:border-primary/30"
                  }`}
                >
                  <span className="flex items-center gap-3">
                    <span className="font-mono text-[10px] text-muted-foreground">0{i + 1}</span>
                    <span className={`font-bold tracking-widest text-sm ${i === activeRank ? "text-primary glow-neon-sm" : "text-foreground"}`}>
                      {r.name}
                    </span>
                  </span>
                  <span className="font-mono text-[10px] text-muted-foreground tracking-wider">
                    {r.range}
                  </span>
                </button>
              ))}
            </div>

            <div key={rank.id} className="animate-fade-in border border-border bg-card rounded-sm p-5">
              <p className="text-sm text-secondary-foreground leading-relaxed mb-3">{rank.desc}</p>
              <p className="font-mono text-[11px] text-primary tracking-wider uppercase flex items-center gap-2">
                <Gift className="w-3.5 h-3.5" strokeWidth={1.75} />
                {rank.reward}
              </p>
            </div>
          </div>

          {/* Season Beats */}
          <div
            className={`transition-all duration-700 delay-300 ${
              isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
            }`}
          >
            <div className="flex items-center gap-3 mb-6">
              <span className="w-9 h-9 rounded-sm border border-primary/30 bg-primary/10 flex items-center justify-center">
                <CalendarRange className="w-4 h-4 text-primary" strokeWidth={1.75} />
              </span>
              <h3 className="text-xl md:text-2xl font-bold text-foreground">
                Eine Season, zehn Wochen
              </h3>
            </div>

            <div className="relative pl-6 space-y-5">
              <div className="absolute left-1.5 top-1 bottom-1 w-px bg-gradient-to-b from-primary/60 to-transparent" />
              {SEASON_BEATS.map((beat) => (
                <div key={beat.week} className="relative">
                  <span className="absolute -left-[19px] top-1.5 w-2 h-2 rounded-full bg-primary/70 border border-primary" />
                  <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-primary mb-1">
                    {beat.week}
                  </p>
                  <p className="text-sm text-secondary-foreground leading-relaxed">{beat.event}</p>
                </div>
              ))}
            </div>

            {/* Fairness Pledge */}
            <div className="mt-8 border border-primary/40 bg-primary/5 rounded-sm p-5 flex items-start gap-4">
              <span className="w-9 h-9 rounded-sm border border-primary/40 bg-primary/10 flex items-center justify-center shrink-0">
                <HeartHandshake className="w-4 h-4 text-primary" strokeWidth={1.75} />
              </span>
              <div>
                <p className="font-mono text-xs tracking-[0.2em] uppercase text-primary mb-1.5 glow-neon-sm">
                  Fairness-Doktrin
                </p>
                <p className="text-sm text-foreground leading-relaxed">
                  Kein Pay-to-Win. Kein Stat-Boost kaufbar. Das Kriegsprotokoll
                  (Battlepass) enthält ausschließlich Kosmetik – jede gameplay-relevante
                  Waffe, Karte und jeder Genetische Code ist kostenlos erspielbar.
                </p>
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
