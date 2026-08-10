"use client";

import { useEffect, useRef, useState } from "react";
import Image from "next/image";
import {
  Swords,
  Shield,
  Skull,
  Bomb,
  Flag,
  Crown,
  Server,
  Gauge,
  Trophy,
  ShieldCheck,
  Clapperboard,
  Users,
} from "lucide-react";

/* ------------------------------------------------------------------ */
/* Daten: Die sechs Multiplayer-Modi                                   */
/* ------------------------------------------------------------------ */

type ModeId = "tdm" | "hq" | "ffa" | "sabotage" | "ctf" | "domination";

interface GameMode {
  id: ModeId;
  label: string;
  name: string;
  tagline: string;
  players: string;
  teams: string;
  respawn: string;
  win: string;
  duration: string;
  description: string;
  twists: string[];
  tactics: string[];
  icon: typeof Swords;
}

const MODES: GameMode[] = [
  {
    id: "tdm",
    label: "TDM",
    name: "Team-Deathmatch",
    tagline: "Der klassische Team-Krieg: Präzision, Tempo, Kommunikation.",
    players: "5v5",
    teams: "2 Teams",
    respawn: "Sofort (3 s)",
    win: "75 Kills oder Zeitlimit",
    duration: "2 × 5 Minuten",
    description:
      "Zwei Squads treffen auf symmetrischen Arenen aufeinander. Jede Elimination zählt, jede Rotation entscheidet. Team-Deathmatch ist der reinste Skill-Test in WIRRWARR – schnell, brutal und ehrlich.",
    twists: [
      "Dynamic Destructibility verändert Lanes live in der Runde: Eine gesprengte Wand ist eine neue Flankroute – für beide Teams.",
      "Biomass-Zonen wachsen während der Partie und verschieben Deckungen dynamisch.",
    ],
    tactics: [
      "Chokepoints mit dem Taktik-Modus koordiniert durchbrechen.",
      "High Ground sichern, bevor ihr die Lane pusht.",
    ],
    icon: Swords,
  },
  {
    id: "hq",
    label: "HQ",
    name: "Hauptquartier",
    tagline: "Erobere das HQ. Halte es. Oder sprenge es.",
    players: "6v6",
    teams: "2 Teams",
    respawn: "Halter-Team: deaktiviert // Stürmer-Team: aktiv",
    win: "300 Halte-Punkte oder HQ-Zerstörung",
    duration: "Runden mit Seitenwechsel",
    description:
      "Ein neutrales Hauptquartier erscheint auf der Karte. Das Team, das es einnimmt, sammelt sekündlich Punkte – aber ohne Respawns. Das gegnerische Team stürmt mit voller Mannschaftsstärke. Wer das HQ hält, hält die Runde. Wer stirbt, wartet.",
    twists: [
      "Das HQ ist ein zerstörbares Bauwerk: Stürmer können es mit schweren Waffen sprengen und erzwingen so ein neues HQ an anderem Ort.",
      "Holdout-Spannung pur: Das Halter-Team spielt ab Einnahme ohne Respawns – jeder Tod wiegt doppelt.",
    ],
    tactics: [
      "Halter-Team: verschanzt euch in den oberen Etagen und reißt die Treppen ein.",
      "Stürmer-Team: Tunnel durch die Außenwand sprengen statt Frontalangriff.",
    ],
    icon: Shield,
  },
  {
    id: "ffa",
    label: "FFA",
    name: "Frei für alle",
    tagline: "Jeder gegen jeden. Keine Verbündeten, keine Gnade.",
    players: "8–12",
    teams: "Keine – jeder allein",
    respawn: "Zufällig verteilt",
    win: "20 Kills",
    duration: "1 Runde, ca. 8 Minuten",
    description:
      "Zwölf Kämpfer, eine Ruinenstadt, null Vertrauen. In „Frei für alle“ zählt nur dein eigenes Fadenkreuz. Wer zuerst 20 Kills erreicht, gewinnt – und die letzten Kills sind die schwersten, denn alle jagen den Führenden.",
    twists: [
      "Genetische Codes spawnen als Pickups auf der Karte: temporäre biomechanische Upgrades mitten im Match.",
      "Showdown: Erreichen zwei Spieler gleichzeitig 18+ Kills, startet ein 1v1-Finale in schrumpfender Zone.",
    ],
    tactics: [
      "Duelle Dritter nutzen: Angreifen, wenn zwei andere kämpfen.",
      "Parkour-Routen kennen, um gejagt zu entkommen.",
    ],
    icon: Skull,
  },
  {
    id: "sabotage",
    label: "SAB",
    name: "Sabotage",
    tagline: "Platziere die Ladung. Oder werde selbst zur Falle.",
    players: "5v5",
    teams: "Angreifer vs. Verteidiger",
    respawn: "Deaktiviert (Rundenbasiert)",
    win: "Detonation / Entschärfung / Team-Wipe",
    duration: "2 × 6 Runden, Seitenwechsel zur Halbzeit",
    description:
      "Die Angreifer infiltrieren die Anlage, um an Site A oder B eine Biomass-Ladung zu platzieren. Die Verteidiger halten die Sites – oder entschärfen. Ohne Respawns wird jede Patrone, jede Information und jeder Meter Wand zur Ressource.",
    twists: [
      "Breaching: Angreifer sprengen sich völlig neue Wege zu den Sites – die „sichere“ Defense-Rotation existiert nicht.",
      "Verteidiger bauen aus Trümmern Barrikaden und Fallen: Zerstörung ist hier ein Verteidigungswerkzeug.",
    ],
    tactics: [
      "Fake-Plants: Ladung an Site A antäuschen, dann durch den gesprengten Tunnel zu B rotieren.",
      "Post-Plant-Positionen vor der Detonation einreißen, um das Retaken zu erzwingen.",
    ],
    icon: Bomb,
  },
  {
    id: "ctf",
    label: "CTF",
    name: "Capture the Flag",
    tagline: "Stiehl die Flagge. Überlebe den Heimweg.",
    players: "5v5",
    teams: "2 Teams",
    respawn: "Aktiv (Wellen alle 10 s)",
    win: "3 Flaggen-Eroberungen",
    duration: "2 × 7 Minuten",
    description:
      "Der Klassiker, neu gedacht: Dringe in die gegnerische Basis ein, sichere das Datenbanner und bringe es nach Hause. Doch der Träger ist gebrandmarkt – sichtbar, langsamer im Parkour und das wichtigste Ziel der Karte.",
    twists: [
      "Der Flaggenträger verliert Wandlauf & Sprint (Biomass-Gewicht des Banners) – Eskorte und Blocker werden Pflicht.",
      "Heimrouten können gesprengt oder verbarrikadiert werden: Die Karte erinnert sich an jede Schlacht.",
    ],
    tactics: [
      "Double-Cap: Zwei Läufer, zwei Routen – eine davon durch frischen Wandbruch.",
      "Eigene Flagge nie unbeaufsichtigt lassen: ein Defender bleibt immer zu Hause.",
    ],
    icon: Flag,
  },
  {
    id: "domination",
    label: "DOM",
    name: "Herrschaft",
    tagline: "Drei Zonen. Eine Karte. Totale Kontrolle.",
    players: "6v6",
    teams: "2 Teams",
    respawn: "Aktiv nahe eigener Zonen",
    win: "250 Kontrollpunkte",
    duration: "ca. 12 Minuten",
    description:
      "Drei Kontrollzonen – A, B und C – wollen gehalten werden. Jede gehaltene Zone generiert Punkte; wer alle drei gleichzeitig dominiert, löst den Dominanz-Multiplikator aus und rast dem Sieg entgegen. Ein dauerhafter Stellungskrieg aus Angriff, Ausbau und Verrat.",
    twists: [
      "Zonen-Pylone sind zerstörbar: Eine gesprengte Zone wird neutral und muss neu aufgebaut werden.",
      "Dominanz-Bonus: Alle 3 Zonen = 3× Punkte – aber die Karte markiert das dominante Team für alle sichtbar.",
    ],
    tactics: [
      "2-3-1-Aufteilung: zwei halten A/B, drei roamen, einer überwacht C.",
      "Pylone des Gegners sprengen, statt sie zu stürmen – Zeit ist die Währung.",
    ],
    icon: Crown,
  },
];

/* ------------------------------------------------------------------ */
/* Taktische SVG-Diagramme pro Modus                                   */
/* ------------------------------------------------------------------ */

function ModeDiagram({ mode }: { mode: ModeId }) {
  const stroke = "hsl(130 100% 50%)";
  const strokeDim = "hsl(130 100% 50% / 0.35)";
  const enemy = "hsl(0 70% 55%)";
  const muted = "hsl(80 5% 55%)";
  const fillDim = "hsl(130 100% 50% / 0.06)";

  const label = (x: number, y: number, text: string, color = muted) => (
    <text
      x={x}
      y={y}
      fill={color}
      fontSize="7"
      fontFamily="monospace"
      textAnchor="middle"
      letterSpacing="1"
    >
      {text}
    </text>
  );

  const spawn = (x: number, y: number, color: string, name: string) => (
    <g>
      <rect x={x} y={y} width="34" height="18" fill="none" stroke={color} strokeWidth="1" strokeDasharray="3 2" />
      {label(x + 17, y + 12, name, color)}
    </g>
  );

  return (
    <svg viewBox="0 0 220 120" className="w-full h-auto" role="img" aria-label={`Taktisches Diagramm für ${mode}`}>
      {/* Kartenrahmen */}
      <rect x="4" y="4" width="212" height="112" fill={fillDim} stroke={strokeDim} strokeWidth="1" />

      {mode === "tdm" && (
        <g>
          {spawn(12, 51, stroke, "ALPHA")}
          {spawn(174, 51, enemy, "BRAVO")}
          <rect x="88" y="40" width="44" height="40" fill="none" stroke={muted} strokeWidth="1" />
          {label(110, 62, "ARENA")}
          <path d="M46 60 H84" stroke={stroke} strokeWidth="1" markerEnd="none" strokeDasharray="4 3" />
          <path d="M174 60 H136" stroke={enemy} strokeWidth="1" strokeDasharray="4 3" />
          {/* zerstörbare Wand */}
          <line x1="110" y1="18" x2="110" y2="34" stroke={stroke} strokeWidth="2" />
          {label(110, 14, "BREACH-WAND", stroke)}
          <path d="M104 24 l4 -3 l2 5 l4 -4" stroke={stroke} fill="none" strokeWidth="1" />
        </g>
      )}

      {mode === "hq" && (
        <g>
          {spawn(12, 51, stroke, "ALPHA")}
          {spawn(174, 51, enemy, "BRAVO")}
          <polygon
            points="110,38 128,48 128,68 110,78 92,68 92,48"
            fill="none"
            stroke={stroke}
            strokeWidth="1.5"
          />
          {label(110, 61, "HQ", stroke)}
          <circle cx="110" cy="58" r="26" fill="none" stroke={strokeDim} strokeWidth="1" strokeDasharray="3 3" />
          {label(110, 100, "EINNAHME-RADIUS")}
          <path d="M50 58 H82" stroke={stroke} strokeWidth="1" strokeDasharray="4 3" />
          <path d="M170 58 H138" stroke={enemy} strokeWidth="1" strokeDasharray="4 3" />
        </g>
      )}

      {mode === "ffa" && (
        <g>
          {[
            [30, 30], [110, 22], [190, 32], [26, 92], [110, 98], [192, 90], [66, 60], [156, 60],
          ].map(([x, y], i) => (
            <circle key={i} cx={x} cy={y} r="3.5" fill="none" stroke={i % 2 ? enemy : stroke} strokeWidth="1" />
          ))}
          {[ [70, 34], [150, 86], [110, 60] ].map(([x, y], i) => (
            <g key={i}>
              <path d={`M${x - 4} ${y} h8 M${x} ${y - 4} v8`} stroke={stroke} strokeWidth="1.5" />
            </g>
          ))}
          {label(110, 74, "GEN-CODES", stroke)}
          {label(110, 112, "12 SPAWNS · ZUFALLSVERTEILT")}
        </g>
      )}

      {mode === "sabotage" && (
        <g>
          {spawn(12, 92, enemy, "ANGRIF")}
          {spawn(174, 10, stroke, "VERTEID." )}
          <rect x="52" y="30" width="26" height="22" fill="none" stroke={stroke} strokeWidth="1.5" />
          {label(65, 44, "A", stroke)}
          <rect x="142" y="66" width="26" height="22" fill="none" stroke={stroke} strokeWidth="1.5" />
          {label(155, 80, "B", stroke)}
          {/* Routen */}
          <path d="M46 96 C 60 80, 58 62, 62 54" stroke={enemy} fill="none" strokeWidth="1" strokeDasharray="4 3" />
          <path d="M46 100 C 90 104, 130 96, 140 88" stroke={enemy} fill="none" strokeWidth="1" strokeDasharray="4 3" />
          {/* Breach-Tunnel durch Wand */}
          <line x1="96" y1="70" x2="128" y2="70" stroke={muted} strokeWidth="2" />
          <path d="M104 70 l6 -4 l3 6 l6 -5" stroke={enemy} fill="none" strokeWidth="1.5" />
          {label(112, 84, "TUNNEL-BREACH", enemy)}
        </g>
      )}

      {mode === "ctf" && (
        <g>
          <rect x="12" y="44" width="34" height="32" fill="none" stroke={stroke} strokeWidth="1.5" />
          <path d="M24 52 v14 M24 52 h10 l-3 3 l3 3 h-10" stroke={stroke} fill="none" strokeWidth="1.5" />
          {label(29, 88, "BASIS A", stroke)}
          <rect x="174" y="44" width="34" height="32" fill="none" stroke={enemy} strokeWidth="1.5" />
          <path d="M186 52 v14 M186 52 h10 l-3 3 l3 3 h-10" stroke={enemy} fill="none" strokeWidth="1.5" />
          {label(191, 88, "BASIS B", enemy)}
          <line x1="110" y1="8" x2="110" y2="112" stroke={strokeDim} strokeWidth="1" strokeDasharray="2 4" />
          <path d="M170 60 C 140 30, 90 30, 52 56" stroke={stroke} fill="none" strokeWidth="1" strokeDasharray="4 3" />
          {label(110, 24, "PARKOUR-ROUTE", stroke)}
          {label(110, 104, "TRÄGER: SPRINT GESPERRT", muted)}
        </g>
      )}

      {mode === "domination" && (
        <g>
          {spawn(10, 51, stroke, "ALPHA")}
          {spawn(176, 51, enemy, "BRAVO")}
          {[ [70, 60, "A"], [110, 60, "B"], [150, 60, "C"] ].map(([x, y, t]) => (
            <g key={t as string}>
              <polygon
                points={`${x},${Number(y) - 10} ${Number(x) + 9},${y} ${x},${Number(y) + 10} ${Number(x) - 9},${y}`}
                fill="none"
                stroke={t === "B" ? stroke : muted}
                strokeWidth="1.5"
              />
              {label(Number(x), Number(y) + 3.5, t as string, t === "B" ? stroke : muted)}
            </g>
          ))}
          {label(110, 100, "3 ZONEN · ALLE GEHALTEN = 3× PUNKTE", muted)}
        </g>
      )}
    </svg>
  );
}

/* ------------------------------------------------------------------ */
/* Infrastruktur-Daten                                                 */
/* ------------------------------------------------------------------ */

const INFRASTRUCTURE = [
  {
    icon: Server,
    title: "Dedizierte 128-Tick-Server",
    text: "Autoritative Server-Logik, kein Host-Advantage. Alle sechs Modi laufen auf eigener Hardware in EU-Regionen.",
  },
  {
    icon: Gauge,
    title: "Netcode & Lag-Kompensation",
    text: "Interpolation, Server-Side Rewinding und präzise Treffer-Validierung – auch bei 80 ms Ping fühlt sich jeder Schuss fair an.",
  },
  {
    icon: Trophy,
    title: "Matchmaking & Ranked-Ladder",
    text: "Skill-basiertes Matchmaking mit Placement-Matches, saisonaler Rangliste und Leaderboards pro Modus.",
  },
  {
    icon: ShieldCheck,
    title: "Serverseitiger Anti-Cheat",
    text: "Bewegungs- und Treffer-Plausibilitätsprüfung auf dem Server, kombiniert mit Replay-Review durch das Trust-Team.",
  },
  {
    icon: Clapperboard,
    title: "Replays & Spectator-Modus",
    text: "Jede Match-Replay zum Download, freier Kameraflug und Turnier-Spectator für eSport-Übertragungen.",
  },
  {
    icon: Users,
    title: "Custom-Lobbys & Community",
    text: "Private Lobbys mit Regel-Presets, Community-Server-Support und Mod-Schnittstelle für eigene Kartenrotationen.",
  },
];

/* ------------------------------------------------------------------ */
/* Modus-Matrix                                                        */
/* ------------------------------------------------------------------ */

const MATRIX: { mode: string; values: [string, string, string, string, string] }[] = [
  { mode: "Team-Deathmatch", values: ["5v5", "Kills", "Sofort", "Nein", "Schnell"] },
  { mode: "Hauptquartier", values: ["6v6", "Halte-Punkte", "Nur Stürmer", "Ja", "Mittel"] },
  { mode: "Frei für alle", values: ["8–12 Solo", "Kills", "Zufall", "Nein", "Schnell"] },
  { mode: "Sabotage", values: ["5v5", "Rundensiege", "Nein", "Ja", "Taktisch"] },
  { mode: "Capture the Flag", values: ["5v5", "Flaggen", "Wellen", "Nein", "Dynamisch"] },
  { mode: "Herrschaft", values: ["6v6", "Zonen-Punkte", "Zonen-nah", "Nein", "Strategisch"] },
];

/* ------------------------------------------------------------------ */
/* Sektion                                                             */
/* ------------------------------------------------------------------ */

export function MultiplayerSection() {
  const [activeMode, setActiveMode] = useState<ModeId>("tdm");
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

  const mode = MODES.find((m) => m.id === activeMode) ?? MODES[0];
  const ActiveIcon = mode.icon;

  return (
    <section
      ref={sectionRef}
      id="multiplayer"
      className="relative py-20 md:py-32 px-6 scanlines"
    >
      <div className="max-w-6xl mx-auto">
        {/* Section Label */}
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            05 // Multiplayer
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-4 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Sechs Modi,
          <span className="text-primary glow-neon-sm"> ein Krieg</span>
        </h2>
        <p
          className={`text-base md:text-lg text-muted-foreground max-w-3xl mb-12 leading-relaxed transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Jeder Modus nutzt die Kernmechaniken von WIRRWARR – Zerstörung,
          Parkour und Taktik – auf seine eigene Art. Wähle einen Modus für das
          vollständige Design-Dokument.
        </p>

        {/* Key Art */}
        <div
          className={`relative aspect-[21/9] rounded-sm overflow-hidden border border-border border-glow mb-10 transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <Image
            src="/images/multiplayer-keyart.jpg"
            alt="Zwei Squads stehen sich in einer von Biomass überwucherten Ruinenstadt gegenüber"
            fill
            className="object-cover"
          />
          <div className="absolute inset-0 bg-gradient-to-t from-background/80 via-transparent to-transparent" />
          <span className="absolute bottom-3 left-4 font-mono text-[10px] tracking-[0.25em] uppercase text-primary glow-neon-sm">
            Konzept-Key-Art // Squad-Krieg
          </span>
        </div>

        {/* Mode Selector */}
        <div
          className={`grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-2 mb-10 transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {MODES.map((m) => {
            const Icon = m.icon;
            const active = m.id === activeMode;
            return (
              <button
                key={m.id}
                type="button"
                onClick={() => setActiveMode(m.id)}
                aria-pressed={active}
                className={`flex flex-col items-center gap-2 px-3 py-4 rounded-sm border transition-all min-h-[44px] ${
                  active
                    ? "bg-primary text-primary-foreground border-primary box-glow-neon"
                    : "bg-secondary/60 text-secondary-foreground border-border hover:border-primary/40 hover:text-primary"
                }`}
              >
                <Icon className="w-5 h-5" strokeWidth={1.75} />
                <span className="font-mono text-[11px] tracking-wider uppercase">
                  {m.label}
                </span>
              </button>
            );
          })}
        </div>

        {/* Active Mode Detail */}
        <div
          key={mode.id}
          className="animate-fade-in border border-border bg-card rounded-sm box-glow-neon overflow-hidden"
        >
          {/* Header */}
          <div className="flex flex-wrap items-center gap-4 px-6 md:px-8 py-5 border-b border-border bg-secondary/30">
            <span className="w-11 h-11 rounded-sm border border-primary/40 bg-primary/10 flex items-center justify-center">
              <ActiveIcon className="w-5 h-5 text-primary" strokeWidth={1.75} />
            </span>
            <div className="flex-1 min-w-[200px]">
              <h3 className="text-xl md:text-2xl font-bold text-foreground leading-none">
                {mode.name}
              </h3>
              <p className="font-mono text-xs text-primary mt-1.5 tracking-wider">
                {mode.tagline}
              </p>
            </div>
            <span className="font-mono text-[10px] tracking-[0.25em] uppercase text-muted-foreground border border-border rounded-sm px-3 py-1.5">
              GDD-Eintrag // MP-{mode.label}
            </span>
          </div>

          <div className="grid lg:grid-cols-2 gap-0">
            {/* Left: Stats + Diagram */}
            <div className="p-6 md:p-8 border-b lg:border-b-0 lg:border-r border-border">
              {/* Stats Grid */}
              <div className="grid grid-cols-2 gap-px bg-border border border-border rounded-sm overflow-hidden mb-6">
                {[
                  ["Spieler", mode.players],
                  ["Teams", mode.teams],
                  ["Respawn", mode.respawn],
                  ["Sieg durch", mode.win],
                  ["Dauer", mode.duration],
                  ["Status", "In Entwicklung"],
                ].map(([k, v]) => (
                  <div key={k} className="bg-card px-4 py-3">
                    <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-1">
                      {k}
                    </p>
                    <p className="text-sm text-foreground font-medium leading-snug">
                      {v}
                    </p>
                  </div>
                ))}
              </div>

              {/* Tactical Diagram */}
              <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-3">
                Taktische Übersicht
              </p>
              <div className="border border-border rounded-sm bg-background/60 p-3">
                <ModeDiagram mode={mode.id} />
              </div>
            </div>

            {/* Right: Description + Twists + Tactics */}
            <div className="p-6 md:p-8 flex flex-col gap-6">
              <p className="text-base text-secondary-foreground leading-relaxed">
                {mode.description}
              </p>

              <div>
                <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-primary mb-3 glow-neon-sm">
                  WIRRWARR-Twist
                </p>
                <ul className="space-y-3">
                  {mode.twists.map((t, i) => (
                    <li key={i} className="flex items-start gap-3">
                      <span className="mt-1.5 w-1.5 h-1.5 rounded-full bg-primary shrink-0 animate-pulse-neon" />
                      <span className="text-sm text-foreground leading-relaxed">{t}</span>
                    </li>
                  ))}
                </ul>
              </div>

              <div>
                <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-3">
                  Taktik-Notizen
                </p>
                <ul className="space-y-3">
                  {mode.tactics.map((t, i) => (
                    <li key={i} className="flex items-start gap-3">
                      <span className="mt-1.5 w-1.5 h-1.5 rounded-full bg-accent shrink-0" />
                      <span className="text-sm text-secondary-foreground leading-relaxed">{t}</span>
                    </li>
                  ))}
                </ul>
              </div>
            </div>
          </div>
        </div>

        {/* Mode Matrix */}
        <div
          className={`mt-16 transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-6">
            Modus-Matrix
          </p>
          <div className="overflow-x-auto border border-border rounded-sm">
            <table className="w-full text-left border-collapse min-w-[640px]">
              <thead>
                <tr className="bg-secondary/40">
                  {["Modus", "Format", "Wertung", "Respawn", "Rundenbasiert", "Tempo"].map(
                    (h) => (
                      <th
                        key={h}
                        className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground px-4 py-3 border-b border-border"
                      >
                        {h}
                      </th>
                    )
                  )}
                </tr>
              </thead>
              <tbody>
                {MATRIX.map((row, i) => (
                  <tr
                    key={row.mode}
                    className={`${i % 2 ? "bg-secondary/20" : ""} hover:bg-primary/5 transition-colors`}
                  >
                    <td className="px-4 py-3 text-sm font-medium text-foreground border-b border-border/60">
                      {row.mode}
                    </td>
                    {row.values.map((v, j) => (
                      <td
                        key={j}
                        className="px-4 py-3 text-sm text-muted-foreground border-b border-border/60 font-mono text-xs"
                      >
                        {v}
                      </td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        {/* Infrastructure */}
        <div
          className={`mt-16 transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-2">
            Infrastruktur
          </p>
          <h3 className="text-2xl md:text-3xl font-bold text-foreground mb-8">
            Gebaut für <span className="text-primary glow-neon-sm">fairen Krieg</span>
          </h3>
          <div className="grid sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {INFRASTRUCTURE.map((item) => {
              const Icon = item.icon;
              return (
                <div
                  key={item.title}
                  className="group border border-border bg-card rounded-sm p-5 hover:border-primary/40 hover:box-glow-neon transition-all"
                >
                  <div className="flex items-center gap-3 mb-3">
                    <span className="w-9 h-9 rounded-sm border border-primary/30 bg-primary/10 flex items-center justify-center group-hover:bg-primary/20 transition-colors">
                      <Icon className="w-4 h-4 text-primary" strokeWidth={1.75} />
                    </span>
                    <h4 className="text-sm font-bold text-foreground leading-tight">
                      {item.title}
                    </h4>
                  </div>
                  <p className="text-sm text-muted-foreground leading-relaxed">
                    {item.text}
                  </p>
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </section>
  );
}
