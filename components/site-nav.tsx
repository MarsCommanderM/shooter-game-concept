"use client";

import { useEffect, useState } from "react";
import { STWLogo } from "./stw-logo";

const navLinks = [
  { label: "USP", href: "#usp" },
  { label: "Setting", href: "#setting" },
  { label: "Mechanik", href: "#mechanics" },
  { label: "Charakter", href: "#character" },
  { label: "Multiplayer", href: "#multiplayer" },
  { label: "Arsenal", href: "#arsenal" },
  { label: "Evo", href: "#evolution" },
  { label: "Karten", href: "#maps" },
  { label: "HUD", href: "#hud" },
  { label: "Sound", href: "#sound" },
  { label: "Live-Ops", href: "#liveops" },
  { label: "Roadmap", href: "#roadmap" },
];

export function SiteNav() {
  const [scrolled, setScrolled] = useState(false);
  const [menuOpen, setMenuOpen] = useState(false);

  useEffect(() => {
    const handleScroll = () => setScrolled(window.scrollY > 50);
    window.addEventListener("scroll", handleScroll, { passive: true });
    return () => window.removeEventListener("scroll", handleScroll);
  }, []);

  return (
    <nav
      className={`fixed top-0 left-0 right-0 z-50 transition-all duration-300 ${
        scrolled
          ? "bg-background/90 backdrop-blur-md border-b border-border"
          : "bg-transparent"
      }`}
    >
      <div className="max-w-6xl mx-auto px-6 flex items-center justify-between h-16">
        {/* Logo */}
        <a href="#" className="flex items-center gap-2">
          <STWLogo className="w-7 h-7" />
          <span className="font-bold text-lg tracking-tight text-foreground">
            <span className="text-primary glow-neon-sm">SAVE</span> THE WORLD
          </span>
          <span className="font-mono text-[10px] text-muted-foreground tracking-wider hidden sm:block">
            GDD
          </span>
        </a>

        {/* Desktop Nav */}
        <div className="hidden lg:flex items-center gap-4">
          <a
            href="/nova/"
            className="font-mono text-[11px] tracking-wider uppercase text-primary border border-primary/50 bg-primary/10 rounded-sm px-2.5 py-1.5 hover:bg-primary/20 transition-colors"
          >
            🎮 Web-Client
          </a>
          {navLinks.map((link) => (
            <a
              key={link.href}
              href={link.href}
              className="font-mono text-[11px] tracking-wider uppercase text-muted-foreground hover:text-primary transition-colors min-h-[44px] flex items-center"
            >
              {link.label}
            </a>
          ))}
        </div>

        {/* Demo CTA (desktop) */}
        <a
          href="/play"
          className="hidden lg:inline-flex items-center gap-2 font-mono text-[11px] tracking-wider uppercase text-muted-foreground border border-border rounded-sm px-3 py-2 hover:text-primary hover:border-primary/40 transition-colors min-h-[36px]"
        >
          ▶ Demo
        </a>
        <a
          href="/game"
          className="hidden lg:inline-flex items-center gap-2 font-mono text-[11px] tracking-wider uppercase text-primary-foreground bg-primary rounded-sm px-3 py-2 box-glow-neon hover:opacity-90 transition-opacity min-h-[36px]"
        >
          🎮 3D-Game
        </a>
        <a
          href="/online"
          className="hidden lg:inline-flex items-center gap-2 font-mono text-[11px] tracking-wider uppercase text-primary border border-primary/60 rounded-sm px-3 py-2 hover:bg-primary/10 transition-colors min-h-[36px]"
        >
          📡 Online
        </a>

        {/* Mobile Menu Button */}
        <button
          type="button"
          onClick={() => setMenuOpen(!menuOpen)}
          className="lg:hidden flex flex-col gap-1.5 p-2 min-w-[44px] min-h-[44px] items-center justify-center"
          aria-label="Men\u00fc \u00f6ffnen"
        >
          <span
            className={`w-5 h-0.5 bg-foreground transition-transform ${
              menuOpen ? "rotate-45 translate-y-1" : ""
            }`}
          />
          <span
            className={`w-5 h-0.5 bg-foreground transition-opacity ${
              menuOpen ? "opacity-0" : ""
            }`}
          />
          <span
            className={`w-5 h-0.5 bg-foreground transition-transform ${
              menuOpen ? "-rotate-45 -translate-y-1" : ""
            }`}
          />
        </button>
      </div>

      {/* Mobile Menu */}
      {menuOpen && (
        <div className="lg:hidden bg-background/95 backdrop-blur-md border-b border-border">
          <div className="px-6 py-4 flex flex-col gap-1">
            <a
              href="/play"
              onClick={() => setMenuOpen(false)}
              className="font-mono text-sm tracking-wider uppercase text-primary glow-neon-sm py-3 min-h-[44px] flex items-center"
            >
              ▶ Demo spielen
            </a>
            {navLinks.map((link) => (
              <a
                key={link.href}
                href={link.href}
                onClick={() => setMenuOpen(false)}
                className="font-mono text-sm tracking-wider uppercase text-muted-foreground hover:text-primary transition-colors py-3 min-h-[44px] flex items-center"
              >
                {link.label}
              </a>
            ))}
          </div>
        </div>
      )}
    </nav>
  );
}
