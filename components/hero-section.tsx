"use client";

import { useEffect, useRef, useState } from "react";
import Image from "next/image";

export function HeroSection() {
  const [isVisible, setIsVisible] = useState(false);
  const [scrollY, setScrollY] = useState(0);
  const sectionRef = useRef<HTMLElement>(null);

  useEffect(() => {
    setIsVisible(true);
    const handleScroll = () => setScrollY(window.scrollY);
    window.addEventListener("scroll", handleScroll, { passive: true });
    return () => window.removeEventListener("scroll", handleScroll);
  }, []);

  return (
    <section
      ref={sectionRef}
      className="relative min-h-screen flex items-center justify-center overflow-hidden"
    >
      {/* Background Image with Parallax */}
      <div
        className="absolute inset-0"
        style={{ transform: `translateY(${scrollY * 0.3}px)` }}
      >
        <Image
          src="/images/hero-biomass.jpg"
          alt="Post-apokalyptische Welt, überwuchert von außerirdischer Biomasse"
          fill
          className="object-cover"
          priority
        />
        <div className="absolute inset-0 bg-background/70" />
        <div className="absolute inset-0 bg-gradient-to-b from-background/40 via-transparent to-background" />
      </div>

      {/* Scanline overlay */}
      <div className="absolute inset-0 scanlines pointer-events-none" />

      {/* Content */}
      <div className="relative z-10 px-6 text-center max-w-5xl mx-auto">
        {/* Subtitle */}
        <p
          className={`font-mono text-sm md:text-base tracking-[0.3em] uppercase text-primary mb-6 glow-neon-sm transition-all duration-1000 ${
            isVisible
              ? "opacity-100 translate-y-0"
              : "opacity-0 translate-y-4"
          }`}
        >
          Game Design Document // Konzept v1.0
        </p>

        {/* Main Title */}
        <h1
          className={`text-6xl sm:text-7xl md:text-8xl lg:text-9xl font-bold tracking-tight text-foreground leading-none mb-4 transition-all duration-1000 delay-200 ${
            isVisible
              ? "opacity-100 translate-y-0"
              : "opacity-0 translate-y-8"
          }`}
        >
          <span className="text-primary glow-neon">SAVE</span>
          <span className="text-foreground"> THE WORLD</span>
        </h1>
        <p className="font-mono text-sm md:text-lg tracking-[0.25em] uppercase text-muted-foreground -mt-2 mb-4">
          Rette die Welt auf deine Art
        </p>

        {/* Tagline */}
        <p
          className={`text-lg sm:text-xl md:text-2xl font-light text-muted-foreground max-w-2xl mx-auto mb-10 leading-relaxed transition-all duration-1000 delay-500 ${
            isVisible
              ? "opacity-100 translate-y-0"
              : "opacity-0 translate-y-6"
          }`}
        >
          Third-Person-Shooter trifft taktische Echtzeit-Strategie.
          <br className="hidden sm:block" />
          <span className="text-foreground font-medium">
            Zerst&ouml;re alles. Befehlige dein Team. &Uuml;berlebe.
          </span>
        </p>

        {/* USP Badge */}
        <div
          className={`inline-flex items-center gap-3 border border-primary/30 bg-card/50 backdrop-blur-sm px-6 py-3 rounded-sm box-glow-neon transition-all duration-1000 delay-700 ${
            isVisible
              ? "opacity-100 translate-y-0"
              : "opacity-0 translate-y-6"
          }`}
        >
          <span className="w-2 h-2 rounded-full bg-primary animate-pulse-neon" />
          <span className="font-mono text-xs sm:text-sm tracking-wider uppercase text-primary">
            Dynamic Destructibility
          </span>
        </div>

        {/* Play CTA */}
        <div
          className={`mt-8 transition-all duration-1000 delay-700 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-6"
          }`}
        >
          <a
            href="/play"
            className="inline-flex items-center gap-3 bg-primary text-primary-foreground font-mono text-sm tracking-[0.2em] uppercase px-8 py-4 rounded-sm box-glow-neon hover:opacity-90 transition-opacity min-h-[44px]"
          >
            ▶ Prototyp spielen
          </a>
          <a
            href="/game"
            className="inline-flex items-center gap-3 border border-primary/60 text-primary font-mono text-sm tracking-[0.2em] uppercase px-8 py-4 rounded-sm hover:bg-primary/10 transition-colors min-h-[44px] ml-3"
          >
            🎮 Echtes 3D-Game
          </a>
          <p className="font-mono text-[10px] text-muted-foreground tracking-wider uppercase mt-3">
            Web-Demo: 6 Modi vs. Bots · 3D: Vertical Slice mit Destruction
          </p>
        </div>

        {/* Scroll indicator */}
        <div
          className={`mt-16 flex flex-col items-center gap-2 transition-all duration-1000 delay-1000 ${
            isVisible ? "opacity-100" : "opacity-0"
          }`}
        >
          <span className="font-mono text-xs text-muted-foreground tracking-widest uppercase">
            Scroll
          </span>
          <div className="w-px h-12 bg-gradient-to-b from-primary/60 to-transparent animate-pulse-neon" />
        </div>
      </div>
    </section>
  );
}
