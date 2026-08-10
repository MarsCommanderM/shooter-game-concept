"use client";

import { useEffect, useRef, useState } from "react";

type PhaseStatus = "aktiv" | "geplant" | "später";

interface Phase {
  id: string;
  title: string;
  window: string;
  status: PhaseStatus;
  summary: string;
  items: string[];
}

const PHASES: Phase[] = [
  {
    id: "p0",
    title: "Phase 0 – Konzept & GDD",
    window: "Jetzt",
    status: "aktiv",
    summary:
      "Das vollständige Design-Dokument: Kernmechaniken, Multiplayer-Modi, Arsenal, Karten und Infrastruktur sind definiert und abgestimmt.",
    items: [
      "Game Design Document (diese Seite)",
      "6 Multiplayer-Modi spezifiziert",
      "Arsenal- & Kartendesign v1",
    ],
  },
  {
    id: "p1",
    title: "Phase 1 – Vertical Slice",
    window: "Q4 2026",
    status: "geplant",
    summary:
      "Ein spielbarer Querschnitt beweist die Fantasie: Zerstörung, Gunplay und Parkour in einer Arena – feel first, features second.",
    items: [
      "Destruction-Tech (Voxel/Chunk-Basiert)",
      "Gunplay- & Movement-Prototyp",
      "Eine Arena, ein Modus (TDM)",
    ],
  },
  {
    id: "p2",
    title: "Phase 2 – Alpha",
    window: "Q2 2027",
    status: "geplant",
    summary:
      "Alle vier Kernmechaniken sind integriert, der Taktik-Modus läuft, und die ersten Multiplayer-Modi gehen auf dedizierten Servern online.",
    items: [
      "Taktik-Modus & KI-Kameraden",
      "Multiplayer: TDM + Sabotage",
      "Karten: Sektor 7 + Stahlwiege",
    ],
  },
  {
    id: "p3",
    title: "Phase 3 – Closed Beta",
    window: "Q4 2027",
    status: "später",
    summary:
      "Alle sechs Modi live, Netcode und Anti-Cheat gehärtet, Balancing durch tausende Testspieler. Die Biomass-Upgrades gehen in den Langzeittest.",
    items: [
      "Alle 6 Modi + Ranked-Grundlagen",
      "128-Tick-Infrastruktur & Anti-Cheat",
      "Biomass-Garten + Upgrade-System",
    ],
  },
  {
    id: "p4",
    title: "Phase 4 – Launch & Seasons",
    window: "2028",
    status: "später",
    summary:
      "Release mit Live-Ops: saisonale Ranglisten, neue Waffen und Karten, Community-Server und ein eSport-fähiges Spectator-Ökosystem.",
    items: [
      "Launch auf PC & Konsolen",
      "Season-System & Leaderboards",
      "Replay-, Spectator- & Mod-Support",
    ],
  },
];

const STATUS_STYLE: Record<PhaseStatus, { label: string; cls: string }> = {
  aktiv: {
    label: "Aktiv",
    cls: "border-primary text-primary bg-primary/10 box-glow-neon",
  },
  geplant: {
    label: "Geplant",
    cls: "border-primary/40 text-primary/80 bg-primary/5",
  },
  später: {
    label: "Roadmap",
    cls: "border-border text-muted-foreground bg-secondary/40",
  },
};

export function RoadmapSection() {
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

  return (
    <section ref={sectionRef} id="roadmap" className="relative py-20 md:py-32 px-6">
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            08 // Roadmap
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-12 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Vom Papier
          <span className="text-primary glow-neon-sm"> in die Schlacht</span>
        </h2>

        <div className="relative pl-8 md:pl-10">
          {/* Timeline Line */}
          <div className="absolute left-2.5 md:left-3.5 top-2 bottom-2 w-px bg-gradient-to-b from-primary/60 via-primary/25 to-transparent" />

          <div className="space-y-10">
            {PHASES.map((phase, i) => {
              const style = STATUS_STYLE[phase.status];
              return (
                <div
                  key={phase.id}
                  className={`relative transition-all duration-700 ${
                    isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
                  }`}
                  style={{ transitionDelay: `${150 + i * 120}ms` }}
                >
                  {/* Node */}
                  <span
                    className={`absolute -left-[26px] md:-left-[30px] top-1.5 w-3 h-3 rounded-full border-2 ${
                      phase.status === "aktiv"
                        ? "border-primary bg-primary animate-pulse-neon"
                        : phase.status === "geplant"
                          ? "border-primary/50 bg-background"
                          : "border-muted-foreground/50 bg-background"
                    }`}
                  />

                  <div className="flex flex-wrap items-center gap-3 mb-3">
                    <h3
                      className={`text-lg md:text-xl font-bold ${
                        phase.status === "aktiv" ? "text-primary glow-neon-sm" : "text-foreground"
                      }`}
                    >
                      {phase.title}
                    </h3>
                    <span className={`font-mono text-[10px] tracking-wider uppercase border rounded-sm px-2.5 py-1 ${style.cls}`}>
                      {style.label}
                    </span>
                    <span className="font-mono text-[10px] tracking-wider uppercase text-muted-foreground">
                      {phase.window}
                    </span>
                  </div>

                  <p className="text-sm md:text-base text-muted-foreground leading-relaxed max-w-3xl mb-4">
                    {phase.summary}
                  </p>

                  <ul className="flex flex-wrap gap-2">
                    {phase.items.map((item) => (
                      <li
                        key={item}
                        className="font-mono text-[11px] tracking-wide text-secondary-foreground border border-border bg-card rounded-sm px-3 py-1.5"
                      >
                        {item}
                      </li>
                    ))}
                  </ul>
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </section>
  );
}
