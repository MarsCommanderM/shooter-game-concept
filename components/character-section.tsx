"use client";

import { useEffect, useRef, useState } from "react";
import Image from "next/image";

export function CharacterSection() {
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

  return (
    <section
      ref={sectionRef}
      id="character"
      className="relative py-20 md:py-32 px-6"
    >
      <div className="absolute inset-0 bg-[hsl(var(--surface))]" />

      <div className="relative max-w-6xl mx-auto">
        {/* Section Label */}
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            04 // Charakter & Erz&auml;hlung
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <div className="flex flex-col lg:flex-row-reverse gap-12 lg:gap-16 items-center">
          {/* Character Portrait */}
          <div
            className={`w-full lg:w-5/12 transition-all duration-700 delay-200 ${
              isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-12"
            }`}
          >
            <div className="relative aspect-[3/4] rounded-sm overflow-hidden border-glow">
              <Image
                src="/images/protagonist.jpg"
                alt="Die Protagonistin \u2013 eine ehemalige Wissenschaftlerin, teilweise mit dem Wirrwarr verschmolzen"
                fill
                className="object-cover"
              />
              <div className="absolute inset-0 bg-gradient-to-t from-background/80 via-transparent to-background/20" />

              {/* Character info overlay */}
              <div className="absolute bottom-0 left-0 right-0 p-6">
                <p className="font-mono text-xs text-primary tracking-wider uppercase mb-2">
                  Protagonistin
                </p>
                <h3 className="text-2xl font-bold text-foreground mb-1">
                  Dr. Elena Vasquez
                </h3>
                <p className="text-sm text-muted-foreground">
                  Ehemalige Xenobiologin // Biomasse-Hybrid
                </p>
              </div>
            </div>
          </div>

          {/* Text Content */}
          <div
            className={`w-full lg:w-7/12 transition-all duration-700 delay-300 ${
              isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-12"
            }`}
          >
            <h2 className="text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-6 leading-tight text-balance">
              Zwischen Mensch und
              <span className="text-primary glow-neon-sm"> Biomasse</span>
            </h2>

            <div className="space-y-6 mb-10">
              <p className="text-base md:text-lg text-muted-foreground leading-relaxed">
                Dr. Elena Vasquez war eine der f&uuml;hrenden Xenobiologinnen, als der Wirrwarr
                die Erde &uuml;berflutete. Bei dem Versuch, ein Heilmittel zu finden,
                verschmolz sie teilweise mit der au&szlig;erirdischen Biomasse.
              </p>
              <p className="text-base md:text-lg text-muted-foreground leading-relaxed">
                Jetzt k&auml;mpft sie um ihre Menschlichkeit &ndash; w&auml;hrend die Fähigkeiten,
                die der Wirrwarr ihr verleiht, sie immer weiter von dem entfernen,
                was sie einmal war.
              </p>
            </div>

            {/* Narrative Structure */}
            <div className="p-6 bg-card border border-border rounded-sm">
              <p className="font-mono text-xs text-primary tracking-wider uppercase mb-4">
                Erz&auml;hlstruktur
              </p>
              <div className="space-y-4">
                {[
                  {
                    label: "Nicht-linear",
                    text: "Freie Reihenfolge bei der Erkundung der Mini-Open-World",
                  },
                  {
                    label: "Au&szlig;enposten",
                    text: "Befreie Siedlungen und entscheide \u00fcber ihr Schicksal",
                  },
                  {
                    label: "Menschlichkeit",
                    text: "Jedes Biomasse-Upgrade ver\u00e4ndert Elenas Wahrnehmung der Welt",
                  },
                ].map((item, i) => (
                  <div key={i} className="flex items-start gap-4">
                    <span
                      className="font-mono text-xs text-primary tracking-wider uppercase whitespace-nowrap mt-0.5 min-w-[100px]"
                      dangerouslySetInnerHTML={{ __html: item.label }}
                    />
                    <span className="text-sm text-muted-foreground">
                      {item.text}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
