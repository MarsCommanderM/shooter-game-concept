"use client";

import { useEffect, useRef, useState } from "react";
import Image from "next/image";

interface GameMap {
  id: string;
  name: string;
  sub: string;
  size: "S" | "M" | "L";
  destruction: number;
  verticality: "Niedrig" | "Mittel" | "Hoch";
  modes: string[];
  desc: string;
  note: string;
}

const MAPS: GameMap[] = [
  {
    id: "sektor7",
    name: "SEKTOR 7 – DIE SPIRE",
    sub: "Vertikale Ruinenstadt",
    size: "M",
    destruction: 65,
    verticality: "Hoch",
    modes: ["TDM", "CTF", "DOM"],
    desc: "Ein gefallener Stadtbezirk, dessen zentraler Orbital-Lift wie ein Speer in den Smog ragt. Zip-Lines, Kranarme und zerbrochene Skybridges machen die Vertikale zur dritten Kampfachse.",
    note: "Die Skybridge zwischen Turm A und B ist sprengbar – wer sie fällt, kappt die schnellste Rotationsroute für den Rest der Runde.",
  },
  {
    id: "garten",
    name: "BIOMASS-GARTEN",
    sub: "Überwuchertes Forschungskomplex",
    size: "S",
    destruction: 40,
    verticality: "Mittel",
    modes: ["HQ", "SAB", "FFA"],
    desc: "Treibhaus-Kuppeln, in denen die Biomass einst gezüchtet wurde – und die sich zurückerobert haben, was ihr gehörte. Enge Sichtlinien, wuchernde Deckungen und ein Herz aus pulsierendem Gewebe.",
    note: "Das zentrale HQ im Kuppelkern ist nur über zwei Glasbrücken oder einen gesprengten Lüftungsschacht erreichbar.",
  },
  {
    id: "stahlwiege",
    name: "STAHLWIEGE",
    sub: "Verlassene Fertigungsanlage",
    size: "L",
    destruction: 90,
    verticality: "Mittel",
    modes: ["SAB", "DOM", "TDM"],
    desc: "Die Fabrik, in der die ersten Kriegsmaschinen vom Band liefen – jetzt ein Labyrinth aus Hallen, Kranbahnen und dünnen Wänden. Die zerstörbarste Karte im Spiel: fast jede Wand ist eine künftige Tür.",
    note: "Site B liegt hinter einer doppelten Produktionswand. Ein BRECHER-7-Schuss eröffnet hier eine komplett neue Angriffsachse.",
  },
];

function MapDiagram({ id }: { id: string }) {
  const stroke = "hsl(130 100% 50%)";
  const strokeDim = "hsl(130 100% 50% / 0.35)";
  const muted = "hsl(80 5% 55%)";
  const fillDim = "hsl(130 100% 50% / 0.06)";

  const label = (x: number, y: number, text: string, color = muted) => (
    <text x={x} y={y} fill={color} fontSize="7" fontFamily="monospace" textAnchor="middle" letterSpacing="1">
      {text}
    </text>
  );

  return (
    <svg viewBox="0 0 220 120" className="w-full h-auto" role="img" aria-label="Kartenlayout">
      <rect x="4" y="4" width="212" height="112" fill={fillDim} stroke={strokeDim} strokeWidth="1" />

      {id === "sektor7" && (
        <g>
          <rect x="24" y="20" width="30" height="34" fill="none" stroke={muted} strokeWidth="1" />
          {label(39, 40, "TURM A")}
          <rect x="166" y="64" width="30" height="34" fill="none" stroke={muted} strokeWidth="1" />
          {label(181, 84, "TURM B")}
          <rect x="98" y="46" width="24" height="28" fill="none" stroke={stroke} strokeWidth="1.5" />
          {label(110, 63, "SPIRE", stroke)}
          {/* Skybridge, sprengbar */}
          <line x1="54" y1="37" x2="98" y2="52" stroke={stroke} strokeWidth="1" strokeDasharray="4 3" />
          <path d="M72 42 l4 -3 l2 5 l4 -4" stroke={stroke} fill="none" strokeWidth="1" />
          {label(76, 32, "SKYBRIDGE", stroke)}
          <line x1="122" y1="66" x2="166" y2="78" stroke={muted} strokeWidth="1" strokeDasharray="2 3" />
          {label(144, 60, "ZIP-LINE")}
          {label(110, 108, "VERTIKALITÄT: HOCH")}
        </g>
      )}

      {id === "garten" && (
        <g>
          <circle cx="60" cy="44" r="22" fill="none" stroke={muted} strokeWidth="1" />
          {label(60, 47, "KUPPEL 1")}
          <circle cx="160" cy="44" r="22" fill="none" stroke={muted} strokeWidth="1" />
          {label(160, 47, "KUPPEL 2")}
          <circle cx="110" cy="76" r="18" fill="none" stroke={stroke} strokeWidth="1.5" />
          {label(110, 79, "KERN", stroke)}
          <line x1="76" y1="56" x2="96" y2="66" stroke={strokeDim} strokeWidth="1" />
          <line x1="144" y1="56" x2="124" y2="66" stroke={strokeDim} strokeWidth="1" />
          <path d="M104 96 l4 -3 l2 5 l4 -4" stroke={stroke} fill="none" strokeWidth="1" />
          {label(110, 108, "LÜFTUNGSSCHACHT = BREACH", stroke)}
        </g>
      )}

      {id === "stahlwiege" && (
        <g>
          <rect x="20" y="24" width="52" height="32" fill="none" stroke={muted} strokeWidth="1" />
          {label(46, 43, "HALLE 1")}
          <rect x="84" y="24" width="52" height="32" fill="none" stroke={muted} strokeWidth="1" />
          {label(110, 43, "HALLE 2")}
          <rect x="148" y="24" width="52" height="32" fill="none" stroke={muted} strokeWidth="1" />
          {label(174, 43, "HALLE 3")}
          <rect x="52" y="74" width="26" height="22" fill="none" stroke={stroke} strokeWidth="1.5" />
          {label(65, 88, "A", stroke)}
          <rect x="142" y="74" width="26" height="22" fill="none" stroke={stroke} strokeWidth="1.5" />
          {label(155, 88, "B", stroke)}
          <line x1="72" y1="40" x2="84" y2="40" stroke={stroke} strokeWidth="2" />
          <path d="M74 40 l3 -3 l2 4 l3 -3" stroke={stroke} fill="none" strokeWidth="1" />
          {label(110, 108, "DOPPELWAND = BRECHER-ZIEL", stroke)}
        </g>
      )}
    </svg>
  );
}

export function MapsSection() {
  const [activeMap, setActiveMap] = useState(0);
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

  const map = MAPS[activeMap] ?? MAPS[0];

  return (
    <section ref={sectionRef} id="maps" className="relative py-20 md:py-32 px-6 scanlines">
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            07 // Karten
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-12 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Karten, die sich
          <span className="text-primary glow-neon-sm"> an dich erinnern</span>
        </h2>

        {/* Key Art */}
        <div
          className={`relative aspect-[21/9] rounded-sm overflow-hidden border border-border border-glow mb-10 transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <Image
            src="/images/map-spire.jpg"
            alt="Luftaufnahme von Sektor 7: vertikale Ruinenstadt mit leuchtendem Orbital-Lift"
            fill
            className="object-cover"
          />
          <div className="absolute inset-0 bg-gradient-to-t from-background/80 via-transparent to-transparent" />
          <span className="absolute bottom-3 left-4 font-mono text-[10px] tracking-[0.25em] uppercase text-primary glow-neon-sm">
            Konzept-Key-Art // Sektor 7 – Die Spire
          </span>
        </div>

        {/* Map Selector */}
        <div
          className={`grid sm:grid-cols-3 gap-2 mb-8 transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {MAPS.map((m, i) => (
            <button
              key={m.id}
              type="button"
              onClick={() => setActiveMap(i)}
              aria-pressed={i === activeMap}
              className={`text-left px-4 py-3.5 rounded-sm border transition-all min-h-[44px] ${
                i === activeMap
                  ? "border-primary/60 bg-primary/10 box-glow-neon"
                  : "border-border bg-card hover:border-primary/30"
              }`}
            >
              <p className={`font-bold text-sm tracking-wide ${i === activeMap ? "text-primary glow-neon-sm" : "text-foreground"}`}>
                {m.name}
              </p>
              <p className="font-mono text-[10px] tracking-wider uppercase text-muted-foreground mt-0.5">
                {m.sub}
              </p>
            </button>
          ))}
        </div>

        {/* Map Detail */}
        <div
          key={map.id}
          className="animate-fade-in grid lg:grid-cols-2 gap-0 border border-border bg-card rounded-sm overflow-hidden box-glow-neon"
        >
          <div className="p-6 md:p-8 border-b lg:border-b-0 lg:border-r border-border">
            <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-3">
              Taktisches Layout
            </p>
            <div className="border border-border rounded-sm bg-background/60 p-3 mb-6">
              <MapDiagram id={map.id} />
            </div>
            <p className="text-sm text-secondary-foreground leading-relaxed">{map.desc}</p>
          </div>

          <div className="p-6 md:p-8 flex flex-col gap-6">
            <div className="grid grid-cols-3 gap-px bg-border border border-border rounded-sm overflow-hidden">
              {[
                ["Größe", map.size],
                ["Zerstörbar", `${map.destruction} %`],
                ["Vertikalität", map.verticality],
              ].map(([k, v]) => (
                <div key={k} className="bg-card px-4 py-3">
                  <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-1">{k}</p>
                  <p className="text-sm text-foreground font-medium">{v}</p>
                </div>
              ))}
            </div>

            <div>
              <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-primary mb-3 glow-neon-sm">
                Ausgelegte Modi
              </p>
              <div className="flex flex-wrap gap-2">
                {map.modes.map((mode) => (
                  <span
                    key={mode}
                    className="font-mono text-xs tracking-wider uppercase border border-primary/40 text-primary bg-primary/10 rounded-sm px-3 py-1.5"
                  >
                    {mode}
                  </span>
                ))}
              </div>
            </div>

            <div className="border border-primary/30 bg-primary/5 rounded-sm px-4 py-3 flex items-start gap-3">
              <span className="mt-1.5 w-2 h-2 rounded-full bg-primary animate-pulse-neon shrink-0" />
              <p className="text-sm text-foreground leading-relaxed">
                <span className="font-mono text-[10px] tracking-[0.2em] uppercase text-primary mr-2">
                  Designer-Note
                </span>
                {map.note}
              </p>
            </div>

            {/* Destruction bar */}
            <div>
              <div className="flex justify-between items-baseline mb-2">
                <span className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground">
                  Zerstörungsgrad der Struktur
                </span>
                <span className="font-mono text-xs text-primary">{map.destruction} %</span>
              </div>
              <div className="h-1.5 bg-secondary rounded-full overflow-hidden">
                <div
                  className="h-full bg-primary rounded-full transition-all duration-700"
                  style={{ width: `${map.destruction}%` }}
                />
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
