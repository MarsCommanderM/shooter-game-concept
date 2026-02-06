"use client";

import { useEffect, useRef, useState } from "react";
import Image from "next/image";

const mechanics = [
  {
    id: "movement",
    label: "Bewegung",
    title: "Fl\u00fcssiges Parkour-System",
    description:
      "Klettern, Springen, Rutschen \u2013 die gesamte Umgebung ist dein Spielplatz. Keine automatische Deckungsnahme: Du entscheidest, wann und wo du in Deckung gehst.",
    image: "/images/parkour.jpg",
    imageAlt: "Parkour-Bewegungssystem mit fl\u00fcssigen \u00dcberg\u00e4ngen",
    details: [
      "Klettern an jeder Oberfl\u00e4che",
      'Keine "Magnetic Cover" \u2013 freie Deckungswahl',
      "Rutschen, Springen, Wandlauf",
      "Fl\u00fcssige \u00dcberg\u00e4nge zwischen Aktionen",
    ],
  },
  {
    id: "combat",
    label: "Kampf",
    title: "Taktischer Kampf mit Befehlen",
    description:
      "Unterschiedliche Waffen f\u00fcr organische und mechanische Ziele. Der Taktik-Modus pausiert das Spiel und erlaubt Wegpunkte und Befehle f\u00fcr deine zwei KI-Kameraden.",
    image: "/images/tactics.jpg",
    imageAlt: "Taktik-Modus mit Squad-Befehlen und Wegpunkten",
    details: [
      "Waffenklassen: Organisch vs. Mechanisch",
      "Taktik-Modus pausiert das Spiel",
      "Wegpunkte f\u00fcr 2 KI-Kameraden",
      "Echtzeit-Befehlssystem",
    ],
  },
  {
    id: "destruction",
    label: "Destruktion",
    title: "Permanente Zerst\u00f6rung",
    description:
      "Waffen und Umgebungsexplosionen hinterlassen permanente Spuren. Ein gro\u00dfkalibriges Gesch\u00fctz kann einen Tunnel durch eine Geb\u00e4udewand sprengen.",
    image: "/images/destruction.jpg",
    imageAlt: "Zerst\u00f6rbare Umgebung mit permanenten Sch\u00e4den",
    details: [
      "Vollst\u00e4ndig zerst\u00f6rbare Strukturen",
      "Permanente Schadensberechnung",
      "Neue Wege durch Zerst\u00f6rung",
      "Taktische Fallenstellung",
    ],
  },
  {
    id: "progression",
    label: "Progression",
    title: "Biomechanische Upgrades",
    description:
      'Finde "Genetische Codes" der Biomasse f\u00fcr permanente, groteske Upgrades. Ein visueller Sinn f\u00fcr verborgene Feinde \u2013 der dein Sichtfeld verzerrt.',
    image: "/images/biomass-upgrades.jpg",
    imageAlt: "Biomechanische Upgrades durch genetische Codes",
    details: [
      "Genetische Codes sammeln",
      "Groteske biomechanische Upgrades",
      "Verzerrtes Sichtfeld als F\u00e4higkeit",
      "Permanente K\u00f6rpermodifikationen",
    ],
  },
];

export function MechanicsSection() {
  const [activeTab, setActiveTab] = useState(0);
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

  const currentMechanic = mechanics[activeTab];

  return (
    <section
      ref={sectionRef}
      id="mechanics"
      className="relative py-20 md:py-32 px-6"
    >
      <div className="max-w-6xl mx-auto">
        {/* Section Label */}
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            03 // Core Gameplay Loop
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-16 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Vier Mechaniken,
          <span className="text-primary glow-neon-sm"> ein Kreislauf</span>
        </h2>

        {/* Tab Navigation */}
        <div
          className={`flex flex-wrap gap-2 mb-10 transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          {mechanics.map((m, index) => (
            <button
              key={m.id}
              type="button"
              onClick={() => setActiveTab(index)}
              className={`flex items-center gap-2 px-4 py-3 font-mono text-sm tracking-wider uppercase rounded-sm transition-all min-h-[44px] ${
                activeTab === index
                  ? "bg-primary text-primary-foreground box-glow-neon"
                  : "bg-secondary text-secondary-foreground hover:bg-secondary/80 border border-border"
              }`}
            >
              <span className="opacity-50 text-xs">0{index + 1}</span>
              {m.label}
            </button>
          ))}
        </div>

        {/* Active Mechanic Content */}
        <div
          key={activeTab}
          className="flex flex-col lg:flex-row gap-10 lg:gap-16 animate-fade-in"
        >
          {/* Image */}
          <div className="w-full lg:w-1/2">
            <div className="relative aspect-video rounded-sm overflow-hidden border-glow">
              <Image
                src={currentMechanic.image || "/placeholder.svg"}
                alt={currentMechanic.imageAlt}
                fill
                className="object-cover"
              />
              <div className="absolute inset-0 bg-gradient-to-t from-background/60 via-transparent to-transparent" />
              {/* Label overlay */}
              <div className="absolute top-4 left-4 flex items-center gap-2">
                <span className="w-2 h-2 rounded-full bg-primary animate-pulse-neon" />
                <span className="font-mono text-xs text-primary tracking-wider uppercase">
                  {currentMechanic.label}
                </span>
              </div>
            </div>
          </div>

          {/* Details */}
          <div className="w-full lg:w-1/2 flex flex-col justify-center">
            <h3 className="text-2xl md:text-3xl font-bold text-foreground mb-4">
              {currentMechanic.title}
            </h3>
            <p className="text-base text-muted-foreground leading-relaxed mb-8">
              {currentMechanic.description}
            </p>

            {/* Detail List */}
            <ul className="space-y-3">
              {currentMechanic.details.map((detail, i) => (
                <li key={i} className="flex items-start gap-3">
                  <span className="mt-1.5 w-1.5 h-1.5 rounded-full bg-primary shrink-0" />
                  <span className="text-sm text-secondary-foreground">
                    {detail}
                  </span>
                </li>
              ))}
            </ul>
          </div>
        </div>

        {/* Gameplay Loop Diagram */}
        <div
          className={`mt-20 p-6 md:p-8 bg-card border border-border rounded-sm transition-all duration-700 delay-500 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <p className="font-mono text-xs text-muted-foreground tracking-wider uppercase mb-6">
            Core Gameplay Loop
          </p>
          <div className="flex flex-col sm:flex-row items-center justify-center gap-4 sm:gap-0">
            {["Erkunden", "K\u00e4mpfen", "Zerst\u00f6ren", "Upgraden"].map(
              (step, i) => (
                <div key={i} className="flex items-center gap-4">
                  <div className="flex flex-col items-center gap-2">
                    <div className="w-16 h-16 md:w-20 md:h-20 rounded-full border border-primary/40 flex items-center justify-center bg-secondary/30">
                      <span className="font-mono text-lg md:text-xl font-bold text-primary">
                        {String(i + 1).padStart(2, "0")}
                      </span>
                    </div>
                    <span className="font-mono text-xs text-foreground tracking-wider">
                      {step}
                    </span>
                  </div>
                  {i < 3 && (
                    <svg
                      className="w-6 h-6 text-primary/40 hidden sm:block"
                      viewBox="0 0 24 24"
                      fill="none"
                      stroke="currentColor"
                      strokeWidth="1.5"
                    >
                      <path d="M5 12h14M12 5l7 7-7 7" />
                    </svg>
                  )}
                </div>
              )
            )}
          </div>
        </div>
      </div>
    </section>
  );
}
