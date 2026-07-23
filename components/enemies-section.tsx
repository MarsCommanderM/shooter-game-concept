"use client";

import { useEffect, useRef, useState } from "react";

const enemies = [
  {
    name: "Br\u00fcter",
    threat: "Niedrig",
    description:
      "Kriechende Biomasse-Larven, die in Schw\u00e4rmen angreifen. Einzeln harmlos, in Masse t\u00f6dlich. Nutzen enge Korridore und zerst\u00f6rte W\u00e4nde als Zug\u00e4nge.",
  },
  {
    name: "Wandler",
    threat: "Mittel",
    description:
      "Ehemalige Menschen, vom Wirrwarr \u00fcbernommen. Sie erinnern sich an Taktik und Deckung \u2013 ein verst\u00f6render Gegner, der wie ein Mensch k\u00e4mpft.",
  },
  {
    name: "Koloss",
    threat: "Hoch",
    description:
      "Eine wandelnde Festung aus verschmolzenem Beton und Biomasse. Nur schwere Waffen und gezielte Umgebungszerst\u00f6rung bringen ihn zu Fall.",
  },
  {
    name: "Der Kern",
    threat: "Boss",
    description:
      "Das pulsierende Zentrum eines Wirrwarr-Nests. Steuert alle umliegenden Kreaturen. Seine Zerst\u00f6rung befreit einen ganzen Sektor der Karte.",
  },
];

const threatColor: Record<string, string> = {
  Niedrig: "text-muted-foreground border-border",
  Mittel: "text-accent border-accent/40",
  Hoch: "text-primary border-primary/40",
  Boss: "text-destructive border-destructive/50",
};

export function EnemiesSection() {
  const [isVisible, setIsVisible] = useState(false);
  const sectionRef = useRef<HTMLElement>(null);

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) setIsVisible(true);
      },
      { threshold: 0.1 }
    );
    if (sectionRef.current) observer.observe(sectionRef.current);
    return () => observer.disconnect();
  }, []);

  return (
    <section
      ref={sectionRef}
      id="enemies"
      className="relative py-20 md:py-32 px-6 bg-[hsl(var(--surface))]"
    >
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            06 // Bedrohungen
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-4 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Die Kreaturen des
          <span className="text-primary glow-neon-sm"> Wirrwarr</span>
        </h2>

        <p
          className={`text-base md:text-lg text-muted-foreground max-w-2xl mb-16 leading-relaxed transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Die Biomasse bringt eine Hierarchie von Gegnern hervor &ndash; von
          Schw&auml;rmen kriechender Larven bis zu den kartenbeherrschenden
          Nest-Kernen.
        </p>

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-6">
          {enemies.map((enemy, index) => (
            <div
              key={enemy.name}
              className={`group relative p-6 md:p-8 bg-card border border-border rounded-sm hover:border-primary/30 transition-all duration-500 ${
                isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-12"
              }`}
              style={{ transitionDelay: `${300 + index * 120}ms` }}
            >
              <div className="flex items-center justify-between mb-4">
                <h3 className="text-xl md:text-2xl font-bold text-foreground">
                  {enemy.name}
                </h3>
                <span
                  className={`px-3 py-1 font-mono text-[10px] tracking-wider uppercase border rounded-sm ${threatColor[enemy.threat]}`}
                >
                  {enemy.threat}
                </span>
              </div>
              <p className="text-sm text-muted-foreground leading-relaxed">
                {enemy.description}
              </p>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
