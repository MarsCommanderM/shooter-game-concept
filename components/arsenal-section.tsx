"use client";

import { useEffect, useRef, useState } from "react";
import { Dna, Cog } from "lucide-react";

type WeaponClass = "organisch" | "mechanisch";

interface Weapon {
  id: string;
  name: string;
  role: string;
  desc: string;
  trait: string;
  strongVs: "organisch" | "mechanisch" | "universal";
  stats: { label: string; value: number }[];
}

const CLASSES: Record<
  WeaponClass,
  { title: string; icon: typeof Dna; blurb: string; weapons: Weapon[] }
> = {
  organisch: {
    title: "Organisch",
    icon: Dna,
    blurb:
      "Gezüchtet aus der Biomasse. Leicht, leise und regenerierend – tödlich gegen organisches Gewebe, aber schwach gegen Panzerung.",
    weapons: [
      {
        id: "dorn",
        name: "DORN",
        role: "Sturmgewehr",
        desc: "Feuert gehärtete Biomass-Dorne mit hoher Kadenz. Der Allrounder für mittlere Distanzen – wächst buchstäblich in deine Hand.",
        trait: "Bio-Regeneration: regeneriert Munition langsam aus umliegender Biomasse.",
        strongVs: "organisch",
        stats: [
          { label: "Schaden", value: 55 },
          { label: "Feuerrate", value: 70 },
          { label: "Reichweite", value: 60 },
          { label: "Zerstörung", value: 30 },
          { label: "Mobilität", value: 70 },
        ],
      },
      {
        id: "nessel",
        name: "NESSEL",
        role: "Spitter-SMG",
        desc: "Versprüht ätzende Nesselsalven im Nahkampf. Zersetzt Deckungen und zwingt Gegner aus jeder Position.",
        trait: "Ätzend: Treffer verursachen Schaden über Zeit und zersetzen organische Deckung.",
        strongVs: "organisch",
        stats: [
          { label: "Schaden", value: 40 },
          { label: "Feuerrate", value: 90 },
          { label: "Reichweite", value: 30 },
          { label: "Zerstörung", value: 45 },
          { label: "Mobilität", value: 85 },
        ],
      },
      {
        id: "weide",
        name: "WEIDE",
        role: "Organisches Präzisionsgewehr",
        desc: "Beschleunigt einen einzelnen Nervendorn auf Überschall. Wer getroffen wird, lebt lange genug, um es zu bereuen.",
        trait: "Nerven-Sinn: Treffer markieren das Ziel kurzzeitig für dein gesamtes Team.",
        strongVs: "universal",
        stats: [
          { label: "Schaden", value: 90 },
          { label: "Feuerrate", value: 15 },
          { label: "Reichweite", value: 95 },
          { label: "Zerstörung", value: 25 },
          { label: "Mobilität", value: 40 },
        ],
      },
    ],
  },
  mechanisch: {
    title: "Mechanisch",
    icon: Cog,
    blurb:
      "Klassische Ballistik, militärische Doktrin. Schwer, laut und kompromisslos – reißt Panzerung, Maschinen und ganze Wände nieder.",
    weapons: [
      {
        id: "brecher",
        name: "BRECHER-7",
        role: "Abbruchkanone",
        desc: "Verschiebt 40-mm-Granaten mit hoher Wanddurchschlagskraft. Das Werkzeug der Wahl, um die Karte neu zu zeichnen.",
        trait: "Wallbreaker: sprengt passierbare Breschen in nahezu jede Struktur.",
        strongVs: "mechanisch",
        stats: [
          { label: "Schaden", value: 80 },
          { label: "Feuerrate", value: 25 },
          { label: "Reichweite", value: 50 },
          { label: "Zerstörung", value: 100 },
          { label: "Mobilität", value: 35 },
        ],
      },
      {
        id: "tempest",
        name: "TEMPEST",
        role: "Maschinenpistole",
        desc: "Präzise gefertigt, brutal schnell. Der Standard für Parkour-Spieler, die zwischen den Lines tanzen.",
        trait: "Leichtlauf: keine Bewegungseinschränkung beim Feuern – gebaut für Wandlauf-Kämpfe.",
        strongVs: "universal",
        stats: [
          { label: "Schaden", value: 45 },
          { label: "Feuerrate", value: 85 },
          { label: "Reichweite", value: 40 },
          { label: "Zerstörung", value: 20 },
          { label: "Mobilität", value: 90 },
        ],
      },
      {
        id: "richter",
        name: "RICHTER-50",
        role: "Anti-Material-Gewehr",
        desc: "Ein Satz, ein Urteil. Durchschlägt mechanische Ziele und die Wand dahinter – in dieser Reihenfolge.",
        trait: "Panzerbrechend: Bonusschaden und Schild-Störung gegen mechanische Ziele.",
        strongVs: "mechanisch",
        stats: [
          { label: "Schaden", value: 100 },
          { label: "Feuerrate", value: 10 },
          { label: "Reichweite", value: 100 },
          { label: "Zerstörung", value: 60 },
          { label: "Mobilität", value: 30 },
        ],
      },
    ],
  },
};

const VS_LABEL: Record<Weapon["strongVs"], string> = {
  organisch: "Effektiv vs. Organisch",
  mechanisch: "Effektiv vs. Mechanisch",
  universal: "Universal einsetzbar",
};

export function ArsenalSection() {
  const [activeClass, setActiveClass] = useState<WeaponClass>("organisch");
  const [activeWeapon, setActiveWeapon] = useState(0);
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

  const cls = CLASSES[activeClass];
  const weapon = cls.weapons[activeWeapon] ?? cls.weapons[0];

  const switchClass = (c: WeaponClass) => {
    setActiveClass(c);
    setActiveWeapon(0);
  };

  return (
    <section ref={sectionRef} id="arsenal" className="relative py-20 md:py-32 px-6">
      <div className="max-w-6xl mx-auto">
        {/* Section Label */}
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            06 // Arsenal
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-12 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Zwei Doktrinen,
          <span className="text-primary glow-neon-sm"> ein Arsenal</span>
        </h2>

        {/* Class Toggle */}
        <div
          className={`grid grid-cols-2 gap-2 mb-8 max-w-xl transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {(Object.keys(CLASSES) as WeaponClass[]).map((key) => {
            const c = CLASSES[key];
            const Icon = c.icon;
            const active = key === activeClass;
            return (
              <button
                key={key}
                type="button"
                onClick={() => switchClass(key)}
                aria-pressed={active}
                className={`flex items-center justify-center gap-2 px-4 py-3.5 rounded-sm border font-mono text-sm tracking-wider uppercase transition-all min-h-[44px] ${
                  active
                    ? "bg-primary text-primary-foreground border-primary box-glow-neon"
                    : "bg-secondary/60 text-secondary-foreground border-border hover:border-primary/40 hover:text-primary"
                }`}
              >
                <Icon className="w-4 h-4" strokeWidth={1.75} />
                {c.title}
              </button>
            );
          })}
        </div>

        <p
          className={`text-base text-muted-foreground max-w-3xl mb-8 leading-relaxed transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100" : "opacity-0"
          }`}
        >
          {cls.blurb}
        </p>

        <div className="grid lg:grid-cols-[260px_1fr] gap-6">
          {/* Weapon List */}
          <div
            className={`flex lg:flex-col gap-2 flex-wrap transition-all duration-700 delay-300 ${
              isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
            }`}
          >
            {cls.weapons.map((w, i) => (
              <button
                key={w.id}
                type="button"
                onClick={() => setActiveWeapon(i)}
                aria-pressed={i === activeWeapon}
                className={`text-left px-4 py-3.5 rounded-sm border transition-all min-h-[44px] ${
                  i === activeWeapon
                    ? "border-primary/60 bg-primary/10 box-glow-neon"
                    : "border-border bg-card hover:border-primary/30"
                }`}
              >
                <p
                  className={`font-bold tracking-wide ${
                    i === activeWeapon ? "text-primary glow-neon-sm" : "text-foreground"
                  }`}
                >
                  {w.name}
                </p>
                <p className="font-mono text-[10px] tracking-wider uppercase text-muted-foreground mt-0.5">
                  {w.role}
                </p>
              </button>
            ))}
          </div>

          {/* Weapon Detail */}
          <div
            key={weapon.id}
            className="animate-fade-in border border-border bg-card rounded-sm p-6 md:p-8"
          >
            <div className="flex flex-wrap items-start justify-between gap-4 mb-4">
              <div>
                <h3 className="text-2xl md:text-3xl font-bold text-foreground tracking-wide">
                  {weapon.name}
                </h3>
                <p className="font-mono text-xs text-muted-foreground tracking-wider uppercase mt-1">
                  {weapon.role} // Klasse {cls.title}
                </p>
              </div>
              <span className="font-mono text-[10px] tracking-wider uppercase border border-primary/40 text-primary bg-primary/10 rounded-sm px-3 py-1.5">
                {VS_LABEL[weapon.strongVs]}
              </span>
            </div>

            <p className="text-base text-secondary-foreground leading-relaxed mb-6">
              {weapon.desc}
            </p>

            {/* Stat Bars */}
            <div className="space-y-3 mb-6">
              {weapon.stats.map((s) => (
                <div key={s.label} className="grid grid-cols-[90px_1fr_36px] items-center gap-3">
                  <span className="font-mono text-[10px] tracking-[0.15em] uppercase text-muted-foreground">
                    {s.label}
                  </span>
                  <div className="h-1.5 bg-secondary rounded-full overflow-hidden">
                    <div
                      className="h-full bg-primary rounded-full transition-all duration-700"
                      style={{ width: `${s.value}%` }}
                    />
                  </div>
                  <span className="font-mono text-xs text-primary text-right">{s.value}</span>
                </div>
              ))}
            </div>

            {/* Trait */}
            <div className="border border-primary/30 bg-primary/5 rounded-sm px-4 py-3 flex items-start gap-3">
              <span className="mt-1.5 w-2 h-2 rounded-full bg-primary animate-pulse-neon shrink-0" />
              <p className="text-sm text-foreground leading-relaxed">
                <span className="font-mono text-[10px] tracking-[0.2em] uppercase text-primary mr-2">
                  Spezial
                </span>
                {weapon.trait}
              </p>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
