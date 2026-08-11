/* SAVE THE WORLD – Logo (Schild + Globus + Fadenkreuz) als Inline-SVG */
export function STWLogo({ className = "w-8 h-8" }: { className?: string }) {
  return (
    <svg viewBox="0 0 64 64" className={className} fill="none" aria-label="SAVE THE WORLD Logo" role="img">
      <path
        d="M32 3 L56 14 V32 C56 46 46 56 32 61 C18 56 8 46 8 32 V14 Z"
        stroke="currentColor"
        strokeWidth="3"
        className="text-primary"
      />
      <circle cx="32" cy="32" r="14" stroke="currentColor" strokeWidth="2" className="text-primary" />
      <ellipse cx="32" cy="32" rx="14" ry="5.5" stroke="currentColor" strokeWidth="1.2" className="text-primary" opacity="0.8" />
      <ellipse cx="32" cy="32" rx="5.5" ry="14" stroke="currentColor" strokeWidth="1.2" className="text-primary" opacity="0.8" />
      <path d="M32 10 V23 M32 41 V54 M10 32 H23 M41 32 H54" stroke="currentColor" strokeWidth="2.5" className="text-primary" />
      <circle cx="32" cy="32" r="3.2" fill="currentColor" className="text-primary" />
    </svg>
  );
}
