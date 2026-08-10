"use client";

import { useEffect, useRef, useState } from "react";
import { Volume2, Activity, Waves, Radio } from "lucide-react";

/* ------------------------------------------------------------------ */
/* WebAudio-Demos: organisch vs. mechanisch                            */
/* ------------------------------------------------------------------ */

function useAudioCtx() {
  const ctxRef = useRef<AudioContext | null>(null);
  const getCtx = () => {
    if (typeof window === "undefined") return null;
    const AC =
      window.AudioContext ??
      (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!AC) return null;
    if (!ctxRef.current) ctxRef.current = new AC();
    if (ctxRef.current.state === "suspended") void ctxRef.current.resume();
    return ctxRef.current;
  };
  return getCtx;
}

function playOrganic(getCtx: () => AudioContext | null) {
  const ctx = getCtx();
  if (!ctx) return;
  const t = ctx.currentTime;

  // Tiefes, wachsendes Grollen
  const osc = ctx.createOscillator();
  const gain = ctx.createGain();
  osc.type = "sine";
  osc.frequency.setValueAtTime(140, t);
  osc.frequency.exponentialRampToValueAtTime(38, t + 0.7);
  gain.gain.setValueAtTime(0.0001, t);
  gain.gain.exponentialRampToValueAtTime(0.35, t + 0.06);
  gain.gain.exponentialRampToValueAtTime(0.0001, t + 0.8);
  osc.connect(gain).connect(ctx.destination);
  osc.start(t);
  osc.stop(t + 0.85);

  // Feuchtes Oberton-Flirren
  const shimmer = ctx.createOscillator();
  const sGain = ctx.createGain();
  shimmer.type = "triangle";
  shimmer.frequency.setValueAtTime(520, t);
  shimmer.frequency.exponentialRampToValueAtTime(220, t + 0.5);
  sGain.gain.setValueAtTime(0.0001, t);
  sGain.gain.exponentialRampToValueAtTime(0.08, t + 0.04);
  sGain.gain.exponentialRampToValueAtTime(0.0001, t + 0.5);
  shimmer.connect(sGain).connect(ctx.destination);
  shimmer.start(t);
  shimmer.stop(t + 0.55);
}

function playMechanic(getCtx: () => AudioContext | null) {
  const ctx = getCtx();
  if (!ctx) return;
  const t0 = ctx.currentTime;

  // Drei harte, industrielle Impulse (Feuerstoß)
  for (let i = 0; i < 3; i++) {
    const t = t0 + i * 0.12;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = "square";
    osc.frequency.setValueAtTime(160 - i * 12, t);
    osc.frequency.exponentialRampToValueAtTime(50, t + 0.09);
    gain.gain.setValueAtTime(0.0001, t);
    gain.gain.exponentialRampToValueAtTime(0.22, t + 0.008);
    gain.gain.exponentialRampToValueAtTime(0.0001, t + 0.1);
    osc.connect(gain).connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.12);
  }
}

/* ------------------------------------------------------------------ */

const LAYERS = [
  {
    icon: Waves,
    title: "Exploration-Layer",
    text: "Dünne Synthesizer-Flächen, Wind durch Ruinen, darunter ein kaum hörbares Biomass-Grollen im Sub-Bass. Die Welt atmet.",
    bars: [30, 45, 25, 55, 35, 20, 40],
  },
  {
    icon: Activity,
    title: "Combat-Layer",
    text: "Percussive Industrie-Pulse, die mit der Spieler-AGGRO anschwellen. Organische und mechanische Waffen erhalten eigene Klangfamilien.",
    bars: [70, 90, 60, 100, 80, 65, 95],
  },
  {
    icon: Radio,
    title: "Dominanz-Layer",
    text: "Hält dein Team alle Zonen oder das HQ, steigt ein chorales Biomass-Motiv ein – Triumph, der sich falsch anfühlt. Absichtlich.",
    bars: [50, 75, 95, 80, 100, 70, 85],
  },
];

const CUES = [
  "Fußstufen nach Material & Gewicht – du hörst, ob ein Carapax-Build anrückt.",
  "Reload als hörbarer Wachstumsprozess (organisch) vs. mechanisches Nachladen.",
  "Richtungs-Indikatoren als Subtitle-Option für alle Kampf-Cues.",
  "HQ-Alarm und Zonen-Capture mit eigenem, ortbaren Sound-Signatur.",
];

export function SoundSection() {
  const [isVisible, setIsVisible] = useState(false);
  const [playing, setPlaying] = useState<"organisch" | "mechanisch" | null>(null);
  const sectionRef = useRef<HTMLElement>(null);
  const getCtx = useAudioCtx();

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

  const demo = (kind: "organisch" | "mechanisch") => {
    try {
      if (kind === "organisch") playOrganic(getCtx);
      else playMechanic(getCtx);
      setPlaying(kind);
      window.setTimeout(() => setPlaying(null), 900);
    } catch {
      /* Audio nicht verfügbar – Demo bleibt stumm, Seite läuft weiter */
    }
  };

  return (
    <section ref={sectionRef} id="sound" className="relative py-20 md:py-32 px-6">
      <div className="max-w-6xl mx-auto">
        <div
          className={`flex items-center gap-4 mb-6 transition-all duration-700 ${
            isVisible ? "opacity-100 translate-x-0" : "opacity-0 -translate-x-8"
          }`}
        >
          <span className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
            10 // Sound &amp; Musik
          </span>
          <div className="flex-1 h-px bg-gradient-to-r from-primary/40 to-transparent" />
        </div>

        <h2
          className={`text-3xl sm:text-4xl md:text-5xl font-bold text-foreground mb-4 leading-tight text-balance transition-all duration-700 delay-100 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          Die Biomass
          <span className="text-primary glow-neon-sm"> hat eine Stimme</span>
        </h2>
        <p
          className={`text-base md:text-lg text-muted-foreground max-w-3xl mb-10 leading-relaxed transition-all duration-700 delay-200 ${
            isVisible ? "opacity-100" : "opacity-0"
          }`}
        >
          Ein adaptives Drei-Schichten-System reagiert auf Kampf, Kontrolle und
          Präsenz der Biomass. Und die zwei Waffen-Doktrinen klingen so
          verschieden, wie sie kämpfen – hör rein (WebAudio-Demo, direkt im Browser synthetisiert):
        </p>

        {/* Audio Demo Buttons */}
        <div
          className={`flex flex-wrap gap-3 mb-12 transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <button
            type="button"
            onClick={() => demo("organisch")}
            className={`flex items-center gap-3 px-5 py-3.5 rounded-sm border font-mono text-xs sm:text-sm tracking-wider uppercase transition-all min-h-[44px] ${
              playing === "organisch"
                ? "border-primary bg-primary/20 text-primary box-glow-neon"
                : "border-primary/40 bg-primary/10 text-primary hover:bg-primary/20"
            }`}
          >
            <Volume2 className="w-4 h-4" strokeWidth={1.75} />
            {playing === "organisch" ? "Spielt …" : "Organisch: DORN"}
          </button>
          <button
            type="button"
            onClick={() => demo("mechanisch")}
            className={`flex items-center gap-3 px-5 py-3.5 rounded-sm border font-mono text-xs sm:text-sm tracking-wider uppercase transition-all min-h-[44px] ${
              playing === "mechanisch"
                ? "border-foreground bg-foreground/10 text-foreground box-glow-neon"
                : "border-border bg-secondary/60 text-secondary-foreground hover:border-foreground/40"
            }`}
          >
            <Volume2 className="w-4 h-4" strokeWidth={1.75} />
            {playing === "mechanisch" ? "Spielt …" : "Mechanisch: TEMPEST"}
          </button>
        </div>

        {/* Music Layers */}
        <div className="grid md:grid-cols-3 gap-4 mb-12">
          {LAYERS.map((layer, i) => {
            const Icon = layer.icon;
            return (
              <div
                key={layer.title}
                className={`border border-border bg-card rounded-sm p-5 transition-all duration-700 ${
                  isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
                }`}
                style={{ transitionDelay: `${200 + i * 120}ms` }}
              >
                <div className="flex items-center gap-3 mb-4">
                  <span className="w-9 h-9 rounded-sm border border-primary/30 bg-primary/10 flex items-center justify-center">
                    <Icon className="w-4 h-4 text-primary" strokeWidth={1.75} />
                  </span>
                  <h3 className="text-sm font-bold text-foreground">{layer.title}</h3>
                </div>
                {/* Fake equalizer */}
                <div className="flex items-end gap-1 h-10 mb-4" aria-hidden="true">
                  {layer.bars.map((h, j) => (
                    <div
                      key={j}
                      className="flex-1 bg-primary/60 rounded-t-sm animate-pulse-neon"
                      style={{ height: `${h}%`, animationDelay: `${j * 180}ms` }}
                    />
                  ))}
                </div>
                <p className="text-sm text-muted-foreground leading-relaxed">{layer.text}</p>
              </div>
            );
          })}
        </div>

        {/* Audio Cues */}
        <div
          className={`border border-border bg-card rounded-sm p-6 md:p-8 transition-all duration-700 delay-300 ${
            isVisible ? "opacity-100 translate-y-0" : "opacity-0 translate-y-8"
          }`}
        >
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-5">
            Gameplay-Audio-Cues
          </p>
          <ul className="grid sm:grid-cols-2 gap-x-8 gap-y-3">
            {CUES.map((cue, i) => (
              <li key={i} className="flex items-start gap-3">
                <span className="mt-1.5 w-1.5 h-1.5 rounded-full bg-primary shrink-0" />
                <span className="text-sm text-secondary-foreground leading-relaxed">{cue}</span>
              </li>
            ))}
          </ul>
        </div>
      </div>
    </section>
  );
}
