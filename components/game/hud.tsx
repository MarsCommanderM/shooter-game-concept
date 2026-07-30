"use client";

import { useEffect, useRef, useState } from "react";
import { useGameState } from "@/lib/game/store";

export function Hud() {
  const phase = useGameState((s) => s.phase);
  const health = useGameState((s) => s.health);
  const maxHealth = useGameState((s) => s.maxHealth);
  const score = useGameState((s) => s.score);
  const wave = useGameState((s) => s.wave);
  const enemies = useGameState((s) => s.enemiesRemaining);
  const ammo = useGameState((s) => s.ammo);
  const maxAmmo = useGameState((s) => s.maxAmmo);
  const reloading = useGameState((s) => s.reloading);
  const hitMarkerAt = useGameState((s) => s.hitMarkerAt);
  const killMarkerAt = useGameState((s) => s.killMarkerAt);
  const damageAt = useGameState((s) => s.damageAt);

  // Transient feedback flags.
  const [hit, setHit] = useState(false);
  const [kill, setKill] = useState(false);
  const [damage, setDamage] = useState(false);
  const [spread, setSpread] = useState(0);
  const prevAmmo = useRef(ammo);

  useEffect(() => {
    if (!hitMarkerAt) return;
    setHit(true);
    const t = setTimeout(() => setHit(false), 110);
    return () => clearTimeout(t);
  }, [hitMarkerAt]);

  useEffect(() => {
    if (!killMarkerAt) return;
    setKill(true);
    const t = setTimeout(() => setKill(false), 260);
    return () => clearTimeout(t);
  }, [killMarkerAt]);

  useEffect(() => {
    if (!damageAt) return;
    setDamage(true);
    const t = setTimeout(() => setDamage(false), 220);
    return () => clearTimeout(t);
  }, [damageAt]);

  // Crosshair recoil bloom whenever a shot is fired (ammo decreased).
  useEffect(() => {
    if (ammo < prevAmmo.current) {
      setSpread(9);
      const t = setTimeout(() => setSpread(0), 90);
      prevAmmo.current = ammo;
      return () => clearTimeout(t);
    }
    prevAmmo.current = ammo;
  }, [ammo]);

  if (phase !== "playing") return null;

  const healthPct = (health / maxHealth) * 100;
  const healthColor =
    healthPct > 50 ? "bg-primary" : healthPct > 25 ? "bg-yellow-400" : "bg-destructive";
  const lowHealth = healthPct <= 25;

  return (
    <div className="pointer-events-none absolute inset-0 z-20 select-none font-mono">
      {/* Damage vignette */}
      <div
        className="absolute inset-0 transition-opacity duration-200"
        style={{
          opacity: damage ? 0.9 : lowHealth ? 0.5 : 0,
          background:
            "radial-gradient(ellipse at center, transparent 45%, rgba(220,20,40,0.55) 100%)",
          animation: lowHealth && !damage ? "pulse-neon 1.4s ease-in-out infinite" : undefined,
        }}
      />

      {/* Crosshair */}
      <div className="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2">
        <div className="relative h-10 w-10">
          <span
            className="absolute left-1/2 top-1/2 h-2.5 w-0.5 -translate-x-1/2 bg-primary/90 transition-transform duration-75"
            style={{ transform: `translate(-50%, calc(-50% - ${8 + spread}px))` }}
          />
          <span
            className="absolute left-1/2 top-1/2 h-2.5 w-0.5 -translate-x-1/2 bg-primary/90 transition-transform duration-75"
            style={{ transform: `translate(-50%, calc(-50% + ${8 + spread}px))` }}
          />
          <span
            className="absolute left-1/2 top-1/2 h-0.5 w-2.5 -translate-y-1/2 bg-primary/90 transition-transform duration-75"
            style={{ transform: `translate(calc(-50% - ${8 + spread}px), -50%)` }}
          />
          <span
            className="absolute left-1/2 top-1/2 h-0.5 w-2.5 -translate-y-1/2 bg-primary/90 transition-transform duration-75"
            style={{ transform: `translate(calc(-50% + ${8 + spread}px), -50%)` }}
          />
          <span className="absolute left-1/2 top-1/2 h-0.5 w-0.5 -translate-x-1/2 -translate-y-1/2 rounded-full bg-primary" />

          {/* Hit marker */}
          {hit && (
            <svg
              className="absolute left-1/2 top-1/2 h-6 w-6 -translate-x-1/2 -translate-y-1/2"
              viewBox="0 0 24 24"
              aria-hidden="true"
            >
              <title>Treffer</title>
              <path
                d="M5 5 L9 9 M19 5 L15 9 M5 19 L9 15 M19 19 L15 15"
                stroke={kill ? "#ffd23f" : "#ffffff"}
                strokeWidth="2"
                strokeLinecap="round"
              />
            </svg>
          )}
        </div>
      </div>

      {/* Top bar: score + wave */}
      <div className="absolute left-0 right-0 top-0 flex items-start justify-between p-4 md:p-6">
        <div className="rounded border border-primary/30 bg-background/70 px-4 py-2 backdrop-blur-sm">
          <p className="text-[10px] uppercase tracking-widest text-muted-foreground">Punkte</p>
          <p className="text-2xl font-bold text-primary glow-neon-sm tabular-nums">
            {score.toLocaleString("de-DE")}
          </p>
        </div>
        <div className="rounded border border-primary/30 bg-background/70 px-4 py-2 text-right backdrop-blur-sm">
          <p className="text-[10px] uppercase tracking-widest text-muted-foreground">Welle</p>
          <p className="text-2xl font-bold text-foreground tabular-nums">{wave}</p>
        </div>
      </div>

      {/* Enemies remaining (top center) */}
      <div className="absolute left-1/2 top-4 -translate-x-1/2 md:top-6">
        <div className="rounded border border-destructive/40 bg-background/70 px-4 py-1.5 backdrop-blur-sm">
          <p className="text-[10px] uppercase tracking-widest text-muted-foreground">
            Feinde &middot; {enemies}
          </p>
        </div>
      </div>

      {/* Wave-clear banner */}
      {enemies === 0 && (
        <div className="absolute left-1/2 top-1/3 -translate-x-1/2 animate-pulse-neon text-center">
          <p className="text-lg uppercase tracking-[0.3em] text-primary glow-neon">
            Welle bereinigt
          </p>
          <p className="mt-1 text-xs uppercase tracking-widest text-muted-foreground">
            N&auml;chste Welle eingehend...
          </p>
        </div>
      )}

      {/* Health bar (bottom-left) */}
      <div className="absolute bottom-4 left-4 w-48 md:bottom-6 md:left-6 md:w-64">
        <div className="mb-1 flex items-center justify-between">
          <span className="text-[10px] uppercase tracking-widest text-muted-foreground">
            Integrit&auml;t
          </span>
          <span className="text-xs font-bold text-foreground tabular-nums">
            {Math.ceil(health)}
          </span>
        </div>
        <div className="h-3 w-full overflow-hidden rounded-sm border border-primary/30 bg-secondary">
          <div
            className={`h-full ${healthColor} transition-all duration-200`}
            style={{ width: `${healthPct}%` }}
          />
        </div>
      </div>

      {/* Ammo (bottom-right) */}
      <div className="absolute bottom-4 right-4 text-right md:bottom-6 md:right-6">
        <p className="text-[10px] uppercase tracking-widest text-muted-foreground">Munition</p>
        {reloading ? (
          <p className="animate-pulse text-lg font-bold uppercase text-yellow-400">
            Nachladen...
          </p>
        ) : (
          <p className="text-3xl font-bold tabular-nums text-foreground">
            <span className={ammo <= 5 ? "text-destructive" : "text-primary"}>{ammo}</span>
            <span className="text-base text-muted-foreground"> / {maxAmmo}</span>
          </p>
        )}
      </div>
    </div>
  );
}
