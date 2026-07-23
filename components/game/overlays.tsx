"use client";

import { gameStore, useGameState } from "@/lib/game/store";
import { resetWorld } from "@/lib/game/shared";

export function Overlays() {
  const phase = useGameState((s) => s.phase);
  const wave = useGameState((s) => s.wave);
  const score = useGameState((s) => s.score);
  const kills = useGameState((s) => s.kills);

  if (phase === "playing") return null;

  const handleStart = () => {
    resetWorld();
    gameStore.reset();
    gameStore.set({ phase: "playing" });
  };

  const handleRestart = handleStart;

  return (
    <div className="absolute inset-0 z-20 flex items-center justify-center bg-background/85 backdrop-blur-sm scanlines">
      <div className="relative mx-4 w-full max-w-lg">
        {phase === "menu" && (
          <div className="animate-fade-in text-center">
            <p className="font-mono text-xs uppercase tracking-[0.4em] text-primary glow-neon-sm">
              Prototyp // Spielbare Demo
            </p>
            <h1 className="mt-4 text-6xl font-bold tracking-tight text-foreground text-balance md:text-7xl">
              <span className="text-primary glow-neon">WIRR</span>WARR
            </h1>
            <p className="mx-auto mt-4 max-w-md text-pretty leading-relaxed text-muted-foreground">
              Ein Third-Person-Shooter gegen die Biomasse. &Uuml;berlebe die
              Wellen, sprenge dir Schusslinien frei und halte den{" "}
              <span className="text-primary">Wirrwarr</span> auf.
            </p>

            <div className="mx-auto mt-8 max-w-sm rounded-md border border-border bg-card/60 p-5 text-left">
              <p className="mb-3 font-mono text-xs uppercase tracking-wider text-primary">
                Steuerung
              </p>
              <ul className="space-y-2 font-mono text-sm text-muted-foreground">
                <ControlRow keys="WASD" action="Bewegen" />
                <ControlRow keys="Maus" action="Zielen" />
                <ControlRow keys="Linksklick" action="Schießen" />
                <ControlRow keys="Shift" action="Sprinten" />
                <ControlRow keys="Leertaste" action="Ausweichrolle" />
                <ControlRow keys="R" action="Nachladen" />
              </ul>
            </div>

            <button
              type="button"
              onClick={handleStart}
              className="mt-8 min-h-[52px] w-full max-w-xs rounded-md bg-primary px-8 font-mono text-sm font-bold uppercase tracking-widest text-primary-foreground transition-all hover:box-glow-neon hover:brightness-110"
            >
              Einsatz starten
            </button>
            <p className="mt-4 font-mono text-xs text-muted-foreground/60">
              Klicke ins Spielfeld, um die Maussteuerung zu aktivieren
            </p>
          </div>
        )}

        {phase === "gameover" && (
          <div className="animate-fade-in text-center">
            <p className="font-mono text-xs uppercase tracking-[0.4em] text-destructive">
              Verbindung verloren
            </p>
            <h2 className="mt-4 text-5xl font-bold tracking-tight text-foreground md:text-6xl">
              AUSGEL&Ouml;SCHT
            </h2>
            <p className="mt-3 text-muted-foreground">
              Der Wirrwarr hat dich &uuml;berrannt.
            </p>

            <div className="mx-auto mt-8 grid max-w-sm grid-cols-3 gap-3">
              <StatBox label="Welle" value={wave} />
              <StatBox label="Kills" value={kills} />
              <StatBox label="Punkte" value={score} />
            </div>

            <button
              type="button"
              onClick={handleRestart}
              className="mt-8 min-h-[52px] w-full max-w-xs rounded-md bg-primary px-8 font-mono text-sm font-bold uppercase tracking-widest text-primary-foreground transition-all hover:box-glow-neon hover:brightness-110"
            >
              Erneut einsetzen
            </button>
          </div>
        )}
      </div>
    </div>
  );
}

function ControlRow({ keys, action }: { keys: string; action: string }) {
  return (
    <li className="flex items-center justify-between gap-4">
      <span className="rounded border border-border bg-secondary px-2 py-1 text-xs text-foreground">
        {keys}
      </span>
      <span className="text-right">{action}</span>
    </li>
  );
}

function StatBox({ label, value }: { label: string; value: number }) {
  return (
    <div className="rounded-md border border-border bg-card/60 p-4">
      <p className="text-3xl font-bold text-primary glow-neon-sm">{value}</p>
      <p className="mt-1 font-mono text-xs uppercase tracking-wider text-muted-foreground">
        {label}
      </p>
    </div>
  );
}
