"use client";

import { useEffect, useRef, useState } from "react";

const facts = [
  { label: "Genre", value: "Third-Person-Shooter" },
  { label: "Subgenre", value: "Taktik / RTS-Hybrid" },
  { label: "Perspektive", value: "Third-Person" },
  { label: "Struktur", value: "Mini-Open-World" },
  { label: "Modus", value: "Singleplayer + Squad-KI" },
  { label: "USP", value: "Dynamic Destructibility" },
];

export function FactsBar() {
  const [isVisible, setIsVisible] = useState(false);
  const sectionRef = useRef<HTMLElement>(null);

  useEffect(() => {
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) setIsVisible(true);
      },
      { threshold: 0.2 }
    );
    if (sectionRef.current) observer.observe(sectionRef.current);
    return () => observer.disconnect();
  }, []);

  return (
    <section ref={sectionRef} className="relative px-6 py-12 border-y border-border bg-[hsl(var(--surface))]">
      <div className="max-w-6xl mx-auto">
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-6">
          {facts.map((fact, i) => (
            <div
              key={fact.label}
              className={`flex flex-col gap-1 transition-all duration-500 ${
                isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-6"
              }`}
              style={{ transitionDelay: `${i * 80}ms` }}
            >
              <span className="font-mono text-[10px] text-muted-foreground tracking-[0.2em] uppercase">
                {fact.label}
              </span>
              <span className="text-sm md:text-base font-semibold text-foreground leading-tight text-balance">
                {fact.value}
              </span>
              <span className="mt-1 w-6 h-px bg-primary/50" />
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}
