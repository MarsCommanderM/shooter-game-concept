"use client";

import { useEffect, useRef, useState } from "react";

export function SettingSection() {
  const [isVisible, setIsVisible] = useState(false);
  const sectionRef = useRef<HTMLElement>(null);

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) setIsVisible(true);
      },
      { threshold: 0.15 }
    );
    if (sectionRef.current) observer.observe(sectionRef.current);
    return () => observer.disconnect();
  }, []);

  const atmosphereFeatures = [
    {
      title: "Der Wirrwarr",
      description:
        "Eine außerirdische Biomasse hat die Erde überwuchert. Organische Strukturen wachsen über zerfallene Städte und verschmelzen Natur mit dem Fremden.",
      icon: (
        <svg
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.5"
          className="w-6 h-6"
        >
          <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2z" />
          <path d="M8 12c0-2 1-4 4-4s4 2 4 4-1 4-4 4-4-2-4-4z" />
          <path d="M12 2v4M12 18v4M2 12h4M18 12h4" />
        </svg>
      ),
    },
    {
      title: "Visueller Stil",
      description:
        "Üppige, überwucherte Landschaften treffen auf fremdartige, organische Architektur. Gedämpfte Naturtöne durchsetzt mit grellen, giftigen Neon-Akzenten.",
      icon: (
        <svg
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.5"
          className="w-6 h-6"
        >
          <path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7z" />
          <circle cx="12" cy="12" r="3" />
        </svg>
      ),
    },
    {
      title: "Mini-Open-World",
      description:
        "Erkunde einen großen, zusammenhängenden Kartenausschnitt. Entscheide selbst, in welcher Reihenfolge du Außenposten befreist oder Bedrohungen beseitigst.",
      icon: (
        <svg
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="1.5"
          className="w-6 h-6"
        >
          <path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z" />
          <circle cx="12" cy="10" r="3" />
        </svg>
      ),
    },
  ];

  return (
    <section
      ref={sectionRef}
      id="setting"
      className="relative py-20 md:py-32 px-6"
    >
      {/* Background accent */}
      <div className="absolute inset-0 bg-[hsl(var(--surface))]" />

      <div className="relative max-w-6xl mx-auto">
        {/* Section Label */}
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            02 // Setting & Atmosph&auml;re
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-4 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Eine Welt zwischen
          <span className="text-primary glow-neon-sm"> Sch&ouml;nheit und Schrecken</span>
        </h2>

        <p
          className={`text-base md:text-lg text-muted-foreground max-w-2xl mb-16 leading-relaxed transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Post-apokalyptische Sci-Fi. Die Inspiration reicht von den überwucherten
          Landschaften aus &bdquo;The Last of Us&ldquo; bis zur fremdartigen Biologie
          von &bdquo;Annihilation&ldquo;.
        </p>

        {/* Feature Cards */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
          {atmosphereFeatures.map((feature, index) => (
            <div
              key={index}
              className={`group relative p-6 md:p-8 bg-card border border-border rounded-sm hover:border-primary/30 transition-all duration-500 ${
                isVisible
                  ? "opacity-100 translate-y-0"
                  : "opacity-0 translate-y-12"
              }`}
              style={{ transitionDelay: `${300 + index * 150}ms` }}
            >
              <div className="text-primary mb-4 group-hover:glow-neon-sm transition-all">
                {feature.icon}
              </div>
              <h3 className="text-xl font-semibold text-foreground mb-3">
                {feature.title}
              </h3>
              <p className="text-sm text-muted-foreground leading-relaxed">
                {feature.description}
              </p>
              {/* Corner accent */}
              <div className="absolute top-0 right-0 w-8 h-8 border-t border-r border-primary/20 rounded-tr-sm opacity-0 group-hover:opacity-100 transition-opacity" />
            </div>
          ))}
        </div>

        {/* Color Palette Display */}
        <div
          className={`mt-16 p-6 bg-card border border-border rounded-sm transition-all duration-700 delay-700 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <p className="font-mono text-xs text-muted-foreground tracking-wider uppercase mb-4">
            Farbpalette
          </p>
          <div className="flex gap-3 flex-wrap">
            {[
              { color: "bg-[#2a2f28]", label: "Moos-Dunkel" },
              { color: "bg-[#4a4a3a]", label: "Erde" },
              { color: "bg-[#7a8a6a]", label: "Verwittert" },
              {
                color: "bg-[hsl(130,100%,50%)]",
                label: "Biomasse-Neon",
              },
              { color: "bg-[hsl(160,100%,40%)]", label: "Toxisch-Cyan" },
            ].map((swatch, i) => (
              <div key={i} className="flex flex-col items-center gap-2">
                <div
                  className={`w-12 h-12 md:w-16 md:h-16 rounded-sm ${swatch.color} border border-border`}
                />
                <span className="font-mono text-[10px] text-muted-foreground">
                  {swatch.label}
                </span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
