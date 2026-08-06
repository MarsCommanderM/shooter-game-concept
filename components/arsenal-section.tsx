"use client";

import { useEffect, useRef, useState } from "react";

const weapons = [
  {
    name: "Bio-Korrosiv",
    type: "Organisch",
    description:
      "Verschie\u00dft s\u00e4urehaltige Sporen, die organisches Gewebe der Biomasse aufl\u00f6sen. Extrem effektiv gegen den Wirrwarr, wirkungslos gegen Metall.",
    effectiveness: { organic: 95, mechanical: 15 },
  },
  {
    name: "EMP-Railgun",
    type: "Mechanisch",
    description:
      "Ein gro\u00dfkalibriges Gauss-Gesch\u00fctz. Durchschl\u00e4gt mechanische Panzerung und sprengt Tunnel durch Geb\u00e4udew\u00e4nde. Der Schl\u00fcssel zur Destruktion.",
    effectiveness: { organic: 40, mechanical: 90 },
  },
  {
    name: "Hybrid-Sturmgewehr",
    type: "Universal",
    description:
      "Umschaltbare Munitionstypen f\u00fcr flexible Gefechte. Der Allrounder f\u00fcr Spieler, die schnell zwischen Zielarten wechseln m\u00fcssen.",
    effectiveness: { organic: 65, mechanical: 65 },
  },
];

function EffBar({ label, value }: { label: string; value: number }) {
  return (
    <div className="flex items-center gap-3">
      <span className="font-mono text-[10px] text-muted-foreground tracking-wider uppercase w-20 shrink-0">
        {label}
      </span>
      <div className="flex-1 h-1.5 rounded-full bg-secondary overflow-hidden">
        <div
          className="h-full rounded-full bg-primary"
          style={{ width: `${value}%` }}
        />
      </div>
      <span className="font-mono text-[10px] text-primary w-8 text-right">
        {value}%
      </span>
    </div>
  );
}

export function ArsenalSection() {
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
    <section ref={sectionRef} id="arsenal" className="relative py-20 md:py-32 px-6">
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            05 // Arsenal
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-4 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Waffen mit
          <span className="text-primary glow-neon-sm"> Konsequenzen</span>
        </h2>

        <p
          className={`text-base md:text-lg text-muted-foreground max-w-2xl mb-16 leading-relaxed transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Jede Waffe hat eine unterschiedliche Wirkung gegen organische und
          mechanische Ziele. Die richtige Wahl entscheidet &uuml;ber Sieg oder
          Niederlage &ndash; und dar&uuml;ber, wie viel vom Schlachtfeld &uuml;brig
          bleibt.
        </p>

        <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
          {weapons.map((weapon, index) => (
            <div
              key={weapon.name}
              className={`group flex flex-col p-6 bg-card border border-border rounded-sm hover:border-primary/30 transition-all duration-500 ${
                isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-12"
              }`}
              style={{ transitionDelay: `${300 + index * 150}ms` }}
            >
              <span className="inline-flex self-start px-3 py-1 mb-4 font-mono text-[10px] tracking-wider uppercase border border-primary/30 text-primary rounded-sm">
                {weapon.type}
              </span>
              <h3 className="text-xl font-semibold text-foreground mb-3">
                {weapon.name}
              </h3>
              <p className="text-sm text-muted-foreground leading-relaxed mb-6 flex-1">
                {weapon.description}
              </p>
              <div className="space-y-2">
                <EffBar label="Organisch" value={weapon.effectiveness.organic} />
                <EffBar
                  label="Mechanisch"
                  value={weapon.effectiveness.mechanical}
                />
              </div>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
