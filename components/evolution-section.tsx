"use client";

import { useEffect, useRef, useState } from "react";
import { Eye, Shield, Zap, Lock } from "lucide-react";

type PathId = "sinne" | "panzer" | "mobilitaet";

interface EvolutionNode {
  id: string;
  tier: 1 | 2 | 3;
  name: string;
  desc: string;
  cost: number;
}

interface EvolutionPath {
  id: PathId;
  title: string;
  icon: typeof Eye;
  blurb: string;
  nodes: EvolutionNode[];
}

const POINT_BUDGET = 5;

const PATHS: EvolutionPath[] = [
  {
    id: "sinne",
    title: "Sinne",
    icon: Eye,
    blurb: "Die Biomass sieht, was Menschen verborgen bleibt. Verzerrte Wahrnehmung als Waffe.",
    nodes: [
      {
        id: "s1",
        tier: 1,
        name: "Schwarm-Sinn",
        desc: "Du nimmst verborgene Gegner in 15 m als verzerrte Silhouetten wahr – dein Sichtfeld flackert dabei grotesk.",
        cost: 1,
      },
      {
        id: "s2",
        tier: 2,
        name: "Echo-Blick",
        desc: "Nach einem eigenen Treffer siehst du das Ziel 3 s lang durch Wände. Die Karte hat keine Geheimnisse.",
        cost: 2,
      },
      {
        id: "s3",
        tier: 3,
        name: "Raubtier-Markierung",
        desc: "Gegner unter 30 % HP werden für dich und dein Team markiert. Blut riecht man nicht – man sieht es.",
        cost: 2,
      },
    ],
  },
  {
    id: "panzer",
    title: "Carapax",
    icon: Shield,
    blurb: "Dein Körper wächst über sich hinaus hinaus: Panzerung, Dornen, ein zweites Leben.",
    nodes: [
      {
        id: "p1",
        tier: 1,
        name: "Dermale Platten",
        desc: "Bio-Panzerung unter der Haut: 10 % weniger Schaden aus allen Quellen. Sieht verstörend aus. Funktioniert.",
        cost: 1,
      },
      {
        id: "p2",
        tier: 2,
        name: "Dornen-Reflex",
        desc: "Nahkampfangriffe auf dich werden mit Dornen-Schaden beantwortet. Umarmungen sind tödlich – für beide.",
        cost: 2,
      },
      {
        id: "p3",
        tier: 3,
        name: "Chitin-Overdrive",
        desc: "Einmal pro Runde: Beim Tod härtet dein Carapax aus und du stehst mit 50 % HP wieder auf.",
        cost: 2,
      },
    ],
  },
  {
    id: "mobilitaet",
    title: "Fortbewegung",
    icon: Zap,
    blurb: "Sehnen wie Federn, Muskeln wie Hydraulik. Die Stadt wird zum Parcours.",
    nodes: [
      {
        id: "m1",
        tier: 1,
        name: "Wandläufer+",
        desc: "Wallrun-Distanz +40 %. Jede Wand ist ein Weg, jeder Hinterhof eine Festung, die keine ist.",
        cost: 1,
      },
      {
        id: "m2",
        tier: 2,
        name: "Mantis-Sprung",
        desc: "Doppelsprung in der Luft. Vertikale Kämpfe gehören dir – die Spire wird dein Zuhause.",
        cost: 2,
      },
      {
        id: "m3",
        tier: 3,
        name: "Phasen-Gleiten",
        desc: "Slides werden 50 % länger und enden in einem kurzen Geschwindigkeits-Burst. Unfassbar schwer zu treffen.",
        cost: 2,
      },
    ],
  },
];

export function EvolutionSection() {
  const [activePath, setActivePath] = useState<PathId>("sinne");
  const [unlocked, setUnlocked] = useState<Record<string, boolean>>({});
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

  const path = PATHS.find((p) => p.id === activePath) ?? PATHS[0];

  const spent = PATHS.reduce(
    (sum, p) =>
      sum + p.nodes.reduce((s, n) => s + (unlocked[n.id] ? n.cost : 0), 0),
    0
  );
  const remaining = POINT_BUDGET - spent;

  const isNodeAvailable = (node: EvolutionNode) => {
    if (node.tier === 1) return true;
    const prev = path.nodes.find((n) => n.tier === node.tier - 1);
    return !!prev && !!unlocked[prev.id];
  };

  const toggleNode = (node: EvolutionNode) => {
    if (unlocked[node.id]) {
      // Nur entsperren, wenn keine höhere Stufe darauf aufbaut
      const next = path.nodes.find((n) => n.tier === node.tier + 1);
      if (next && unlocked[next.id]) return;
      setUnlocked((u) => ({ ...u, [node.id]: false }));
      return;
    }
    if (remaining < node.cost || !isNodeAvailable(node)) return;
    setUnlocked((u) => ({ ...u, [node.id]: true }));
  };

  return (
    <section ref={sectionRef} id="evolution" className="relative py-20 md:py-32 px-6">
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            08 // Evolution
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <div
          className={`flex flex-wrap items-end justify-between gap-6 mb-12 transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <h2 className="text-3xl sm:text-4xl md:text-5xl font-bold text-foreground leading-tight text-balance">
            Dein Körper
            <span className="text-primary glow-neon-sm"> ist das Meta</span>
          </h2>
          <div className="border border-primary/40 bg-primary/10 rounded-sm px-4 py-2.5 box-glow-neon">
            <span className="font-mono text-xs tracking-wider uppercase text-primary">
              Genetische Codes: {remaining} / {POINT_BUDGET}
            </span>
          </div>
        </div>

        {/* Path Selector */}
        <div
          className={`grid grid-cols-3 gap-2 mb-8 max-w-2xl transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {PATHS.map((p) => {
            const Icon = p.icon;
            const active = p.id === activePath;
            return (
              <button
                key={p.id}
                type="button"
                onClick={() => setActivePath(p.id)}
                aria-pressed={active}
                className={`flex items-center justify-center gap-2 px-3 py-3.5 rounded-sm border font-mono text-xs sm:text-sm tracking-wider uppercase transition-all min-h-[44px] ${
                  active
                    ? "bg-primary text-primary-foreground border-primary box-glow-neon"
                    : "bg-secondary/60 text-secondary-foreground border-border hover:border-primary/40 hover:text-primary"
                }`}
              >
                <Icon className="w-4 h-4" strokeWidth={1.75} />
                {p.title}
              </button>
            );
          })}
        </div>

        <p
          className={`text-base text-muted-foreground max-w-3xl mb-8 leading-relaxed transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100" : "opacity-0"
          }`}
        >
          {path.blurb}
        </p>

        {/* Node Chain */}
        <div
          key={path.id}
          className={`animate-fade-in grid md:grid-cols-3 gap-4 transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {path.nodes.map((node, i) => {
            const isUnlocked = !!unlocked[node.id];
            const available = isNodeAvailable(node);
            const affordable = remaining >= node.cost;
            const clickable = isUnlocked || (available && affordable);
            return (
              <div key={node.id} className="relative">
                {/* Connector */}
                {i < path.nodes.length - 1 && (
                  <div className="hidden md:block absolute top-1/2 -right-4 w-4 h-px bg-primary/40" />
                )}
                <button
                  type="button"
                  onClick={() => toggleNode(node)}
                  disabled={!clickable}
                  aria-pressed={isUnlocked}
                  className={`w-full text-left rounded-sm border p-5 transition-all min-h-[44px] ${
                    isUnlocked
                      ? "border-primary bg-primary/10 box-glow-neon"
                      : available
                        ? "border-border bg-card hover:border-primary/40 cursor-pointer"
                        : "border-border/60 bg-card/50 opacity-60 cursor-not-allowed"
                  }`}
                >
                  <div className="flex items-center justify-between mb-3">
                    <span className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground">
                      Stufe {node.tier}
                    </span>
                    <span className="flex items-center gap-2">
                      {!available && !isUnlocked && (
                        <Lock className="w-3.5 h-3.5 text-muted-foreground" strokeWidth={1.75} />
                      )}
                      <span
                        className={`font-mono text-[10px] tracking-wider uppercase border rounded-sm px-2 py-0.5 ${
                          isUnlocked
                            ? "border-primary text-primary"
                            : "border-border text-muted-foreground"
                        }`}
                      >
                        {node.cost} Code{node.cost > 1 ? "s" : ""}
                      </span>
                    </span>
                  </div>
                  <h3
                    className={`text-lg font-bold mb-2 ${
                      isUnlocked ? "text-primary glow-neon-sm" : "text-foreground"
                    }`}
                  >
                    {node.name}
                  </h3>
                  <p className="text-sm text-muted-foreground leading-relaxed">{node.desc}</p>
                  <p className="font-mono text-[10px] tracking-wider uppercase mt-3 text-muted-foreground">
                    {isUnlocked ? "● Integriert – tippen zum Entfernen" : available ? "○ Tippen zum Integrieren" : "○ Vorherige Stufe nötig"}
                  </p>
                </button>
              </div>
            );
          })}
        </div>

        <p
          className={`font-mono text-xs text-muted-foreground mt-6 transition-all duration-700 delay-500 ${
            isVisible ? "opacity-100" : "opacity-0"
          }`}
        >
          // Probier es aus: Integriere Upgrades mit deinen {POINT_BUDGET} Genetischen Codes – höhere Stufen erfordern die vorherige Stufe.
        </p>
      </div>
    </section>
  );
}
