"use client";

import { useEffect, useRef, useState } from "react";
import { Crosshair, Users, Compass, Accessibility } from "lucide-react";

type HudMode = "combat" | "tactical";

const HUD_ELEMENTS = [
  {
    icon: Crosshair,
    title: "Adaptives Fadenkreuz",
    text: "Öffnet sich mit Spread, färbt sich bei Treffer-Confirm. In Biomass-Deckung zeigt es Wachstumsrichtung an.",
  },
  {
    icon: Users,
    title: "Squad-Status",
    text: "Deine zwei KI-Kameraden als kompakte Vital-Leisten links – inklusive Befehls-Cooldowns im Taktik-Kontext.",
  },
  {
    icon: Compass,
    title: "Objektiv-Kompass",
    text: "Nur, was relevant ist: Flaggen, Zonen, Sites und Marks deines Teams. Kein Müll, keine Minimap-Panik.",
  },
  {
    icon: Accessibility,
    title: "Barrierefreiheit",
    text: "Drei Farbenblind-Profile, Richtungssubtitles für Audio-Cues, Motion-Reduction (Scanlines & Shake aus), Aim-Assist-Stufen.",
  },
];

export function HudSection() {
  const [mode, setMode] = useState<HudMode>("combat");
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

  const tactical = mode === "tactical";

  return (
    <section ref={sectionRef} id="hud" className="relative py-20 md:py-32 px-6 scanlines">
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            09 // HUD &amp; UX
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-4 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Das Interface
          <span className="text-primary glow-neon-sm"> ist Teil der Welt</span>
        </h2>
        <p
          className={`text-base md:text-lg text-muted-foreground max-w-3xl mb-10 leading-relaxed transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100" : "opacity-0"
          }`}
        >
          Diegetisch statt zugekleistert: Deine Vitalwerte leben als Biomass-Adern
          am Waffenarm, Munition zählt ein organischer Counter. Der Taktik-Modus
          friert die Welt ein und legt das Befehls-Overlay darüber – probiere es aus.
        </p>

        {/* Mode Toggle */}
        <div
          className={`grid grid-cols-2 gap-2 max-w-md mb-6 transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {(
            [
              ["combat", "Kampfmodus"],
              ["tactical", "Taktik-Modus"],
            ] as [HudMode, string][]
          ).map(([id, label]) => (
            <button
              key={id}
              type="button"
              onClick={() => setMode(id)}
              aria-pressed={mode === id}
              className={`px-4 py-3 rounded-sm border font-mono text-xs sm:text-sm tracking-wider uppercase transition-all min-h-[44px] ${
                mode === id
                  ? "bg-primary text-primary-foreground border-primary box-glow-neon"
                  : "bg-secondary/60 text-secondary-foreground border-border hover:border-primary/40 hover:text-primary"
              }`}
            >
              {label}
            </button>
          ))}
        </div>

        {/* HUD Mockup */}
        <div
          className={`relative aspect-video max-w-4xl rounded-sm overflow-hidden border transition-all duration-500 ${
            tactical ? "border-primary/60 box-glow-neon" : "border-border"
          } bg-[radial-gradient(ellipse_at_center,hsl(120_5%_10%)_0%,hsl(120_5%_4%)_100%)] transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {/* Tactical grid overlay */}
          {tactical && (
            <div
              className="absolute inset-0 animate-fade-in"
              style={{
                background:
                  "linear-gradient(hsl(130 100% 50% / 0.06) 1px, transparent 1px), linear-gradient(90deg, hsl(130 100% 50% / 0.06) 1px, transparent 1px)",
                backgroundSize: "40px 40px",
              }}
            />
          )}
          <div className="absolute inset-0 scanlines pointer-events-none" />

          {/* Top: Objective compass */}
          <div className="absolute top-3 left-1/2 -translate-x-1/2 flex flex-col items-center gap-1">
            <div className="font-mono text-[10px] tracking-[0.3em] text-primary/90 uppercase">
              {tactical ? "OBJEKTIV-PLANUNG" : "A — 42 m"}
            </div>
            <div className="w-40 h-4 border-y border-primary/30 flex items-center justify-between px-1 font-mono text-[8px] text-muted-foreground">
              <span>W</span><span className="text-primary">▲</span><span>O</span>
            </div>
          </div>

          {/* Left: Squad */}
          <div className="absolute left-3 top-1/2 -translate-y-1/2 space-y-2">
            {["VEGA", "JUNO"].map((name, i) => (
              <div key={name} className="flex items-center gap-2">
                <span className={`font-mono text-[9px] tracking-wider ${tactical ? "text-primary" : "text-muted-foreground"}`}>
                  {name}
                </span>
                <div className="w-14 h-1 bg-secondary rounded-full overflow-hidden">
                  <div
                    className="h-full bg-primary rounded-full"
                    style={{ width: i === 0 ? "80%" : "55%" }}
                  />
                </div>
              </div>
            ))}
            {tactical && (
              <div className="pt-1 space-y-1 animate-fade-in">
                {["BEWEGEN", "HALTEN", "BREACH"].map((c) => (
                  <span key={c} className="block font-mono text-[8px] tracking-wider text-primary border border-primary/40 bg-primary/10 rounded-sm px-1.5 py-0.5 w-max">
                    {c}
                  </span>
                ))}
              </div>
            )}
          </div>

          {/* Center crosshair / waypoints */}
          <div className="absolute inset-0 flex items-center justify-center">
            {!tactical ? (
              <svg viewBox="0 0 48 48" className="w-12 h-12 text-primary" fill="none" stroke="currentColor" strokeWidth="1.5">
                <circle cx="24" cy="24" r="1.5" fill="currentColor" stroke="none" />
                <path d="M24 10v6M24 32v6M10 24h6M32 24h6" />
              </svg>
            ) : (
              <div className="relative w-full h-full animate-fade-in">
                <span className="absolute left-[38%] top-[42%] w-6 h-6 border border-primary rotate-45 flex items-center justify-center">
                  <span className="font-mono text-[9px] text-primary -rotate-45">A</span>
                </span>
                <span className="absolute left-[62%] top-[58%] w-6 h-6 border border-primary/50 rotate-45 flex items-center justify-center">
                  <span className="font-mono text-[9px] text-primary/70 -rotate-45">B</span>
                </span>
                <svg className="absolute inset-0 w-full h-full" fill="none" preserveAspectRatio="none" viewBox="0 0 400 225">
                  <path d="M 200 112 L 156 95" stroke="hsl(130 100% 50% / 0.5)" strokeDasharray="4 4" />
                </svg>
                <span className="absolute top-3 right-3 font-mono text-[10px] tracking-[0.25em] text-primary border border-primary/50 bg-primary/10 px-2 py-1 rounded-sm animate-pulse-neon">
                  TAKTIK // ZEIT ANGEHALTEN
                </span>
              </div>
            )}
          </div>

          {/* Bottom left: vitals */}
          <div className="absolute bottom-3 left-3">
            <p className="font-mono text-[9px] tracking-wider text-muted-foreground mb-1">
              BIOMASS-INTEGRITÄT
            </p>
            <div className="w-36 h-1.5 bg-secondary rounded-full overflow-hidden">
              <div className="h-full w-[72%] bg-primary rounded-full" />
            </div>
            <p className="font-mono text-lg text-primary leading-none mt-1">72</p>
          </div>

          {/* Bottom right: ammo */}
          <div className="absolute bottom-3 right-3 text-right">
            <p className="font-mono text-[9px] tracking-wider text-muted-foreground mb-1">
              {tactical ? "DORN // RESERVE" : "DORN"}
            </p>
            <p className="font-mono text-2xl text-foreground leading-none">
              24<span className="text-muted-foreground text-base"> / 90</span>
            </p>
          </div>
        </div>

        {/* Elements + Accessibility */}
        <div className="grid sm:grid-cols-2 lg:grid-cols-4 gap-4 mt-10">
          {HUD_ELEMENTS.map((el, i) => {
            const Icon = el.icon;
            return (
              <div
                key={el.title}
                className={`border border-border bg-card rounded-sm p-5 hover:border-primary/40 transition-all transition-all duration-700 ${
                  isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
                }`}
                style={{ transitionDelay: `${300 + i * 100}ms` }}
              >
                <div className="flex items-center gap-3 mb-3">
                  <span className="w-9 h-9 rounded-sm border border-primary/30 bg-primary/10 flex items-center justify-center">
                    <Icon className="w-4 h-4 text-primary" strokeWidth={1.75} />
                  </span>
                  <h3 className="text-sm font-bold text-foreground leading-tight">{el.title}</h3>
                </div>
                <p className="text-sm text-muted-foreground leading-relaxed">{el.text}</p>
              </div>
            );
          })}
        </div>
      </div>
    </section>
  );
}
