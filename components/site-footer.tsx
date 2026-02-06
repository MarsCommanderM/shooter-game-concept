export function SiteFooter() {
  return (
    <footer className="relative py-16 px-6 border-t border-border">
      <div className="max-w-6xl mx-auto">
        <div className="flex flex-col md:flex-row items-start md:items-center justify-between gap-8">
          <div>
            <p className="text-2xl font-bold text-foreground mb-2">
              <span className="text-primary glow-neon-sm">WIRR</span>WARR
            </p>
            <p className="font-mono text-xs text-muted-foreground tracking-wider">
              Game Design Document // Konzeptphase
            </p>
          </div>

          <div className="flex flex-col items-start md:items-end gap-2">
            <p className="font-mono text-xs text-muted-foreground tracking-wider">
              Third-Person-Shooter / Taktik / Destruktion
            </p>
            <p className="font-mono text-xs text-muted-foreground/60 tracking-wider">
              Version 1.0 // Vertraulich
            </p>
          </div>
        </div>

        {/* Divider */}
        <div className="my-8 h-px bg-gradient-to-r from-transparent via-border to-transparent" />

        {/* Bottom */}
        <div className="flex flex-col sm:flex-row items-center justify-between gap-4">
          <p className="font-mono text-[10px] text-muted-foreground/50 tracking-wider">
            DIESES DOKUMENT IST VERTRAULICH UND NUR F&Uuml;R INTERNE NUTZUNG BESTIMMT
          </p>
          <div className="flex items-center gap-2">
            <span className="w-1.5 h-1.5 rounded-full bg-primary animate-pulse-neon" />
            <span className="font-mono text-[10px] text-primary/60 tracking-wider">
              IN ENTWICKLUNG
            </span>
          </div>
        </div>
      </div>
    </footer>
  );
}
