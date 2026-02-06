"use client";

import { useEffect, useRef, useState } from "react";
import Image from "next/image";

export function USPSection() {
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
      id="usp"
      className="relative py-20 md:py-32 px-6 overflow-hidden"
    >
      <div className="max-w-6xl mx-auto">
        {/* Section Label */}
        <div
          className={`flex items-center gap-4 mb-12 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            01 // Unique Selling Point
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <div className="flex flex-col lg:flex-row gap-12 lg:gap-16 items-center">
          {/* Image */}
          <div
            className={`w-full lg:w-1/2 transition-all duration-700 delay-200 ${
              isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-12"
            }`}
          >
            <div className="relative aspect-video rounded-sm overflow-hidden border-glow">
              <Image
                src="/images/destruction.jpg"
                alt="Dynamic Destructibility - Zerst&ouml;rbare Umgebungen schaffen neue taktische M&ouml;glichkeiten"
                fill
                className="object-cover"
              />
              <div className="absolute inset-0 bg-gradient-to-t from-background/80 via-transparent to-transparent" />
              <div className="absolute bottom-4 left-4 right-4">
                <span className="font-mono text-xs text-primary tracking-wider">
                  DYNAMIC DESTRUCTIBILITY
                </span>
              </div>
            </div>
          </div>

          {/* Text Content */}
          <div
            className={`w-full lg:w-1/2 transition-all duration-700 delay-400 ${
              isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-12"
            }`}
          >
            <h2 className="text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-6 leading-tight text-balance">
              Jede Wand ist ein
              <span className="text-primary glow-neon-sm"> taktischer Vorteil</span>
            </h2>
            <p className="text-base md:text-lg text-muted-foreground leading-relaxed mb-8">
              Fast jede Struktur im Spiel kann zerst&ouml;rt werden &ndash; W&auml;nde, Decken,
              S&auml;ulen. Sprenge neue Schusslinien, schaffe Fluchtwege oder
              konstruiere t&ouml;dliche Fallen. Die Zerst&ouml;rung ist permanent und
              ver&auml;ndert das Schlachtfeld mit jedem Schuss.
            </p>

            {/* Feature Pills */}
            <div className="flex flex-wrap gap-3">
              {[
                "Neue Schusslinien",
                "Fluchtwege schaffen",
                "Fallen konstruieren",
                "Permanente Zerst&ouml;rung",
              ].map((feature, i) => (
                <span
                  key={i}
                  className="px-4 py-2 text-sm font-mono tracking-wider border border-border bg-secondary/50 text-secondary-foreground rounded-sm"
                  dangerouslySetInnerHTML={{ __html: feature }}
                />
              ))}
            </div>
          </div>
        </div>
      </div>
    </section>
  );
}
