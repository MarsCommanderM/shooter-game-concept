"use client";

import { useEffect, useRef, useState } from "react";
import * as THREE from "three";
import { EffectComposer } from "three/examples/jsm/postprocessing/EffectComposer.js";
import { RenderPass } from "three/examples/jsm/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/examples/jsm/postprocessing/UnrealBloomPass.js";

/* ================================================================== */
/* WIRRWARR – Vertical Slice (echte 3D-Engine, Three.js)               */
/* Phase 1: Destruction + Gunfeel + Bots in einer Arena                */
/* ================================================================== */

type PerkId = "sprint" | "panzer" | "sprung";
const SKINS = [
  { id: "green", name: "Toxin-Grün", color: "#22ff55", level: 1 },
  { id: "cyan", name: "Echo-Cyan", color: "#33ccff", level: 2 },
  { id: "amber", name: "Funken-Amber", color: "#ffcc33", level: 4 },
  { id: "pink", name: "Myzel-Pink", color: "#ff66cc", level: 6 },
  { id: "white", name: "Apex-Weiß", color: "#ffffff", level: 10 },
];
const LEVEL_XP = [0, 400, 1000, 1800, 2800, 4000, 5400, 7000, 8800, 10800, 13000, 15500, 18300, 21400, 24800];
function levelFromXp(xp: number): number {
  let lv = 1;
  for (let i = 0; i < LEVEL_XP.length; i++) if (xp >= LEVEL_XP[i]) lv = i + 1;
  return lv;
}
const PROF_KEY = "wirrwarr-profile";
const LOAD_KEY = "wirrwarr-loadout";
function loadProfile(): { xp: number } {
  try { return JSON.parse(localStorage.getItem(PROF_KEY) ?? "null") ?? { xp: 0 }; } catch { return { xp: 0 }; }
}
interface LoadoutData { streaks: string[]; skin: string; attach: Record<string, string[]>; }
function loadLoadout(): LoadoutData {
  try { return JSON.parse(localStorage.getItem(LOAD_KEY) ?? "null") ?? { streaks: ["recon", "mortar", "rage"], skin: "green", attach: {} }; }
  catch { return { streaks: ["recon", "mortar", "rage"], skin: "green", attach: {} }; }
}
const KSTREAKS = [
  { num: 3, id: "recon", name: "Recon-Puls", level: 1, desc: "Markiert alle Feinde 5 s lang" },
  { num: 4, id: "ammo", name: "Nachschub", level: 2, desc: "+2 Granaten, Munition sofort voll" },
  { num: 5, id: "mortar", name: "Mörserschlag", level: 3, desc: "Detonation am Zielpunkt" },
  { num: 7, id: "orbital", name: "Orbital-Schlag", level: 5, desc: "3-fache Detonation – sprengt Wände" },
  { num: 10, id: "rage", name: "Biomass-Rage", level: 8, desc: "10 s: +50 % Schaden, +Tempo, Schild voll" },
];
interface CineScene { cam: [number, number, number]; look: [number, number, number]; dur: number; speaker?: string; text?: string; }
interface StoryEvent { trigger: "time" | "kills" | "destroyed"; at: number; speaker: string; text: string; shake?: boolean; }
const STORY: Record<string, { intro: CineScene[]; debrief: string; events: StoryEvent[] }> = {
  m0: {
    intro: [
      { cam: [0, 30, 4], look: [0, 0, -10], dur: 4, speaker: "???", text: "Mayday. Mayday. Dropship K-11 geht nieder – Sektor 7 ist nicht evakuiert. Ist nicht evakuiert worden." },
      { cam: [4, 1.6, 12], look: [0, 1, -16], dur: 3.5, speaker: "DU", text: "Atmen. Aufstehen. Irgendwo da vorn liegt meine Waffe." },
    ],
    debrief: "Kontakt bestätigt. Die Biomass hat dich markiert – und etwas in dir hat zurückgesehen.",
    events: [
      { trigger: "time", at: 2, speaker: "SYSTEM", text: "WASD bewegen · Shift sprinten · C ducken · Space springen" },
      { trigger: "time", at: 12, speaker: "VEGA", text: "Signatur erkannt: Deine DORN liegt vorne bei der Markierung. Hol sie." },
    ],
  },
  m5: {
    intro: [
      { cam: [0, 24, -20], look: [0, 0, 6], dur: 4.5, speaker: "DR. MAREN [FLÜSTERN]", text: "Die Terminals sind hier. Drei Downloads. Dann sind wir weg – leise." },
      { cam: [8, 2, -10], look: [-6, 1.5, 6], dur: 3.5, speaker: "DU", text: "Wachen patrouillieren. Messer raus. Funk aus." },
    ],
    debrief: "Drei Downloads. Eine Wahrheit: Die KORP hat die Erde selbst geerntet. Und dein KOMMANDO steckt drin.",
    events: [
      { trigger: "destroyed", at: 1, speaker: "KOMMANDO [FUNK]", text: "Soldat. Stoppen Sie die Übertragung. Das ist ein Befehl.", shake: true },
      { trigger: "destroyed", at: 3, speaker: "DR. MAREN", text: "ER weiß es. RAUS. Zum nördlichen Extraktionspunkt – JETZT!", shake: true },
    ],
  },

  m1: {
    intro: [
      { cam: [-34, 16, -34], look: [0, 0, 0], dur: 4.5, speaker: "KOMMANDO [FUNK]", text: "Soldat. Die Biomass hat Sektor 7 erreicht. Sie sieht mit tausend Augen – also leih ihr keines deiner." },
      { cam: [12, 2.5, 20], look: [-12, 1.5, -12], dur: 4, speaker: "VEGA", text: "Das Glitzern in der Luft? Das ist kein Nebel. Das sind Sporen. Und sie fallen nicht – sie suchen." },
      { cam: [0, 1.8, 10], look: [0, 1.6, -24], dur: 3.5, speaker: "DU", text: "Dann atmen wir schneller, als sie wachsen. Erste Ernte beginnt." },
    ],
    debrief: "Sektor 7 gesichert. Die Proben zeigen: Die Biomass reagiert auf deine Präsenz. Sie lernt.",
    events: [{ trigger: "kills", at: 4, speaker: "KOMMANDO [FUNK]", text: "Vier bestätigt. Sie funken um Verstärkung – oder nach dir." }],
  },
  m2: {
    intro: [
      { cam: [0, 20, 26], look: [0, 2, 0], dur: 4.5, speaker: "KOMMANDO [FUNK]", text: "Die Strukturen sind infiziert. In drei Minuten sporen sie aus. Reiß sie nieder – alle sechs." },
      { cam: [6, 2, 8], look: [0, 2, 0], dur: 3.5, speaker: "VEGA", text: "Der BRECHER-7 ist heute dein Skalpell. Und dein Hammer." },
    ],
    debrief: "Sechs Strukturen gefallen. Unter der dritten Schicht haben wir etwas gefunden: Es hat zurückgefunkt.",
    events: [{ trigger: "destroyed", at: 3, speaker: "VEGA", text: "⚠ Das Nest registriert deine Sprengungen. Es wird unruhig.", shake: true }],
  },
  m3: {
    intro: [
      { cam: [-20, 10, 24], look: [0, 1, 0], dur: 4.5, speaker: "KOMMANDO [FUNK]", text: "Halte die Stellung. Was auch kommt – es kommt zuerst zu dir." },
      { cam: [4, 1.8, -8], look: [-8, 1.5, 12], dur: 3.5, speaker: "JUNO", text: "Meine Sensoren: Bewegung. Viel Bewegung." },
    ],
    debrief: "Stellung gehalten. Die Biomass zieht sich zurück – wie ein Ozean vor dem Sturm.",
    events: [{ trigger: "time", at: 60, speaker: "JUNO", text: "Zweite Welle! Sie haben deine Taktik gelesen.", shake: true }],
  },
  m9: {
    intro: [
      { cam: [0, 4, -14], look: [0, 1.6, 14], dur: 4.5, speaker: "KADE", text: "Ich habe dein Loadout gelesen. Deine Streaks. Deine Ängste. Ich bin nicht dein Gegner – ich bin deine Quittung." },
      { cam: [6, 2, 8], look: [0, 1.6, 14], dur: 3, speaker: "DU", text: "Dann weißt du auch, wie ich kämpfe. Und wie ich dich schlage." },
    ],
    debrief: "KADE gefallen. Sein letzter Funk: ‚Hale sieht dich. Und Hale ist nicht allein.‘",
    events: [{ trigger: "time", at: 30, speaker: "KADE", text: "Orbital? DEIN Orbital. Danke fürs Teilen.", shake: true }],
  },
  m10: {
    intro: [
      { cam: [0, 6, -20], look: [0, 1, 10], dur: 5, speaker: "—", text: "Kein Funk. Nur Wind durch Glas. Und etwas, das wie Kinderlachen klingt, wenn man nicht genau hinhört." },
    ],
    debrief: "Vier Erinnerungen. Eine davon trägt einen Namen, den du kennst: HALE. Das Nest war nie eine Waffe. Es war ein Garten für jemanden.",
    events: [
      { trigger: "time", at: 8, speaker: "WAND-INSCHRIFT", text: "‚ERNTEDANK 2041 – DANKE, KORP‘ (verblasst, von Sporen halb gefressen)" },
      { trigger: "time", at: 22, speaker: "JUNO [FLÜSTERT]", text: "Ich empfange keine Feinde. Aber die Biomass … sie singt hier. Warum singt sie?" },
      { trigger: "time", at: 36, speaker: "???", text: "…Papa?…", shake: true },
    ],
  },
  m11: {
    intro: [
      { cam: [0, 20, 20], look: [0, 2, 0], dur: 4.5, speaker: "DR. MAREN", text: "ERNTE-LÄUFER. KORPs Belagerungsmaschine. Sein Schild frisst alles – außer dem Strom aus drei Pylonen." },
      { cam: [8, 2, -6], look: [-4, 2, 8], dur: 3, speaker: "DU", text: "BRECHER geladen. Pylone markiert. Erntezeit." },
    ],
    debrief: "Der Läufer fiel. In seinem Wrack: ein Frachtplan. Das Nest hat den Tisch gedeckt.",
    events: [{ trigger: "time", at: 40, speaker: "JUNO", text: "Seine Panzerung adaptiert! PYLON-TAKTIK, JETZT!", shake: true }],
  },
  m12: {
    intro: [
      { cam: [0, 28, 0.1], look: [0, 0, 0], dur: 5, speaker: "DER GÄRTNER", text: "Du trittst auf meinen Rasen, kleines Werkzeug. Ich habe Welten gepflanzt. Was pflanzt du?" },
      { cam: [6, 2, 8], look: [0, 3, 0], dur: 3.5, speaker: "DU", text: "Eine Bresche." },
    ],
    debrief: "",
    events: [{ trigger: "time", at: 60, speaker: "DER GÄRTNER", text: "Interessant. Du wächst.", shake: true }],
  },
  m6: {
    intro: [
      { cam: [-10, 3, -16], look: [4, 1.5, 8], dur: 4.5, speaker: "DR. MAREN", text: "Ich habe die Ernte mitdesignt. Deshalb wissen sie, dass ich komme. Und deshalb musst DU jetzt schnell sein." },
      { cam: [6, 2, 6], look: [-6, 1.5, -6], dur: 3.5, speaker: "DU", text: "Regel eins: Du bleibst hinter mir. Regel zwei: Es gibt keine Regel zwei." },
    ],
    debrief: "Maren lebt. Und sie redet. Akt 2 beginnt mit einem Namen: DIREKTOR HALE.",
    events: [{ trigger: "time", at: 45, speaker: "DR. MAREN", text: "Sie funken meine Position! LAUF!", shake: true }],
  },
  m7: {
    intro: [
      { cam: [0, 18, -18], look: [0, 1, 10], dur: 4.5, speaker: "DR. MAREN [FUNK]", text: "KORP schickt Mechs, die Biomass schickt Sporen. Beide hassen dich. Beide hassen einander. Nutz das." },
      { cam: [8, 2, 4], look: [-8, 1.5, 4], dur: 3.5, speaker: "DU", text: "Lockdrocks. Wir hetzen sie aufeinander und ernten die Reste." },
    ],
    debrief: "Zehn Abschüsse. Doch die Mechs haben etwas gelernt: Sie haben NICHT auf die Drocks reagiert. Jemand steuert sie neu.",
    events: [{ trigger: "kills", at: 5, speaker: "JUNO", text: "Hälfte down! Aber die Mechs rotieren – sie lernen dein Muster!", shake: true }],
  },
  m8: {
    intro: [
      { cam: [0, 14, -14], look: [0, 1, 0], dur: 4.5, speaker: "KOMMANDO [FUNK – VERZERRT]", text: "…Vergiss die Wissenschaftlerin. Der Kern ist die Mission. Der Kern IST die Mission…" },
      { cam: [2, 2, -6], look: [0, 1.5, 2], dur: 3.5, speaker: "VEGA [SCHWACH]", text: "…ich wüsste wirklich gern, wie dein Gesicht aussieht, wenn du dich entscheidest…" },
    ],
    debrief: "",
    events: [{ trigger: "time", at: 70, speaker: "JUNO", text: "Strukturintegrity 40 %! Das Dach kommt!", shake: true }],
  },
  m4: {
    intro: [
      { cam: [0, 26, 0.1], look: [0, 0, 0], dur: 5, speaker: "KOMMANDO [FUNK]", text: "Das Nest. Es schläft nicht. Es wartet auf dich." },
      { cam: [8, 2, 10], look: [0, 2, 0], dur: 3.5, speaker: "DU", text: "Dann lassen wir es nicht warten." },
    ],
    debrief: "Das Nest ist gefallen. Doch seine letzte Botschaft war kein Schrei. Es war eine Koordinate.",
    events: [{ trigger: "kills", at: 8, speaker: "???", text: "D A S   N E S T   E R W A C H T.", shake: true }],
  },
};
function sRadio() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  [880, 660].forEach((f, i) => {
    const o = c.createOscillator(); const g = c.createGain();
    o.type = "square"; o.frequency.setValueAtTime(f, t + i * 0.09);
    g.gain.setValueAtTime(0.0001, t + i * 0.09); g.gain.exponentialRampToValueAtTime(0.045, t + i * 0.09 + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + i * 0.09 + 0.09);
    o.connect(g).connect(c.destination); o.start(t + i * 0.09); o.stop(t + i * 0.09 + 0.1);
  });
}
const STORY_KEY = "wirrwarr-story";
const FRAG_KEY = "wirrwarr-frags";
interface StoryData { done: string[]; flags: Record<string, boolean>; }
function loadStory(): StoryData {
  try { return JSON.parse(localStorage.getItem(STORY_KEY) ?? "null") ?? { done: [], flags: {} }; } catch { return { done: [], flags: {} }; }
}
function loadFrags(): number {
  try { return JSON.parse(localStorage.getItem(FRAG_KEY) ?? "0") || 0; } catch { return 0; }
}
const DAILY_KEY = "wirrwarr-daily";
function dayStr(): string { const d = new Date(); return `${d.getFullYear()}-${d.getMonth() + 1}-${d.getDate()}`; }
function weekIdx(): number { return Math.floor(Date.now() / (7 * 86400000)); }
const WEEKLY_EVENTS = [
  { id: "spore", name: "SPORENSTURM", desc: "3× Sporen in der Luft · Bots +10 % Tempo" },
  { id: "2xp", name: "DOPPEL-XP-WOCHE", desc: "Alle XP-Quellen ×2" },
  { id: "breach", name: "BREACH-PROTOKOLL", desc: "Strukturen halten nur die halbe Zeit" },
];
const weeklyEvent = () => WEEKLY_EVENTS[weekIdx() % WEEKLY_EVENTS.length];
interface DailyState { date: string; progress: Record<string, number>; claimed: Record<string, boolean>; }
function loadDaily(): DailyState {
  try {
    const d = JSON.parse(localStorage.getItem(DAILY_KEY) ?? "null") as DailyState | null;
    if (d && d.date === dayStr()) return d;
  } catch { /* */ }
  return { date: dayStr(), progress: {}, claimed: {} };
}
function saveDaily(d: DailyState) { try { localStorage.setItem(DAILY_KEY, JSON.stringify(d)); } catch { /* */ } }
export function updateDaily(metric: string, n = 1) {
  const d = loadDaily();
  d.progress[metric] = (d.progress[metric] ?? 0) + n;
  saveDaily(d);
}
const QUEST_POOL = [
  { id: "kills", metric: "kills", make: (s: number) => 8 + (s % 8), text: (n: number) => `Eliminiere ${n} Gegner`, xp: 250 },
  { id: "hs", metric: "headshots", make: (s: number) => 2 + (s % 3), text: (n: number) => `Lande ${n} Headshots`, xp: 300 },
  { id: "dest", metric: "destroyed", make: (s: number) => 2 + (s % 2), text: (n: number) => `Sprenge ${n} Strukturen`, xp: 250 },
  { id: "frag", metric: "frags", make: (s: number) => 2 + ((s >> 2) % 2), text: (n: number) => `Berge ${n} Lore-Fragmente`, xp: 300 },
  { id: "win", metric: "wins", make: () => 1, text: (n: number) => `Gewinne ${n} Mission`, xp: 400 },
];
function dailyQuests() {
  const s = Number(dayStr().replace(/-/g, "")) % 9973;
  const out: typeof QUEST_POOL[number][] = [];
  const seen = new Set<string>();
  for (const q of [...QUEST_POOL].sort((a, b) => ((s + a.id.length) % 5) - ((s + b.id.length) % 5))) {
    if (!seen.has(q.id)) { seen.add(q.id); out.push(q); }
    if (out.length === 3) break;
  }
  return out.map((q) => ({ ...q, n: q.make(s) }));
}
const BANTER: Record<string, { spk: string; text: string }[]> = {
  headshot: [
    { spk: "VEGA", text: "Notiert. Angeber." },
    { spk: "JUNO", text: "WOW. Nochmal. Sofort." },
  ],
  death: [
    { spk: "VEGA", text: "Respawn-Protokoll aktiv. Atmen, Soldat." },
    { spk: "JUNO", text: "Das hat mir auch wehgetan." },
  ],
  lowhp: [
    { spk: "VEGA", text: "Vitalwerte kritisch. Deckung. Jetzt." },
    { spk: "JUNO", text: "Bleib bei mir, verdammt!" },
  ],
  frag: [
    { spk: "JUNO", text: "Das Fragment singt. Hörst du es?" },
    { spk: "VEGA", text: "Datenpaket gesichert. Interessant …" },
  ],
  streak: [
    { spk: "VEGA", text: "Drei in Folge. Die KORP loggt deine Muster." },
    { spk: "JUNO", text: "Du bist heute ein Naturereignis!" },
  ],
  boss: [
    { spk: "JUNO", text: "Das Ding … ist groß. Wirklich groß." },
    { spk: "VEGA", text: "Panzerungsanalyse läuft. Schwachpunkte kommen." },
  ],
};
const DIFF_KEY = "wirrwarr-diff";
const DIFFS = [
  { id: "recruit", name: "REKRUT", mul: 0.7, desc: "Entspannt · Story genießen" },
  { id: "veteran", name: "VETERAN", mul: 1, desc: "Wie intended" },
  { id: "apex", name: "APEX", mul: 1.35, desc: "Bots: +35 % Schaden, schneller" },
];
function loadDiff() { try { return localStorage.getItem(DIFF_KEY) ?? "veteran"; } catch { return "veteran"; } }
const NG_KEY = "wirrwarr-ngplus";
const NG_MODS = [
  { id: "aggro", name: "Aggressive Biomass", desc: "Bots: +50 % Schaden, +20 % Tempo" },
  { id: "half", name: "Halber Schild", desc: "Schild startet bei 50 statt 100" },
  { id: "iron", name: "Eisen-Modus", desc: "Keine Schusswaffen. Nur Klinge & Streaks." },
];
function loadNG(): { mods: string[] } {
  try { return JSON.parse(localStorage.getItem(NG_KEY) ?? "null") ?? { mods: [] }; } catch { return { mods: [] }; }
}
const CODEX = [
  { at: 1, title: "KORP-Memo 004", text: "„Die Probe wächst auch ohne Licht. Lieferung wie bestellt. – K.“" },
  { at: 3, title: "Fracht-Manifest", text: "Container 77: ‚Saatgut‘. Empfänger: gelöscht. Absender: KORP Terraforming Div." },
  { at: 5, title: "Funkspruch (gebrochen)", text: "„…kein Unfall. Wiederhole: KEIN Unfall. Die Ernte war geplant…“" },
  { at: 7, title: "Dr. Marens Notiz", text: "„Sie ist nicht aggressiv. Sie ist verängstigt. Und sie verteidigt sich gegen UNS.“" },
  { at: 9, title: "KORP-Direktive 9", text: "„Alle Zeugen in Sektor 7 sind entbehrlich. Auch die eigenen.“" },
  { at: 11, title: "Die Koordinate", text: "Unter dem Nest: ein Terminal. Menschlichen Designs. Es sendet … nach oben." },
];
const WEAPON_LEVEL: Record<string, number> = { dorn: 1, brecher: 2, richter: 4 };
const ATTACHMENTS = [
  { id: "optic", name: "Zieloptik", level: 3, desc: "-30 % Bloom, -5 % Tempo" },
  { id: "grip", name: "Vertikalgriff", level: 4, desc: "-50 % Recoil" },
  { id: "leicht", name: "Leichter Lauf", level: 5, desc: "+8 % Tempo, +15 % Bloom" },
  { id: "schwer", name: "Schwerer Lauf", level: 6, desc: "+15 % Schaden, +30 % Recoil" },
];
interface UpgDef { id: string; path: string; tier: number; name: string; desc: string; cost: number; }
const UPG_DEFS: UpgDef[] = [
  { id: "s1", path: "sinne", tier: 1, name: "Schwarm-Sinn", desc: "Feinde als Glow durch Wände sichtbar", cost: 1 },
  { id: "s2", path: "sinne", tier: 2, name: "Echo-Blick", desc: "Treffer markieren Ziele 3 s rot", cost: 2 },
  { id: "s3", path: "sinne", tier: 3, name: "Raubtier-Markierung", desc: "Gegner unter 30 % HP permanent markiert", cost: 2 },
  { id: "c1", path: "carapax", tier: 1, name: "Dermale Platten", desc: "-10 % erlittener Schaden", cost: 1 },
  { id: "c2", path: "carapax", tier: 2, name: "Dornen-Reflex", desc: "Feinde in Melee-Reichweite bluten", cost: 2 },
  { id: "c3", path: "carapax", tier: 3, name: "Chitin-Overdrive", desc: "1x pro Match: Auto-Revive mit 50 %", cost: 2 },
  { id: "l1", path: "fort", tier: 1, name: "Wandläufer+", desc: "+10 % Sprint, schnelleres Mantling", cost: 1 },
  { id: "l2", path: "fort", tier: 2, name: "Mantis-Sprung", desc: "Höherer Sprung + Double-Jump", cost: 2 },
  { id: "l3", path: "fort", tier: 3, name: "Phasen-Gleiten", desc: "Slides dauern 50 % länger", cost: 2 },
];
const PERKS: { id: PerkId; name: string; desc: string }[] = [
  { id: "sprint", name: "Myzel-Sprint", desc: "+20 % Sprint-Tempo" },
  { id: "panzer", name: "Chitin-Panzer", desc: "-30 % erlittener Schaden" },
  { id: "sprung", name: "Mantis-Sprung", desc: "Doppelsprung" },
];

type VsMode = "tdm" | "ffa";
type GameKind = VsMode | "m0" | "m1" | "m2" | "m3" | "m4" | "m5" | "m6" | "m7" | "m8" | "m9" | "m10" | "m11" | "m12" | "range";
type ArenaId = "sektor" | "garten" | "stahl" | "orbital";

interface Mission {
  id: GameKind; title: string; briefing: string;
  type: "kills" | "destroy" | "survive";
  target: number; timeLimit?: number; botCount: number;
}

const MISSIONS: Mission[] = [
  { id: "m0", title: "Prolog // Kontakt", briefing: "Dein Dropship ist gefallen. Finde deine Waffe. Überlebe die erste Ernte.", type: "kills", target: 3, botCount: 3 },
  { id: "m5", title: "M5 // Die Koordinate", briefing: "Infiltration: 3 Terminals herunterladen, dann raus. Wer dich sieht, schlägt Alarm.", type: "destroy", target: 3, botCount: 6 },
  { id: "m6", title: "M6 // Defector", briefing: "Dr. Maren defectiert. Eskortiere sie lebend zur Extraktion. F = Befehl (Folgen/Waiten).", type: "kills", target: 999, botCount: 6 },
  { id: "m7", title: "M7 // Zwei Fronten", briefing: "KORP-Mechs UND Biomass. Wirf Lockdrocks (H) und lass sie einander zerfleischen. 10 Kills.", type: "kills", target: 10, botCount: 8 },
  { id: "m8", title: "M8 // Das Labor brennt", briefing: "Das Labor brennt. Kapsel UND Datenkern vorn. Du kannst nur eines tragen. Wähle.", type: "kills", target: 999, botCount: 5 },
  { id: "m9", title: "M9 // Spiegelbild", briefing: "KADE hat dein Loadout. Deine Streaks. Deine Waffen. Töte dein Spiegelbild.", type: "kills", target: 999, botCount: 1 },
  { id: "m10", title: "M10 // Der Garten", briefing: "Kein Funk. Keine Gegner. Nur das, was sie hier gepflanzt haben. Sammle die 4 Erinnerungen. Dann geh.", type: "kills", target: 999, botCount: 0 },
  { id: "m11", title: "M11 // Belagerung", briefing: "Der ERNTE-LÄUFER hat einen Schild. Drei Pylone speisen ihn. Spreng sie – dann kill ihn.", type: "kills", target: 999, botCount: 4 },
  { id: "m12", title: "M12 // DER GÄRTNER", briefing: "Finale. Die KI im Nest. Drei Phasen. Die Arena stirbt mit ihr.", type: "kills", target: 999, botCount: 1 },
  { id: "m1", title: "M1 // Erste Ernte", briefing: "Die Biomass testet dich. Eliminiere 8 Eindringlinge – sie kommen immer wieder.", type: "kills", target: 8, botCount: 4 },
  { id: "m2", title: "M2 // Abrissunternehmen", briefing: "Sprenge 6 sprengbare Strukturen in 3 Minuten. Der BRECHER-7 ist dein bester Freund.", type: "destroy", target: 6, timeLimit: 180, botCount: 4 },
  { id: "m3", title: "M3 // Stellung halten", briefing: "Halte 120 Sekunden gegen endlose Wellen. Niemand kommt zu dir durch. Niemand.", type: "survive", target: 120, botCount: 6 },
  { id: "m4", title: "M4 // Das Nest", briefing: "Das Finale: 15 Eindringlinge zwischen dir und dem Nest. Brenn es nieder.", type: "kills", target: 15, botCount: 6 },
];

const CAMP_KEY = "wirrwarr-campaign-done";
const STAT_KEY = "wirrwarr-stats";
function loadStats(): { kills: number; headshots: number; melees: number; bestStreak: number } {
  try { return JSON.parse(localStorage.getItem(STAT_KEY) ?? "null") ?? { kills: 0, headshots: 0, melees: 0, bestStreak: 0 }; }
  catch { return { kills: 0, headshots: 0, melees: 0, bestStreak: 0 }; }
}
const NAME_KEY = "wirrwarr-callsign";
export function loadCallsign(): string {
  try { return localStorage.getItem(NAME_KEY) || ""; } catch { return ""; }
}
const RANGE_KEY = "wirrwarr-rangestats";
function loadRange(): { shots: number; hits: number; hs: number; bestAcc: number; sessions: number } {
  try { return JSON.parse(localStorage.getItem(RANGE_KEY) ?? "null") ?? { shots: 0, hits: 0, hs: 0, bestAcc: 0, sessions: 0 }; } catch { return { shots: 0, hits: 0, hs: 0, bestAcc: 0, sessions: 0 }; }
}
function sAnnounce() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  [523, 784, 1046].forEach((f, i) => {
    const o = c.createOscillator(); const g = c.createGain();
    o.type = "square"; o.frequency.setValueAtTime(f, t + i * 0.07);
    g.gain.setValueAtTime(0.0001, t + i * 0.07); g.gain.exponentialRampToValueAtTime(0.07, t + i * 0.07 + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + i * 0.07 + 0.18);
    o.connect(g).connect(c.destination); o.start(t + i * 0.07); o.stop(t + i * 0.07 + 0.2);
  });
}
function loadCampaign(): string[] {
  try { return JSON.parse(localStorage.getItem(CAMP_KEY) ?? "[]") as string[]; } catch { return []; }
}

interface WallBox {
  mesh: THREE.Mesh;
  hw: number;
  hd: number;
  hh: number;
  top: number;
  hp: number;
  maxHp: number;
  destructible: boolean;
  active: boolean;
}

interface BotEnt {
  id: number;
  name: string;
  team: number;
  group: THREE.Group;
  body: THREE.Mesh;
  hp: number;
  alive: boolean;
  respawnAt: number;
  cd: number;
  kills: number;
  strafeDir: number;
  strafeT: number;
  color: THREE.Color;
  y: number;
  vy: number;
  mantle: number | null;
  burstLeft: number;
  reactT: number;
  pauseT: number;
  ghost: THREE.Mesh;
  markedT: number;
  flankT: number;
  flankX: number;
  flankZ: number;
  shieldT: number;
  isBoss: boolean;
  bossHp: number;
  shield: boolean;
  phase: number;
  summonT: number;
  decayT: number;
}

interface Particle {
  mesh: THREE.Mesh;
  vel: THREE.Vector3;
  life: number;
}

interface Tracer {
  line: THREE.Line;
  life: number;
}

const BOT_NAMES = ["VEGA", "JUNO", "RAZOR", "MORBID", "ECHO"];
const TEAM_HEX = [0x22ff55, 0xff5544, 0xffcc33, 0x33ccff, 0xff66cc, 0x99ff33];
const ARENA = 48;

/* ---------------- Audio (reuse Pattern) ---------------- */
let actx: AudioContext | null = null;
function ac(): AudioContext | null {
  if (typeof window === "undefined") return null;
  const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
  if (!AC) return null;
  if (!actx) actx = new AC();
  if (actx.state === "suspended") void actx.resume();
  return actx;
}
function sShot(kind: "dorn" | "brecher" | "richter") {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  if (kind === "richter") {
    o.type = "square"; o.frequency.setValueAtTime(220, t); o.frequency.exponentialRampToValueAtTime(60, t + 0.16);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.26, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.2);
  } else if (kind === "brecher") {
    o.type = "square"; o.frequency.setValueAtTime(120, t); o.frequency.exponentialRampToValueAtTime(32, t + 0.2);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.32, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.24);
  } else {
    o.type = "triangle"; o.frequency.setValueAtTime(700, t); o.frequency.exponentialRampToValueAtTime(170, t + 0.07);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.15, t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.09);
  }
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.3);
}
function sBoom() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const len = Math.floor(c.sampleRate * 0.6);
  const buf = c.createBuffer(1, len, c.sampleRate);
  const d = buf.getChannelData(0);
  for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / len);
  const src = c.createBufferSource(); src.buffer = buf;
  const f = c.createBiquadFilter(); f.type = "lowpass"; f.frequency.setValueAtTime(800, t); f.frequency.exponentialRampToValueAtTime(60, t + 0.6);
  const g = c.createGain(); g.gain.setValueAtTime(0.55, t); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.6);
  src.connect(f).connect(g).connect(c.destination); src.start(t);
}
function sStepPan(dist: number, pan: number) {
  const c = ac(); if (!c) return;
  const vol = Math.max(0, 1 - dist / 14) * 0.09;
  if (vol <= 0.005) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "triangle"; o.frequency.setValueAtTime(80 + Math.random() * 20, t); o.frequency.exponentialRampToValueAtTime(50, t + 0.06);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(vol, t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.07);
  let out: AudioNode = g;
  try { const pn = c.createStereoPanner(); pn.pan.value = Math.max(-1, Math.min(1, pan)); g.connect(pn); out = pn; } catch { /* */ }
  out.connect(c.destination);
  o.connect(g); o.start(t); o.stop(t + 0.08);
}
function sHeart() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  [0, 0.18].forEach((off) => {
    const o = c.createOscillator(); const g = c.createGain();
    o.type = "sine"; o.frequency.setValueAtTime(60, t + off); o.frequency.exponentialRampToValueAtTime(40, t + off + 0.1);
    g.gain.setValueAtTime(0.0001, t + off); g.gain.exponentialRampToValueAtTime(0.16, t + off + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + off + 0.14);
    o.connect(g).connect(c.destination); o.start(t + off); o.stop(t + off + 0.16);
  });
}
function sHit() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(1200, t); o.frequency.exponentialRampToValueAtTime(600, t + 0.05);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.07, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.06);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.07);
}
function sStep() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "triangle"; o.frequency.setValueAtTime(95, t); o.frequency.exponentialRampToValueAtTime(55, t + 0.05);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.05, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.06);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.07);
}
function sShieldHit() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "sawtooth"; o.frequency.setValueAtTime(900, t); o.frequency.exponentialRampToValueAtTime(300, t + 0.08);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.09, t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.1);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.11);
}
function sHurt() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(140, t); o.frequency.exponentialRampToValueAtTime(60, t + 0.12);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.22, t + 0.008); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.15);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.16);
}
function sHeadshot() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "square"; o.frequency.setValueAtTime(1600, t); o.frequency.exponentialRampToValueAtTime(2400, t + 0.06);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.1, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.09);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.1);
}
function sMelee() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "triangle"; o.frequency.setValueAtTime(220, t); o.frequency.exponentialRampToValueAtTime(70, t + 0.09);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.25, t + 0.006); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.11);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.12);
}
function sPickup() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  [660, 990].forEach((f, i) => {
    const o = c.createOscillator(); const g = c.createGain();
    o.type = "sine"; o.frequency.setValueAtTime(f, t + i * 0.07);
    g.gain.setValueAtTime(0.0001, t + i * 0.07); g.gain.exponentialRampToValueAtTime(0.09, t + i * 0.07 + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + i * 0.07 + 0.12);
    o.connect(g).connect(c.destination); o.start(t + i * 0.07); o.stop(t + i * 0.07 + 0.13);
  });
}
function sThrow() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(400, t); o.frequency.exponentialRampToValueAtTime(700, t + 0.08);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.07, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.1);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.11);
}
function sLand() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(120, t); o.frequency.exponentialRampToValueAtTime(40, t + 0.12);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.2, t + 0.008); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.14);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.15);
}

/* ================================================================== */

const REPLAY_WALLS: [number, number, number, number, number, number][] = [
  [0, 2, -40, 80, 4, 1], [0, 2, 40, 80, 4, 1], [-40, 2, 0, 1, 4, 80], [40, 2, 0, 1, 4, 80],
  [0, 1.5, 0, 6, 3, 4], [-14, 1.5, -10, 8, 3, 1.2], [14, 1.5, 10, 8, 3, 1.2], [-14, 1.5, 12, 1.2, 3, 8],
  [14, 1.5, -12, 1.2, 3, 8], [-30, 2, 0, 2, 4, 10], [30, 2, 0, 2, 4, 10], [0, 2, -26, 10, 4, 2], [0, 2, 26, 10, 4, 2],
  [-8, 1, 8, 2, 2, 2], [8, 1, -8, 2, 2, 2], [-24, 1, -24, 3, 2, 3], [24, 1, 24, 3, 2, 3], [24, 1, -24, 3, 2, 3], [-24, 1, 24, 3, 2, 3],
];

function ReplayView({ data, onExit }: { data: { mode: string; frames: number[][]; events: { t: number; e: string }[] }; onExit: () => void }) {
  const mountRef = useRef<HTMLDivElement>(null);
  const [speed, setSpeed] = useState(1);
  const speedRef = useRef(1);
  speedRef.current = speed;
  const [ui, setUi] = useState({ t: 0, feed: [] as string[], done: false });

  useEffect(() => {
    const mount = mountRef.current;
    if (!mount) return;
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(mount.clientWidth, mount.clientHeight);
    mount.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x050705);
    scene.fog = new THREE.FogExp2(0x050705, 0.028);
    const camera = new THREE.PerspectiveCamera(75, mount.clientWidth / mount.clientHeight, 0.1, 300);
    scene.add(new THREE.HemisphereLight(0x22ff55, 0x000000, 0.5));
    const ground = new THREE.Mesh(new THREE.PlaneGeometry(160, 160), new THREE.MeshStandardMaterial({ color: 0x0a0f0a }));
    ground.rotation.x = -Math.PI / 2;
    scene.add(ground);

    for (const [x, y, z, w, h, d] of REPLAY_WALLS) {
      const msh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), new THREE.MeshStandardMaterial({ color: 0x2f7a3a, emissive: 0x0a2a10 }));
      msh.position.set(x, y, z);
      scene.add(msh);
    }
    let raf = 0;
    let t = 0;
    let last = performance.now();
    let ptr = 0;
    let evIdx = 0;
    let uiAcc = 0;
    const feedLocal: string[] = [];
    const fr = data.frames;
    const loop = () => {
      raf = requestAnimationFrame(loop);
      const now = performance.now();
      t += ((now - last) / 1000) * speedRef.current;
      last = now;
      if (fr.length > 1) {
        while (ptr < fr.length - 2 && fr[ptr + 1][0] <= t) ptr++;
        const a = fr[ptr];
        const b = fr[Math.min(ptr + 1, fr.length - 1)];
        const f = b[0] > a[0] ? Math.max(0, Math.min(1, (t - a[0]) / (b[0] - a[0]))) : 0;
        camera.position.set(a[1] + (b[1] - a[1]) * f, 1.7, a[2] + (b[2] - a[2]) * f);
        camera.rotation.set(0, a[3] + (b[3] - a[3]) * f, 0, "YXZ");
      }
      while (evIdx < data.events.length && data.events[evIdx].t <= t) {
        feedLocal.push(`[${data.events[evIdx].t}s] ${data.events[evIdx].e}`);
        evIdx++;
      }
      uiAcc += 0.25;
      if (uiAcc >= 0.25) {
        uiAcc = 0;
        setUi({ t, feed: feedLocal.slice(-5), done: t > (fr[fr.length - 1]?.[0] ?? 0) });
      }
      renderer.render(scene, camera);
    };
    loop();
    return () => { cancelAnimationFrame(raf); renderer.dispose(); if (renderer.domElement.parentElement === mount) mount.removeChild(renderer.domElement); };
  }, [data]);

  return (
    <div className="relative h-screen w-full bg-black overflow-hidden">
      <div ref={mountRef} className="w-full h-full" />
      <div className="absolute inset-x-0 top-0 h-[9%] bg-black pointer-events-none" />
      <div className="absolute inset-x-0 bottom-0 h-[9%] bg-black pointer-events-none" />
      <p className="absolute top-[10%] left-4 font-mono text-xs text-destructive tracking-[0.3em] uppercase animate-pulse-neon">● REPLAY // {data.mode} // {ui.t.toFixed(1)} s</p>
      <div className="absolute top-[10%] right-4 flex gap-2">
        {[1, 2, 4].map((s) => (
          <button key={s} type="button" onClick={() => setSpeed(s)} className={`font-mono text-[10px] px-2 py-1 rounded-sm border min-h-[28px] ${speed === s ? "border-primary text-primary" : "border-border text-muted-foreground"}`}>{s}×</button>
        ))}
        <button type="button" onClick={onExit} className="font-mono text-[10px] px-2 py-1 rounded-sm border border-border text-muted-foreground min-h-[28px]">✕ EXIT</button>
      </div>
      <div className="absolute bottom-[10%] left-4 space-y-0.5 pointer-events-none">
        {ui.feed.map((f, i) => (<p key={i} className="font-mono text-[10px] text-primary/90">{f}</p>))}
      </div>
      {ui.done && <p className="absolute inset-x-0 top-1/2 text-center font-mono text-xl text-primary glow-neon tracking-[0.3em] uppercase">REPLAY ENDE</p>}
    </div>
  );
}

export function RealGame() {
  const mountRef = useRef<HTMLDivElement>(null);
  const [screen, setScreen] = useState<"menu" | "game" | "end" | "replay">("menu");
  const [replayData, setReplayData] = useState<null | { mode: string; frames: number[][]; events: { t: number; e: string }[] }>(null);
  const [winner, setWinner] = useState("");
  const [arena, setArena] = useState<ArenaId>("sektor");
  const [perk, setPerk] = useState<PerkId>("sprint");
  const [quality, setQuality] = useState<"low" | "med" | "high">("med");
  const [profile, setProfile] = useState(() => loadProfile());
  const [loadout, setLoadout] = useState(() => loadLoadout());
  const level = levelFromXp(profile.xp);
  const feedExtraRef = useRef<string[]>([]);
  const endInfoExt = useRef<null | { debrief?: string; medals: string[]; kills: number; deaths: number; hs: number; frags?: number; rank?: string }>(null);
  const chooseM8Ref = useRef<(v: "vega" | "data") => void>(() => {});
  const replayRef = useRef<null | { mode: string; frames: number[][]; events: { t: number; e: string }[]; date: string }>(null);
  const [ngMods, setNgMods] = useState<string[]>(() => loadNG().mods);
  const [, setDailyTick] = useState(0);
  const [callsign, setCallsign] = useState(() => loadCallsign());
  const [introDone, setIntroDone] = useState(() => { try { return !!localStorage.getItem("wirrwarr-intro"); } catch { return false; } });
  const [introBeat, setIntroBeat] = useState(0);
  const [doneMissions, setDoneMissions] = useState<string[]>(() => { try { return JSON.parse(localStorage.getItem(STORY_KEY) ?? "null")?.done ?? []; } catch { return []; } });
  const [failed, setFailed] = useState(false);
  useEffect(() => {
    if (screen === "menu" || screen === "end") setDoneMissions(loadStory().done);
  }, [screen]);
  const [hud, setHud] = useState({
    hp: 100, ammo: 24, reloading: false, weapon: "DORN",
    scores: "", feed: [] as string[], kills: 0, objective: "", tactics: false,
    shield: 100, bloom: 0, grenades: 2, dmgDirs: [] as { a: number; age: number }[],
    codes: 0, upg: [] as string[], bioOpen: false, announce: null as string | null, skin: "#22ff55",
    fps: 60, killcam: null as string | null, subtitle: null as { speaker: string; text: string } | null,
    m8Offer: null as "vega" | "data" | null, marenHp: -1, lowhp: false,
  });
  const apiRef = useRef<{ dispose: () => void; upgrade?: (id: string) => void } | null>(null);
  const feedRef = useRef<string[]>([]);

  useEffect(() => () => apiRef.current?.dispose(), []);

  const start = (kind: GameKind) => {
    setFailed(false);
    setScreen("game");
    requestAnimationFrame(() =>
      initEngine(kind, arena, perk, loadout, level, (xpGain: number, msg?: string) => {
        if (msg) feedExtraRef.current.push(msg);
        setProfile((pr) => {
          const np = { xp: pr.xp + xpGain };
          try { localStorage.setItem(PROF_KEY, JSON.stringify(np)); } catch { /* */ }
          const oldLv = levelFromXp(pr.xp);
          const newLv = levelFromXp(np.xp);
          if (newLv > oldLv) feedExtraRef.current.push(`⬆ LEVEL UP! Level ${newLv} – neue Unlocks im Menü!`);
          return np;
        });
      }, quality)
    );
  };

  const initEngine = (
    mode: GameKind, arenaId: ArenaId, perk: PerkId,
    loadout: LoadoutData, level: number,
    addXp: (n: number, msg?: string) => void,
    quality: "low" | "med" | "high" = "med"
  ) => {
    const mission = MISSIONS.find((m) => m.id === mode) ?? null;
    const xpMul = weeklyEvent().id === "2xp" ? 2 : 1;
    const diffMul = DIFFS.find((d) => d.id === loadDiff())?.mul ?? 1;
    const breachMul = weeklyEvent().id === "breach" ? 2 : 1;
    const mount = mountRef.current;
    if (!mount) return;

    /* ---------- Renderer / Scene / Camera ---------- */
    const renderer = new THREE.WebGLRenderer({ antialias: quality !== "low", powerPreference: "high-performance" });
    renderer.setPixelRatio(quality === "low" ? 1 : quality === "med" ? Math.min(window.devicePixelRatio, 1.5) : Math.min(window.devicePixelRatio, 2));
    const particleCap = quality === "low" ? 60 : quality === "med" ? 150 : 250;
    renderer.setSize(mount.clientWidth, mount.clientHeight);
    renderer.shadowMap.enabled = quality === "high";
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    mount.appendChild(renderer.domElement);

    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x050705);
    scene.fog = new THREE.FogExp2(0x050705, 0.028);

    const camera = new THREE.PerspectiveCamera(75, mount.clientWidth / mount.clientHeight, 0.1, 200);
    const yaw = new THREE.Object3D();
    const pitch = new THREE.Object3D();
    yaw.add(pitch);
    pitch.add(camera);
    scene.add(yaw);

    /* ---------- Licht ---------- */
    scene.add(new THREE.HemisphereLight(0x22ff55, 0x000000, 0.5));
    const dir = new THREE.DirectionalLight(0x88ffaa, 0.7);
    dir.position.set(20, 40, 10);
    if (quality === "high") {
      dir.castShadow = true;
      dir.shadow.mapSize.set(1024, 1024);
      dir.shadow.camera.left = -50; dir.shadow.camera.right = 50;
      dir.shadow.camera.top = 50; dir.shadow.camera.bottom = -50;
    }
    scene.add(dir);

    /* ---------- Boden ---------- */
    const ground = new THREE.Mesh(
      new THREE.PlaneGeometry(ARENA * 2, ARENA * 2),
      new THREE.MeshStandardMaterial({ color: 0x0a0f0a, roughness: 1 })
    );
    ground.rotation.x = -Math.PI / 2;
    scene.add(ground);
    // ---- Knaller-Optik: Himmel, Sterne, Planet, Sporen, Neon ----
    const sky = new THREE.Mesh(
      new THREE.SphereGeometry(160, 24, 16),
      new THREE.ShaderMaterial({
        side: THREE.BackSide,
        uniforms: { top: { value: new THREE.Color(0x010402) }, bot: { value: new THREE.Color(0x0a3315) } },
        vertexShader: "varying vec3 vP; void main(){ vP = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }",
        fragmentShader: "varying vec3 vP; uniform vec3 top; uniform vec3 bot; void main(){ float h = normalize(vP).y * 0.5 + 0.5; gl_FragColor = vec4(mix(bot, top, pow(h, 0.55)), 1.0); }",
      })
    );
    scene.add(sky);
    const starGeo = new THREE.BufferGeometry();
    const starPos = new Float32Array(300 * 3);
    for (let i = 0; i < 300; i++) {
      const a = Math.random() * Math.PI * 2, e = Math.random() * Math.PI * 0.45 + 0.08, r = 150;
      starPos[i * 3] = Math.cos(a) * Math.cos(e) * r;
      starPos[i * 3 + 1] = Math.sin(e) * r;
      starPos[i * 3 + 2] = Math.sin(a) * Math.cos(e) * r;
    }
    starGeo.setAttribute("position", new THREE.BufferAttribute(starPos, 3));
    scene.add(new THREE.Points(starGeo, new THREE.PointsMaterial({ color: 0x99ffcc, size: 0.7, transparent: true, opacity: 0.8 })));
    const planet = new THREE.Mesh(new THREE.SphereGeometry(14, 24, 24), new THREE.MeshStandardMaterial({ color: 0x0c2a12, emissive: 0x1f8f3a, emissiveIntensity: 0.55, roughness: 1 }));
    planet.position.set(-70, 55, -90);
    scene.add(planet);
    const ring = new THREE.Mesh(new THREE.RingGeometry(18, 24, 48), new THREE.MeshBasicMaterial({ color: 0x22ff55, transparent: true, opacity: 0.22, side: THREE.DoubleSide }));
    ring.position.copy(planet.position);
    ring.rotation.x = 1.2;
    scene.add(ring);
    const sporeCount = (quality === "low" ? 60 : 160) * (weeklyEvent().id === "spore" ? 3 : 1);
    const sporeGeo = new THREE.BufferGeometry();
    const sporePos = new Float32Array(sporeCount * 3);
    for (let i = 0; i < sporeCount; i++) {
      sporePos[i * 3] = (Math.random() - 0.5) * 80;
      sporePos[i * 3 + 1] = Math.random() * 8;
      sporePos[i * 3 + 2] = (Math.random() - 0.5) * 80;
    }
    sporeGeo.setAttribute("position", new THREE.BufferAttribute(sporePos, 3));
    scene.add(new THREE.Points(sporeGeo, new THREE.PointsMaterial({ color: 0x66ff88, size: 0.09, transparent: true, opacity: 0.55 })));
    const neonMat = new THREE.MeshBasicMaterial({ color: 0x22ff55 });
    [
      [0, -39.4, 80, 0.12], [0, 39.4, 80, 0.12], [-39.4, 0, 0.12, 80], [39.4, 0, 0.12, 80],
    ].forEach(([x, z, w, d]) => {
      const m2 = new THREE.Mesh(new THREE.BoxGeometry(w, 0.12, d), neonMat);
      m2.position.set(x, 0.35, z);
      scene.add(m2);
    });
    scene.add(new THREE.GridHelper(80, 40, 0x1a4d24, 0x10240f));
    const grid = new THREE.GridHelper(ARENA * 2, 48, 0x1a4d24, 0x10240f);
    scene.add(grid);

    /* ---------- Walls ---------- */
    const walls: WallBox[] = [];
    const wallGeoCache = new Map<string, THREE.BoxGeometry>();
    const getGeo = (w: number, h: number, d: number) => {
      const k = `${w}|${h}|${d}`;
      let g = wallGeoCache.get(k);
      if (!g) { g = new THREE.BoxGeometry(w, h, d); wallGeoCache.set(k, g); }
      return g;
    };
    const addWall = (x: number, y: number, z: number, w: number, h: number, d: number, destructible: boolean, hp = 150) => {
      const mat = new THREE.MeshStandardMaterial({
        color: destructible ? 0x2f7a3a : 0x1c2a1c,
        roughness: 0.9,
        emissive: destructible ? 0x0a2a10 : 0x000000,
      });
      const mesh = new THREE.Mesh(getGeo(w, h, d), mat);
      mesh.position.set(x, y, z);
      scene.add(mesh);
      walls.push({ mesh, hw: w / 2, hd: d / 2, hh: h / 2, top: y + h / 2, hp, maxHp: hp, destructible, active: true });
    };
    // Außenwände (massiv)
    addWall(0, 2, -ARENA, ARENA * 2, 4, 1, false);
    addWall(0, 2, ARENA, ARENA * 2, 4, 1, false);
    addWall(-ARENA, 2, 0, 1, 4, ARENA * 2, false);
    addWall(ARENA, 2, 0, 1, 4, ARENA * 2, false);
    // Deckungen & sprengbare Strukturen (pro Arena)
    const structs: [number, number, number, number, number, number, boolean][] =
      arenaId === "sektor"
        ? [
            [0, 1.5, 0, 6, 3, 4, true],      // Zentralblock
            [-14, 1.5, -10, 8, 3, 1.2, true],
            [14, 1.5, 10, 8, 3, 1.2, true],
            [-14, 1.5, 12, 1.2, 3, 8, true],
            [14, 1.5, -12, 1.2, 3, 8, true],
            [-30, 2, 0, 2, 4, 10, false],
            [30, 2, 0, 2, 4, 10, false],
            [0, 2, -26, 10, 4, 2, false],
            [0, 2, 26, 10, 4, 2, false],
            [-8, 1, 8, 2, 2, 2, true],
            [8, 1, -8, 2, 2, 2, true],
            [-24, 1, -24, 3, 2, 3, true],
            [24, 1, 24, 3, 2, 3, true],
            [24, 1, -24, 3, 2, 3, true],
            [-24, 1, 24, 3, 2, 3, true],
          ]
        : arenaId === "garten"
          ? [
            // Biomass-Garten: Säulenring + zentrale Kuppel
            [0, 1.5, 0, 4, 3, 4, true],
            [-10, 1, -10, 2, 2, 2, true],
            [10, 1, -10, 2, 2, 2, true],
            [-10, 1, 10, 2, 2, 2, true],
            [10, 1, 10, 2, 2, 2, true],
            [-20, 1.5, 0, 1.5, 3, 6, true],
            [20, 1.5, 0, 1.5, 3, 6, true],
            [0, 1.5, -20, 6, 3, 1.5, true],
            [0, 1.5, 20, 6, 3, 1.5, true],
            [-16, 1, 0, 2, 2, 2, true],
            [16, 1, 0, 2, 2, 2, true],
            [0, 1, -14, 2, 2, 2, true],
            [0, 1, 14, 2, 2, 2, true],
            [-30, 2, 0, 2, 4, 10, false],
            [30, 2, 0, 2, 4, 10, false],
            [0, 2, -26, 10, 4, 2, false],
            [0, 2, 26, 10, 4, 2, false],
          ]
        : arenaId === "stahl"
          ? [
            // Stahlwiege: Fabrikhallen, dünne sprengbare Doppelwände = Brecher-Paradies
            [-10, 1.5, -12, 1.2, 3, 10, true], [10, 1.5, -12, 1.2, 3, 10, true],
            [-10, 1.5, 12, 1.2, 3, 10, true], [10, 1.5, 12, 1.2, 3, 10, true],
            [-22, 1.5, 0, 8, 3, 1.2, true], [22, 1.5, 0, 8, 3, 1.2, true],
            [0, 1, -6, 6, 2, 1.2, true], [0, 1, 6, 6, 2, 1.2, true],
            [0, 1.5, 0, 4, 3, 4, true],
            [-28, 2, -28, 3, 4, 3, false], [28, 2, 28, 3, 4, 3, false],
            [28, 2, -28, 3, 4, 3, false], [-28, 2, 28, 3, 4, 3, false],
          ]
        : [
            // Orbitaldock: Containerzeilen, lange Lanes, viele Mantle-Kanten
            [-8, 1, -8, 6, 1.1, 2.5, true], [8, 1, 8, 6, 1.1, 2.5, true],
            [-8, 1, 8, 6, 1.1, 2.5, true], [8, 1, -8, 6, 1.1, 2.5, true],
            [0, 1.5, -16, 8, 3, 1.2, true], [0, 1.5, 16, 8, 3, 1.2, true],
            [-16, 1.5, 0, 1.2, 3, 8, true], [16, 1.5, 0, 1.2, 3, 8, true],
            [-24, 1, -24, 3, 2, 3, true], [24, 1, 24, 3, 2, 3, true],
            [24, 1, -24, 3, 2, 3, true], [-24, 1, 24, 3, 2, 3, true],
            [0, 2.5, 0, 3, 5, 3, false],
          ];
    for (const [x, y, z, w, h, d, des] of structs) addWall(x, y, z, w, h, d, des);

    /* ---------- Partikel / Tracer Pools ---------- */
    const particles: Particle[] = [];
    const tracers: Tracer[] = [];
    const debrisGeo = new THREE.BoxGeometry(0.15, 0.15, 0.15);
    const burst = (pos: THREE.Vector3, color: number, n: number) => {
      for (let i = 0; i < n; i++) {
        const m = new THREE.Mesh(debrisGeo, new THREE.MeshBasicMaterial({ color }));
        m.position.copy(pos);
        scene.add(m);
        particles.push({
          mesh: m,
          vel: new THREE.Vector3((Math.random() - 0.5) * 8, Math.random() * 7, (Math.random() - 0.5) * 8),
          life: 0.5 + Math.random() * 0.5,
        });
      }
      if (particles.length > particleCap) {
        const rm = particles.splice(0, particles.length - particleCap);
        rm.forEach((p) => scene.remove(p.mesh));
      }
    };
    const tracer = (from: THREE.Vector3, to: THREE.Vector3, color: number) => {
      const geo = new THREE.BufferGeometry().setFromPoints([from, to]);
      const line = new THREE.Line(geo, new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.9 }));
      scene.add(line);
      tracers.push({ line, life: 0.08 });
    };

    /* ---------- Kollision ---------- */
    const collides = (x: number, z: number, r: number, y: number) => {
      for (const w of walls) {
        if (!w.active) continue;
        if (w.top <= y + 0.5) continue; // drüber -> keine horizontale Blockade
        const p = w.mesh.position;
        const hw = w.hw + r, hd = w.hd + r;
        if (x > p.x - hw && x < p.x + hw && z > p.z - hd && z < p.z + hd) return true;
      }
      return false;
    };
    const moveWithCollide = (pos: { x: number; z: number }, dx: number, dz: number, r: number, y: number) => {
      if (!collides(pos.x + dx, pos.z, r, y)) pos.x += dx;
      if (!collides(pos.x, pos.z + dz, r, y)) pos.z += dz;
    };
    const getSupport = (x: number, z: number, fromY: number) => {
      let g = 0;
      for (const w of walls) {
        if (!w.active) continue;
        const p = w.mesh.position;
        if (x > p.x - w.hw - 0.4 && x < p.x + w.hw + 0.4 && z > p.z - w.hd - 0.4 && z < p.z + w.hd + 0.4) {
          if (w.top <= fromY + 0.001 && w.top > g) g = w.top;
        }
      }
      return g;
    };

    /* ---------- Bots ---------- */
    const bots: BotEnt[] = [];
    const SPAWNS: [number, number][] = [[-40, 0], [40, 0], [-40, -40], [40, 40], [0, -40], [0, 40]];
    const makeNameTag = (text: string, color: THREE.Color) => {
      const cv = document.createElement("canvas");
      cv.width = 128; cv.height = 32;
      const c = cv.getContext("2d")!;
      c.font = "bold 20px monospace"; c.textAlign = "center";
      c.fillStyle = `#${color.getHexString()}`;
      c.fillText(text.slice(0, 10), 64, 22);
      const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), transparent: true }));
      sp.scale.set(2.2, 0.55, 1);
      sp.position.y = 2.3;
      return sp;
    };
    const makeBot = (i: number, team: number) => {
      const group = new THREE.Group();
      const color = new THREE.Color(TEAM_HEX[mode === "ffa" ? i + 1 : team]);
      const body = new THREE.Mesh(
        new THREE.BoxGeometry(0.7, 1.5, 0.4),
        new THREE.MeshStandardMaterial({ color, roughness: 0.6, emissive: color.clone().multiplyScalar(0.25) })
      );
      body.position.y = 0.75;
      const head = new THREE.Mesh(
        new THREE.BoxGeometry(0.4, 0.4, 0.4),
        new THREE.MeshStandardMaterial({ color: 0x111111, emissive: color.clone().multiplyScalar(0.6) })
      );
      head.position.y = 1.75;
      const ghost = new THREE.Mesh(
        new THREE.BoxGeometry(0.8, 1.9, 0.5),
        new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.18, depthTest: false })
      );
      ghost.position.y = 0.95;
      ghost.visible = false;
      group.add(body, head, ghost, makeNameTag(BOT_NAMES[i], color));
      const sp = SPAWNS[(i + 1) % SPAWNS.length];
      group.position.set(sp[0], 0, sp[1]);
      scene.add(group);
      bots.push({
        id: i + 1, name: BOT_NAMES[i], team, group, body,
        hp: 100, alive: true, respawnAt: 0, cd: 1 + Math.random(),
        kills: 0, strafeDir: 1, strafeT: 0, color,
        y: 0, vy: 0, mantle: null,
        burstLeft: 0, reactT: 0, pauseT: 0, ghost, markedT: 0,
        flankT: 0, flankX: 0, flankZ: 0, shieldT: 0,
        isBoss: false, bossHp: 0, shield: false, phase: 1, summonT: 20, decayT: 10,
      });
    };
    const bossModes: Record<string, string> = { m9: "KADE", m11: "ERNTE-LÄUFER", m12: "DER GÄRTNER" };
    const pylonWalls: WallBox[] = [];
    const botCount = mission ? mission.botCount : 5;
    for (let i = 0; i < botCount; i++) {
      makeBot(i, mission ? 1 : i % 2 === 0 ? 1 : 0);
      if (mode === "m7") {
        const b = bots[bots.length - 1];
        if (i % 2 === 1) {
          b.team = 2;
          (b.body.material as THREE.MeshStandardMaterial).color.setHex(0x8899aa);
          (b.body.material as THREE.MeshStandardMaterial).emissive.setHex(0x334455);
          b.name = `MECH-${i}`;
        } else {
          b.name = `SPORE-${i}`;
        }
      }
      if (i === 0 && bossModes[mode]) {
        const b = bots[0];
        b.isBoss = true;
        b.name = bossModes[mode];
        if (mode === "m9") {
          b.bossHp = 300; b.hp = 300;
          (b.body.material as THREE.MeshStandardMaterial).color.setHex(0xffffff);
          (b.body.material as THREE.MeshStandardMaterial).emissive.setHex(0x888888);
        }
        if (mode === "m11") {
          b.bossHp = 600; b.hp = 600; b.shield = true;
          b.group.scale.set(2, 2, 2);
          (b.body.material as THREE.MeshStandardMaterial).color.setHex(0x66ff66);
        }
        if (mode === "m12") {
          b.bossHp = 900; b.hp = 900;
          b.group.scale.set(2.5, 2.5, 2.5);
          (b.body.material as THREE.MeshStandardMaterial).color.setHex(0x22ff55);
          (b.body.material as THREE.MeshStandardMaterial).emissive.setHex(0x22ff55);
        }
      }
    }
    // Pylone fuer M11 (sprengbare Walls an freien Stellen)
    if (mode === "m11") {
      for (const [px, pz] of [[-6, 10], [6, 10], [0, -12]] as [number, number][]) {
        const mat = new THREE.MeshStandardMaterial({ color: 0x33ccff, emissive: 0x33ccff, emissiveIntensity: 0.9 });
        const mesh = new THREE.Mesh(new THREE.BoxGeometry(1, 4, 1), mat);
        mesh.position.set(px, 2, pz);
        scene.add(mesh);
        const wb: WallBox = { mesh, hw: 0.5, hd: 0.5, hh: 2, top: 4, hp: 100, maxHp: 100, destructible: true, active: true };
        walls.push(wb);
        pylonWalls.push(wb);
      }
    }

    /* ---------- KI-Kameraden ---------- */
    interface AllyEnt {
      id: number; name: string; group: THREE.Group; body: THREE.Mesh;
      hp: number; alive: boolean; respawnAt: number; cd: number; kills: number;
      wp: { x: number; z: number; cmd: "hold" | "attack" } | null;
      y: number; vy: number; mantle: number | null;
    }
    const allies: AllyEnt[] = [];
    const ALLY_HEX = [0x33ccff, 0x66ffcc];
    const ALLY_NAMES = ["VEGA", "JUNO"];
    for (let i = 0; i < 2; i++) {
      const group = new THREE.Group();
      const color = new THREE.Color(ALLY_HEX[i]);
      const body = new THREE.Mesh(
        new THREE.BoxGeometry(0.7, 1.5, 0.4),
        new THREE.MeshStandardMaterial({ color, roughness: 0.5, emissive: color.clone().multiplyScalar(0.5) })
      );
      body.position.y = 0.75;
      const head = new THREE.Mesh(
        new THREE.BoxGeometry(0.4, 0.4, 0.4),
        new THREE.MeshStandardMaterial({ color: 0x111111, emissive: color.clone().multiplyScalar(0.6) })
      );
      head.position.y = 1.75;
      group.add(body, head);
      group.position.set(SPAWNS[0][0] + (i === 0 ? 1.5 : -1.5), 0, SPAWNS[0][1] + 1.5);
      scene.add(group);
      allies.push({ id: 101 + i, name: ALLY_NAMES[i], group, body, hp: 100, alive: true, respawnAt: 0, cd: 1, kills: 0, wp: null, y: 0, vy: 0, mantle: null });
    }
    let tactics = false;

    /* ---------- Dr. Maren (Escort, M6) ---------- */
    const maren = {
      group: new THREE.Group(),
      hp: 100, alive: mode === "m6",
      mode: "follow" as "follow" | "wait",
    };
    if (mode === "m6") {
      const col = new THREE.Color(0xff99cc);
      const body = new THREE.Mesh(new THREE.BoxGeometry(0.6, 1.5, 0.4), new THREE.MeshStandardMaterial({ color: col, emissive: col.clone().multiplyScalar(0.4) }));
      body.position.y = 0.75;
      const head = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.4, 0.4), new THREE.MeshStandardMaterial({ color: 0x222222, emissive: col.clone().multiplyScalar(0.5) }));
      head.position.y = 1.75;
      maren.group.add(body, head);
      maren.group.position.set(SPAWNS[0][0] + 2, 0, SPAWNS[0][1] + 2);
      scene.add(maren.group);
    }

    /* ---------- Lockdrock (M7) ---------- */
    interface Drock { x: number; z: number; until: number; }
    const drocks: Drock[] = [];
    const throwDrock = () => {
      if (mode !== "m7" || player.nadeCd > 0) return;
      player.nadeCd = 1.2;
      const dir = new THREE.Vector3();
      camera.getWorldDirection(dir);
      const at = new THREE.Vector3(player.x, 0, player.z).addScaledVector(new THREE.Vector3(dir.x, 0, dir.z).normalize(), 14);
      drocks.push({ x: at.x, z: at.z, until: gameTime + 6 });
      sThrow();
      burst(new THREE.Vector3(at.x, 0.5, at.z), 0x8899ff, 12);
      pushFeed(" Lockdrock aktiv – sie riechen es.");
      for (const b of bots) {
        if (b.alive && Math.hypot(b.group.position.x - at.x, b.group.position.z - at.z) < 16) {
          b.hp -= 15;
          if (b.hp <= 0) kill(0, b.id);
        }
      }
    };

    /* ---------- Spieler-State ---------- */
    const player = {
      x: SPAWNS[0][0], z: SPAWNS[0][1], y: 0, vy: 0,
      vx: 0, vz: 0,
      hp: 100, ammo: 24, reloading: 0, fireCd: 0,
      weapon: "dorn" as "dorn" | "brecher" | "richter",
      kills: 0, deaths: 0, respawnAt: 0,
      jumps: 0, jumpHeld: false, jbuf: 0, coyote: 0,
      crouch: false, prone: false, proneHeld: false, prevCrouch: false,
      slideT: 0, camH: 1.7, bobPhase: 0, landDip: 0, stepAcc: 0,
      mantleTarget: null as number | null,
      shield: 100, lastDmg: -99, bloom: 0, meleeCd: 0, nadeCd: 0, grenades: 2,
      dmgDirs: [] as { a: number; t: number }[],
      codes: 0, upg: {} as Record<string, boolean>, bioOpen: false, usedRevive: false,
      streak: 0, revengeTarget: -1, multiKills: [] as number[], announce: null as { text: string; t: number } | null,
      headshots: 0, melees: 0,
      skinColor: SKINS.find((s) => s.id === loadout.skin && level >= s.level)?.color ?? "#22ff55",
      rageT: 0, bonusXp: 0, usedStreaks: [] as number[],
      bestStreakM: 0, shakeT: 0,
      noGun: false, dornFound: false, frags: 0, terminals: 0, termProg: 0,
      alarm: false, seenT: 0,
      spawnShield: 2, killcam: null as { botId: number; t: number } | null,
      rangeT: 60, rangeHits: 0, shots: 0,
    };
    if (mode === "m0") player.noGun = true;
    if (mode === "m8") player.rangeT = 150;
    const ngMods = loadNG().mods;
    const ngOn = ngMods.length > 0;
    if (ngOn && ngMods.includes("half")) player.shield = 50;
    if (ngOn && ngMods.includes("iron")) player.noGun = true;
    yaw.position.set(player.x, 1.7, player.z);

    const teamScore = [0, 0];
    const ffaScore = Array(6).fill(0);
    let gameTime = 0;
    let ended = false;
    let missionKills = 0;
    let missionDestroyed = 0;
    const finishMission = (win: boolean) => {
      if (win) { addXp(250 * xpMul); updateDaily("wins"); }
      fillEnd();
      ended = true;
      setFailed(!win);
      if (win && mission) {
        try {
          const done = loadCampaign();
          if (!done.includes(mission.id)) done.push(mission.id);
          localStorage.setItem(CAMP_KEY, JSON.stringify(done));
        } catch { /* ignore */ }
      }
      setWinner(win ? `MISSION ${mission ? mission.id.toUpperCase() : ""} ERFÜLLT` : "MISSION GESCHEITERT");
      setScreen("end");
    };

    const pushFeed = (t: string) => {
      feedRef.current.push(t);
      if (feedRef.current.length > 5) feedRef.current.shift();
    };

    const nameOf = (id: number) =>
      id === 0 ? "DU" : id >= 100 ? ALLY_NAMES[id - 101] ?? "?" : BOT_NAMES[id - 1] ?? "?";
    const kill = (killerId: number, victimId: number) => {
      if (victimId === 0) { player.hp = 0; player.deaths++; player.respawnAt = gameTime + 3; player.streak = 0; player.revengeTarget = killerId; banter("death"); }
      else if (victimId >= 100) {
        const a = allies.find((x) => x.id === victimId)!;
        a.hp = 0; a.alive = false; a.respawnAt = gameTime + 5;
        burst(a.group.position.clone().add(new THREE.Vector3(0, 1, 0)), 0x33ccff, 14);
        a.group.visible = false;
      } else {
        const b = bots.find((x) => x.id === victimId)!;
        b.hp = 0; b.alive = false; b.respawnAt = gameTime + 3;
        burst(b.group.position.clone().add(new THREE.Vector3(0, 1, 0)), b.color.getHex(), 18);
        b.group.visible = false;
      }
      if (killerId >= 100) allies.find((x) => x.id === killerId)!.kills++;
      else if (killerId > 0) bots.find((x) => x.id === killerId)!.kills++;
      recEvent(`kill:${victimId}`);
      if (killerId === 0 && victimId > 0 && victimId < 100) updateDaily("kills");
      if (victimId > 0 && victimId < 100 && Math.random() < 0.4) {
        const other = bots.find((x) => x.alive && x.id !== victimId);
        if (other) pushFeed(`📻 ${other.name}: „${nameOf(victimId)} ist down! Bleibt scharf!“`);
      }
      if (killerId === 0) {
        player.kills++; missionKills++; player.codes++;
        // Streak / Spree
        player.streak++;
        player.bestStreakM = Math.max(player.bestStreakM, player.streak);
        if (player.streak === 3) banter("streak");
        if (player.streak === 3) { player.announce = { text: "RAMPAGE!", t: gameTime }; sAnnounce(); }
        if (player.streak === 5) { player.announce = { text: "UNSTOPPBAR!", t: gameTime }; sAnnounce(); }
        if (player.streak === 8) { player.announce = { text: "GOTTGLEICH!", t: gameTime }; sAnnounce(); }
        // Multi-Kill-Fenster
        player.multiKills = player.multiKills.filter((t) => gameTime - t < 2.5);
        player.multiKills.push(gameTime);
        if (player.multiKills.length === 2) { player.announce = { text: "DOUBLE KILL!", t: gameTime }; sAnnounce(); }
        if (player.multiKills.length === 3) { player.announce = { text: "TRIPLE KILL!", t: gameTime }; sAnnounce(); }
        if (player.multiKills.length === 4) { player.announce = { text: "MULTI-KILL!", t: gameTime }; sAnnounce(); }
        // Revenge
        if (victimId === player.revengeTarget) { player.announce = { text: "REVENGE!", t: gameTime }; sAnnounce(); player.revengeTarget = -1; }
        // XP
        addXp((100 + player.bonusXp) * xpMul);
        player.bonusXp = 0;
        // Killstreaks (CoD-DNA)
        for (const ks of KSTREAKS) {
          if (!loadout.streaks.includes(ks.id) || level < ks.level) continue;
          if (player.streak >= ks.num && !player.usedStreaks.includes(ks.num)) {
            player.usedStreaks.push(ks.num);
            activateStreak(ks.id);
          }
        }
        // Stats persistieren
        try {
          const st = loadStats();
          st.kills++; st.bestStreak = Math.max(st.bestStreak, player.streak);
          localStorage.setItem(STAT_KEY, JSON.stringify(st));
        } catch { /* ignore */ }
      }
      if (mode === "range") {
        if (killerId === 0) {
          player.rangeHits++;
          const vb = bots.find((x) => x.id === victimId);
          if (vb) vb.respawnAt = gameTime + 0.4;
        }
      } else if (mode === "ffa") { if (killerId < 6) ffaScore[killerId]++; }
      else teamScore[killerId === 0 || killerId >= 100 ? 0 : bots.find((x) => x.id === killerId)!.team]++;
      pushFeed(`${nameOf(killerId)} ⚡ ${nameOf(victimId)}`);
    };

    const strikes: { at: number; pos: THREE.Vector3 }[] = [];
    const activateStreak = (id: string) => {
      const def = KSTREAKS.find((k) => k.id === id)!;
      player.announce = { text: `${def.name.toUpperCase()}!`, t: gameTime };
      sAnnounce();
      const dir = new THREE.Vector3();
      camera.getWorldDirection(dir);
      const target = new THREE.Vector3(player.x, 0, player.z).addScaledVector(new THREE.Vector3(dir.x, 0, dir.z).normalize(), 18);
      if (id === "recon") {
        for (const b of bots) if (b.alive) b.markedT = gameTime + 5;
        pushFeed(" Recon-Puls: Feinde markiert!");
      }
      if (id === "ammo") {
        player.grenades = Math.min(4, player.grenades + 2);
        player.ammo = player.weapon === "richter" ? 5 : 24;
        player.reloading = 0;
        pushFeed("📦 Nachschub angekommen!");
      }
      if (id === "mortar") strikes.push({ at: gameTime + 0.8, pos: target });
      if (id === "orbital") {
        strikes.push({ at: gameTime + 0.8, pos: target.clone() });
        strikes.push({ at: gameTime + 1.3, pos: target.clone().add(new THREE.Vector3(2.2, 0, 1.2)) });
        strikes.push({ at: gameTime + 1.8, pos: target.clone().add(new THREE.Vector3(-2.0, 0, 1.8)) });
        pushFeed("🛰 ORBITAL-SCHLAG eingeleitet!");
      }
      if (id === "rage") {
        player.rageT = 10;
        player.shield = 100;
        pushFeed("🧬 BIOMASS-RAGE!");
      }
    };

    // Smart-Spawn: Punkt mit groesster Distanz zu Feinden waehlen
    const pickSpawn = (px: number, pz: number, avoidBots: boolean) => {
      let best = SPAWNS[0], bestScore = -1;
      for (const sp of SPAWNS) {
        let score = 999;
        if (avoidBots) {
          for (const b of bots) {
            if (!b.alive) continue;
            score = Math.min(score, Math.hypot(b.group.position.x - sp[0], b.group.position.z - sp[1]));
          }
        } else {
          score = Math.hypot(px - sp[0], pz - sp[1]);
        }
        score += Math.random() * 4;
        if (score > bestScore) { bestScore = score; best = sp; }
      }
      return best;
    };

    const hurtPlayer = (dmgIn: number, fx: number, fz: number, killerId: number) => {
      if (player.spawnShield > 0) return;
      const dmg = dmgIn * (player.upg.c1 ? 0.9 : 1);
      player.lastDmg = gameTime;
      const rel = Math.atan2(fx - player.x, fz - player.z) - yaw.rotation.y - Math.PI;
      player.dmgDirs.push({ a: rel, t: gameTime });
      if (player.dmgDirs.length > 4) player.dmgDirs.shift();
      // AimPunch: Treffer kickt die Ansicht (tight feel)
      targetPitch += 0.012 + Math.random() * 0.006;
      targetYaw += (Math.random() - 0.5) * 0.012;
      if (player.hp < 30 && player.hp > 0) banter("lowhp");
      if (player.shield > 0) {
        player.shield -= dmg;
        sShieldHit();
        if (player.shield < 0) { player.hp += player.shield; player.shield = 0; sHurt(); }
      } else {
        player.hp -= dmg;
        sHurt();
      }
      if (player.hp <= 0 && player.upg.c3 && !player.usedRevive && killerId !== 0 && killerId !== -1) {
        player.usedRevive = true;
        player.hp = 50;
        player.shield = 0;
        sPickup();
        pushFeed("🧬 CHITIN-OVERDRIVE – wieder im Kampf!");
        return;
      }
      if (player.hp <= 0) {
        if (killerId === 0 || killerId === -1) {
          player.hp = 0; player.deaths++; player.respawnAt = gameTime + 3;
          pushFeed(killerId === -1 ? "💀 Selbst zerlegt" : "💀 DU");
        } else kill(killerId, 0);
      }
    };

    /* ---------- Input ---------- */
    const keys: Record<string, boolean> = {};
    let mouseDown = false;
    let lastMX = 0;
    const el = renderer.domElement;
    let targetYaw = 0;
    let targetPitch = 0;
    const kd = (e: KeyboardEvent) => {
      keys[e.code] = true;
      if (e.code === "KeyT") {
        tactics = !tactics;
        pushFeed(tactics ? "🧠 TAKTIK // Zeit eingefroren" : "Taktik beendet – Zeit läuft");
        return;
      }
      if (e.code === "KeyG") throwNade();
      if (e.code === "KeyH") throwDrock();
      if (e.code === "KeyF" && mode === "m6") {
        maren.mode = maren.mode === "follow" ? "wait" : "follow";
        pushFeed(maren.mode === "follow" ? "👩‍🔬 MAREN: ‚Ich folge dir.‘" : "👩‍🔬 MAREN: ‚Ich warte hier. Beeil dich.‘");
      }
      if (e.code === "KeyV") melee();
      if (e.code === "KeyB") player.bioOpen = !player.bioOpen;
      if ((e.code === "Enter" || e.code === "Escape") && cine.active) { cine.active = false; hudSubtitle = null; }
      if (tactics) {
        if (e.code === "Digit1") { allies.forEach((a) => { a.wp = null; }); pushFeed("Befehl: FOLGEN"); }
        if (e.code === "Digit2") { allies.forEach((a) => { a.wp = { x: a.group.position.x, z: a.group.position.z, cmd: "hold" }; }); pushFeed("Befehl: STELLUNG HALTEN"); }
        if (e.code === "Digit3") { allies.forEach((a) => { a.wp = a.wp ? { ...a.wp, cmd: "attack" } : { x: a.group.position.x, z: a.group.position.z, cmd: "hold" }; }); pushFeed("Befehl: WAYPOINT ANGREIFEN"); }
        return;
      }
      if (e.code === "KeyQ" || e.code === "Digit1" || e.code === "Digit2" || e.code === "Digit3") {
        const next = e.code === "KeyQ"
          ? (player.weapon === "dorn" ? "brecher" : player.weapon === "brecher" ? "richter" : "dorn")
          : e.code === "Digit1" ? "dorn" : e.code === "Digit2" ? "brecher" : "richter";
        if (player.noGun) return;
        if (level < (WEAPON_LEVEL[next] ?? 1)) { pushFeed(`🔒 ${next.toUpperCase()} braucht Level ${WEAPON_LEVEL[next]}`); return; }
        player.weapon = next;
        player.ammo = player.weapon === "richter" ? 5 : 24;
        player.reloading = 0;
      }
    };
    const ku = (e: KeyboardEvent) => { keys[e.code] = false; };
    const md = (e: MouseEvent) => {
      if (e.button === 2) { melee(); return; }
      if (tactics) {
        const r = el.getBoundingClientRect();
        const nx = ((e.clientX - r.left) / r.width) * 2 - 1;
        const ny = -((e.clientY - r.top) / r.height) * 2 + 1;
        raycaster.setFromCamera(new THREE.Vector2(nx, ny), camera);
        const t = new THREE.Vector3();
        if (raycaster.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0, 1, 0), 0), t)) {
          const cmd = allies[0]?.wp?.cmd === "hold" ? "hold" : "attack";
          allies.forEach((a) => { a.wp = { x: t.x, z: t.z, cmd }; });
          pushFeed("📍 Waypoint gesetzt");
        }
        return;
      }
      mouseDown = true;
      try { el.requestPointerLock(); } catch { /* */ }
    };
    const mu = () => { mouseDown = false; };
    const mm = (e: MouseEvent) => {
      if (document.pointerLockElement === el) {
        targetYaw -= e.movementX * 0.0022;
        targetPitch = Math.max(-1.4, Math.min(1.4, targetPitch - e.movementY * 0.0022));
      } else if (mouseDown) {
        targetYaw -= (e.clientX - lastMX) * 0.004;
      }
      lastMX = e.clientX;
    };
    const cm = (e: Event) => e.preventDefault();
    window.addEventListener("keydown", kd);
    window.addEventListener("keyup", ku);
    el.addEventListener("mousedown", md);
    el.addEventListener("contextmenu", cm);
    window.addEventListener("mouseup", mu);
    window.addEventListener("mousemove", mm);

    /* ---------- Waffen-Viewmodel ---------- */
    const gun = new THREE.Group();
    const gunBodyMat = new THREE.MeshStandardMaterial({ color: 0x1a1f1a, emissive: 0x0a2a10, roughness: 0.6, metalness: 0.4 });
    const gunBody = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.14, 0.7), gunBodyMat);
    const barrel = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.06, 0.35), new THREE.MeshStandardMaterial({ color: 0x111511, metalness: 0.7, roughness: 0.35 }));
    barrel.position.set(0, 0.03, -0.5);
    const mag = new THREE.Mesh(new THREE.BoxGeometry(0.08, 0.22, 0.12), gunBodyMat);
    mag.position.set(0, -0.16, -0.05);
    mag.rotation.x = 0.35;
    const sightM = new THREE.Mesh(new THREE.BoxGeometry(0.03, 0.05, 0.03), new THREE.MeshStandardMaterial({ color: 0x22ff55, emissive: 0x22ff55, emissiveIntensity: 1.4 }));
    sightM.position.set(0, 0.11, -0.2);
    const stripe = new THREE.Mesh(new THREE.BoxGeometry(0.125, 0.02, 0.5), new THREE.MeshBasicMaterial({ color: 0x22ff55 }));
    stripe.position.set(0, -0.06, -0.1);
    const gunTip = new THREE.Mesh(new THREE.BoxGeometry(0.05, 0.05, 0.2), new THREE.MeshStandardMaterial({ color: 0x22ff55, emissive: 0x22ff55 }));
    gunTip.position.set(0, 0.02, -0.45);
    gun.add(gunBody, barrel, mag, sightM, stripe, gunTip);
    gun.position.set(0.3, -0.28, -0.6);
    camera.add(gun);
    const muzzleLight = new THREE.PointLight(0x88ff88, 0, 6);
    muzzleLight.position.set(0.3, -0.2, -1);
    camera.add(muzzleLight);

    /* ---------- Schießen ---------- */
    const raycaster = new THREE.Raycaster();
    const shoot = () => {
      if (tactics || player.noGun) return;
      const rate = player.weapon === "brecher" ? 0.9 : player.weapon === "richter" ? 1.1 : 0.18;
      if (player.fireCd > 0 || player.reloading > 0 || player.ammo <= 0) return;
      player.fireCd = rate;
      player.ammo--;
      sShot(player.weapon);
      muzzleLight.intensity = 3;

      // Attachment-Effekte
      const att = loadout.attach?.[player.weapon] ?? [];
      const bloomMul = (att.includes("optic") ? 0.7 : 1) * (att.includes("leicht") ? 1.15 : 1);
      const recoilMul = (att.includes("grip") ? 0.5 : 1) * (att.includes("schwer") ? 1.3 : 1);
      const dmgMul = att.includes("schwer") ? 1.15 : 1;
      // Spread-Bloom + Recoil-Kick
      raycaster.setFromCamera(
        new THREE.Vector2((Math.random() - 0.5) * player.bloom * 0.07 * bloomMul, (Math.random() - 0.5) * player.bloom * 0.07 * bloomMul),
        camera
      );
      player.bloom = Math.min(1, player.bloom + (player.weapon === "dorn" ? 0.16 : 0.3));
      if (mode === "m5" && !player.alarm) {
        player.alarm = true;
        banter("boss");
        pushFeed("⚠ ALARM! Schusswechsel gehört – sie kommen!");
        sRadio(); player.shakeT = 0.5;
      }
      targetPitch += (player.weapon === "brecher" ? 0.014 : player.weapon === "richter" ? 0.01 : 0.004) * recoilMul;
      player.shots++;
      const botMeshes: THREE.Object3D[] = [];
      for (const b of bots) if (b.alive) botMeshes.push(b.body, b.group.children[1]);
      const wallMeshes = walls.filter((w) => w.active).map((w) => w.mesh);
      const hits = raycaster.intersectObjects([...botMeshes, ...wallMeshes], false);
      const origin = new THREE.Vector3();
      camera.getWorldPosition(origin);
      const muzzle = new THREE.Vector3();
      gunTip.getWorldPosition(muzzle);
      let end = raycaster.ray.at(60, new THREE.Vector3());
      if (hits.length > 0) {
        const h = hits[0];
        end = h.point;
        const botHit = bots.find((b) => b.alive && (h.object === b.body || h.object.parent === b.group));
        if (botHit && (mode === "ffa" || botHit.team === 1 || botHit.isBoss)) {
          if (botHit.shield) {
            burst(h.point, 0x33ccff, 6);
            if (Math.random() < 0.2) pushFeed("🛡 Schild aktiv – SPRENG DIE PYLONE!");
          } else {
          const head = h.object === botHit.group.children[1];
          let dmg = player.weapon === "brecher" ? 80 : player.weapon === "richter" ? 100 : 26;
          if (player.rageT > 0) dmg = Math.round(dmg * 1.5);
          dmg = Math.round(dmg * dmgMul);
          if (head) { dmg = Math.round(dmg * 2.5); sHeadshot(); pushFeed("🎯 HEADSHOT!"); player.headshots++; updateDaily("headshots"); }
          if (player.upg.s2) botHit.markedT = gameTime + 3;
          botHit.hp -= dmg;
          burst(h.point, head ? 0xffcc33 : 0xff5544, head ? 10 : 6);
          spawnDmgNum(h.point.clone(), dmg, botHit.hp <= 0);
          if (botHit.hp <= 0) kill(0, botHit.id);
          }
        } else if (!botHit) {
          const wall = walls.find((w) => w.active && w.mesh === h.object);
          if (wall) {
            if (player.weapon === "brecher" && wall.destructible) {
              wall.hp -= 100 * breachMul;
              if (pylonWalls.includes(wall) && wall.hp <= 0) {
                const boss = bots.find((b) => b.isBoss);
                if (boss) {
                  const left = pylonWalls.filter((w) => w.active && w.hp > 0).length;
                  if (left === 0) { boss.shield = false; pushFeed("⚡ ALLE PYLONE DOWN – Schild permanent OFFEN!"); }
                  else { boss.shield = false; setTimeout(() => { if (pylonWalls.some((w) => w.active && w.hp > 0)) boss.shield = true; }, 12000); pushFeed(`⚡ Pylon gesprengt – Schild 12 s OFFEN (${left - 1} übrig)`); }
                }
                sRadio();
              }
              burst(h.point, 0x22ff55, 14);
              const m = wall.mesh.material as THREE.MeshStandardMaterial;
              m.color.setHex(0x7a4d1f);
              if (wall.hp <= 0) {
                wall.active = false;
                occDirty = true;
                scene.remove(wall.mesh);
                sBoom();
                burst(wall.mesh.position.clone(), 0x22ff55, 30);
                pushFeed("💥 BRESCHE GESPRENGT!");
                missionDestroyed++;
              }
            } else {
              burst(h.point, 0x88ffaa, 4);
            }
          }
        }
      }
      tracer(muzzle, end, player.weapon === "brecher" ? 0xffcc33 : player.weapon === "richter" ? 0x33ccff : 0x22ff55);
      if (player.ammo === 0) player.reloading = 1.2;
    };

    /* ---------- Melee (Halo-DNA) ---------- */
    const melee = () => {
      if (player.meleeCd > 0 || player.hp <= 0 || tactics) return;
      player.meleeCd = 0.6;
      sMelee();
      player.melees++;
      targetPitch += 0.02;
      player.vx += -Math.sin(yaw.rotation.y) * 3.2;
      player.vz += -Math.cos(yaw.rotation.y) * 3.2;
      for (const b of bots) {
        if (!b.alive) continue;
        const dx = b.group.position.x - player.x, dz = b.group.position.z - player.z;
        const d = Math.hypot(dx, dz);
        if (d < 2.4) {
          const vx_ = -Math.sin(yaw.rotation.y), vz_ = -Math.cos(yaw.rotation.y);
          if ((dx / d) * vx_ + (dz / d) * vz_ > 0.5) {
            const silent = mode === "m5" && !player.alarm && b.hp <= 70;
            b.hp -= 70;
            if (silent) { pushFeed("🗡 Lautlos ausgeschaltet. +50 XP"); addXp(50 * xpMul); }
            burst(b.group.position.clone().add(new THREE.Vector3(0, 1.2, 0)), 0xffffff, 8);
            sHit();
            if (b.hp <= 0) kill(0, b.id);
          }
        }
      }
    };

    /* ---------- Dynamischer Soundtrack (Exploration- vs. Combat-Layer) ---------- */
    const mus = { intensity: 0, beat: 0, started: false };
    const startMusic = () => {
      const c = ac(); if (!c || mus.started) return;
      mus.started = true;
      const drone = c.createOscillator(); drone.type = "sine"; drone.frequency.value = 55;
      const drone2 = c.createOscillator(); drone2.type = "sine"; drone2.frequency.value = 57.3;
      const dg = c.createGain(); dg.gain.value = 0.035;
      const dg2 = c.createGain(); dg2.gain.value = 0.028;
      drone.connect(dg).connect(c.destination);
      drone2.connect(dg2).connect(c.destination);
      drone.start(); drone2.start();
      const pad = c.createOscillator(); pad.type = "triangle"; pad.frequency.value = 220;
      const pg = c.createGain(); pg.gain.value = 0.008;
      const lfo = c.createOscillator(); lfo.frequency.value = 0.13;
      const lg = c.createGain(); lg.gain.value = 0.006;
      lfo.connect(lg).connect(pg.gain);
      pad.connect(pg).connect(c.destination);
      pad.start(); lfo.start();
    };
    const musicTick = (dt: number) => {
      startMusic();
      // Combat-Intensity: sichtbarer naher Feind oder kürzlich Schaden
      let near = 99;
      for (const b of bots) {
        if (!b.alive) continue;
        const d = Math.hypot(b.group.position.x - player.x, b.group.position.z - player.z);
        if (d < near) near = d;
      }
      const target = near < 14 || gameTime - player.lastDmg < 2 ? 1 : near < 25 ? 0.5 : 0;
      mus.intensity += (target - mus.intensity) * Math.min(1, dt * 1.5);
      mus.beat -= dt;
      if (mus.intensity > 0.25 && mus.beat <= 0) {
        mus.beat = 0.55 - 0.15 * mus.intensity;
        const c = ac(); if (!c) return;
        const t = c.currentTime;
        const o = c.createOscillator(); const g = c.createGain();
        o.type = "sine"; o.frequency.setValueAtTime(110, t); o.frequency.exponentialRampToValueAtTime(38, t + 0.12);
        g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.09 * mus.intensity, t + 0.006); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.16);
        o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.18);
      }
    };

    /* ---------- 3D-Damage-Numbers ---------- */
    const dmgSprites: { sp: THREE.Sprite; t: number; y0: number }[] = [];
    const spawnDmgNum = (pos: THREE.Vector3, val: number, kill: boolean) => {
      if (dmgSprites.length > 18) { const old = dmgSprites.shift()!; scene.remove(old.sp); }
      const cv = document.createElement("canvas");
      cv.width = 64; cv.height = 32;
      const c = cv.getContext("2d")!;
      c.font = "bold 22px monospace";
      c.textAlign = "center";
      c.fillStyle = kill ? "#ff5544" : "#ffffff";
      c.fillText(String(val), 32, 24);
      const tex = new THREE.CanvasTexture(cv);
      const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, transparent: true, depthTest: false }));
      sp.scale.set(1.1, 0.55, 1);
      sp.position.copy(pos).add(new THREE.Vector3((Math.random() - 0.5) * 0.4, 0.3, 0));
      scene.add(sp);
      dmgSprites.push({ sp, t: gameTime, y0: sp.position.y });
    };
    const dmgTick = () => {
      for (let i = dmgSprites.length - 1; i >= 0; i--) {
        const d = dmgSprites[i];
        const age = gameTime - d.t;
        d.sp.position.y = d.y0 + age * 1.4;
        (d.sp.material as THREE.SpriteMaterial).opacity = Math.max(0, 1 - age / 0.6);
        if (age > 0.6) { scene.remove(d.sp); dmgSprites.splice(i, 1); }
      }
    };

    /* ---------- Postprocessing: Bloom ab Med ---------- */
    let composer: EffectComposer | null = null;
    if (quality !== "low") {
      composer = new EffectComposer(renderer);
      composer.addPass(new RenderPass(scene, camera));
      composer.addPass(new UnrealBloomPass(new THREE.Vector2(mount.clientWidth, mount.clientHeight), quality === "high" ? 0.65 : 0.4, 0.7, 0.72));
    }

    /* ---------- Explosions-Licht ---------- */
    const flashLight = new THREE.PointLight(0xffaa33, 0, 16);
    scene.add(flashLight);

    /* ---------- Granaten (3D, physikalisch) ---------- */
    const nades3: { mesh: THREE.Mesh; vel: THREE.Vector3; t: number; killer: number }[] = [];
    const nadGeo = new THREE.SphereGeometry(0.12, 8, 8);
    const throwNade = () => {
      if (player.grenades <= 0 || player.nadeCd > 0 || player.hp <= 0 || tactics) return;
      player.nadeCd = 0.5;
      player.grenades--;
      sThrow();
      const mesh = new THREE.Mesh(nadGeo, new THREE.MeshStandardMaterial({ color: 0xffcc33, emissive: 0x664400 }));
      const dir = new THREE.Vector3();
      camera.getWorldDirection(dir);
      mesh.position.set(player.x, player.y + 1.5, player.z);
      scene.add(mesh);
      nades3.push({ mesh, vel: dir.multiplyScalar(11).add(new THREE.Vector3(0, 3, 0)), t: 1.25, killer: 0 });
    };
    const explode3 = (pos: THREE.Vector3) => {
      sBoom();
      flashLight.position.copy(pos);
      flashLight.intensity = 12;
      burst(pos, 0xffcc33, 26);
      burst(pos, 0xff5544, 14);
      for (const b of bots) {
        if (!b.alive) continue;
        const d = Math.hypot(b.group.position.x - pos.x, b.group.position.z - pos.z);
        if (d < 3.2) {
          b.hp -= Math.round(95 * (1 - d / 3.6));
          if (b.hp <= 0) kill(0, b.id);
        }
      }
      const dP = Math.hypot(player.x - pos.x, player.z - pos.z);
      if (dP < 3.2 && player.hp > 0) hurtPlayer(Math.round(70 * (1 - dP / 3.6)), pos.x, pos.z, -1);
      for (const w of walls) {
        if (!w.active || !w.destructible) continue;
        const d = Math.hypot(w.mesh.position.x - pos.x, w.mesh.position.z - pos.z);
        if (d < 2.4) {
          w.hp -= 150;
          if (w.hp <= 0) {
            w.active = false; scene.remove(w.mesh);
            burst(w.mesh.position.clone(), 0x22ff55, 24);
            pushFeed("💥 BRESCHE GESPRENGT!");
            missionDestroyed++;
            updateDaily("destroyed");
            if (typeof pylonWalls !== "undefined" && pylonWalls.includes(w)) {
              const boss = bots.find((b) => b.isBoss);
              if (boss) {
                boss.shield = false;
                pushFeed("⚡ Pylon zerstört – Schild DOWN!");
                sRadio();
              }
            }
          } else (w.mesh.material as THREE.MeshStandardMaterial).color.setHex(0x7a4d1f);
        }
      }
    };
    const nadesTick3 = (dt: number) => {
      for (let i = nades3.length - 1; i >= 0; i--) {
        const n = nades3[i];
        n.t -= dt;
        n.vel.y -= 20 * dt;
        const nx = n.mesh.position.x + n.vel.x * dt;
        const nz = n.mesh.position.z + n.vel.z * dt;
        if (collides(nx, n.mesh.position.z, 0.15, n.mesh.position.y)) n.vel.x *= -0.5;
        else n.mesh.position.x = nx;
        if (collides(n.mesh.position.x, nz, 0.15, n.mesh.position.y)) n.vel.z *= -0.5;
        else n.mesh.position.z = nz;
        n.mesh.position.y += n.vel.y * dt;
        const sup = getSupport(n.mesh.position.x, n.mesh.position.z, n.mesh.position.y + 0.5);
        if (n.mesh.position.y < sup + 0.12 && n.vel.y < 0) {
          n.mesh.position.y = sup + 0.12;
          n.vel.y *= -0.45; n.vel.x *= 0.75; n.vel.z *= 0.75;
        }
        if (n.t <= 0) {
          const at = n.mesh.position.clone();
          scene.remove(n.mesh);
          nades3.splice(i, 1);
          explode3(at);
        }
      }
    };

    /* ---------- Waffen-/Granaten-Pickups ---------- */
    const pickups3: { x: number; z: number; type: "richter" | "nades" | "dorn" | "fragment" | "memory"; active: boolean; respawnAt: number; group: THREE.Group }[] = [];
    const mkPick = (x: number, z: number, type: "richter" | "nades" | "dorn" | "fragment" | "memory") => {
      const group = new THREE.Group();
      const col = type === "richter" ? 0x33ccff : type === "nades" ? 0xffcc33 : type === "dorn" ? 0x22ff55 : type === "memory" ? 0xffee88 : 0xcc66ff;
      const box = new THREE.Mesh(new THREE.BoxGeometry(0.45, 0.45, 0.45), new THREE.MeshStandardMaterial({ color: col, emissive: new THREE.Color(col).multiplyScalar(0.7) }));
      box.position.y = 0.6;
      group.add(box);
      group.position.set(x, 0, z);
      scene.add(group);
      pickups3.push({ x, z, type, active: true, respawnAt: 0, group });
    };
    if (mode === "m0") {
      mkPick(0, -14, "dorn");
    } else if (mode === "m10") {
      for (const [mx, mz] of [[-10, -10], [10, -10], [-10, 10], [10, 10]] as [number, number][]) mkPick(mx, mz, "memory");
    } else {
      mkPick(0, -18, "richter"); mkPick(0, 18, "richter"); mkPick(-20, 0, "nades"); mkPick(20, 0, "nades");
      if (mission) {
        const fragSpots: [number, number][] = [[-6, -6], [6, 6], [4, 14]];
        for (const [fx, fz] of fragSpots) mkPick(fx, fz, "fragment");
      }
    }
    const pickupsTick = (dt: number) => {
      for (const pk of pickups3) {
        pk.group.rotation.y += dt * 1.5;
        if (!pk.active) {
          if (gameTime >= pk.respawnAt) { pk.active = true; pk.group.visible = true; }
          continue;
        }
        if (player.hp > 0 && Math.hypot(player.x - pk.x, player.z - pk.z) < 1.1) {
          pk.active = false; pk.group.visible = false; pk.respawnAt = gameTime + 15;
          sPickup();
          if (pk.type === "richter") { player.weapon = "richter"; player.ammo = 5; player.reloading = 0; pushFeed("RICHTER-50 aufgenommen 🎯"); }
          else if (pk.type === "nades") { player.grenades = Math.min(4, player.grenades + 2); pushFeed("💣 +2 Granaten"); }
          else if (pk.type === "dorn") {
            player.noGun = false; player.dornFound = true;
            player.weapon = "dorn"; player.ammo = 24;
            pushFeed("🔫 DORN geborgen. Hallo, alte Freundin.");
            sPickup();
          } else if (pk.type === "memory") {
            player.terminals++;
            const mems = ["ein Namensband: VEGA", "eine Dienstmarke: HALE", "ein Kinderbild: der Garten", "eine Rechnung: 7.000.000 ‚Setzlinge‘"];
            pushFeed(`🕯 Erinnerung ${player.terminals}/4: ${mems[player.terminals - 1]}`);
            sPickup();
          } else {
            player.frags++;
            try {
              const total = loadFrags() + 1;
              localStorage.setItem(FRAG_KEY, JSON.stringify(total));
            } catch { /* */ }
            pushFeed(`◆ Lore-Fragment geborgen (${player.frags}/3)`);
            sPickup();
            banter("frag");
            updateDaily("frags");
          }
        }
      }
    };

    /* ---------- Bot-AI ---------- */
    const losClear = (from: THREE.Vector3, to: THREE.Vector3) => {
      const a = from.clone(); a.y += 1.4;
      const b = to.clone(); b.y += 1.4;
      const d = b.clone().sub(a);
      const len = d.length();
      raycaster.set(a, d.normalize());
      raycaster.far = len;
      const meshes = walls.filter((w) => w.active).map((w) => w.mesh);
      const hit = raycaster.intersectObjects(meshes, false);
      raycaster.far = Infinity;
      return hit.length === 0;
    };

    const botTick = (b: BotEnt, dt: number) => {
      if (mode === "range") {
        // Aim-Range: Ziele stehen, sterben, respawnen woanders
        if (!b.alive && gameTime >= b.respawnAt) {
          const rx = (Math.random() - 0.5) * 60, rz = (Math.random() - 0.5) * 60;
          b.group.position.set(Math.max(-38, Math.min(38, rx)), 0, Math.max(-38, Math.min(38, rz)));
          b.group.visible = true;
          b.hp = 100; b.alive = true;
        }
        return;
      }
      if (!b.alive) {
        if (gameTime >= b.respawnAt) {
          const sp = pickSpawn(b.group.position.x, b.group.position.z, false);
          b.group.position.set(sp[0], 0, sp[1]);
          b.group.visible = true;
          b.hp = 100; b.alive = true;
          b.shieldT = 1;
        }
        return;
      }
      // Ziel suchen
      let target: { x: number; z: number; y: number; id: number; dist: number } | null = null;
      const bp = b.group.position;
      const dP = Math.hypot(player.x - bp.x, player.z - bp.z);
      const enemy = (team: number) => (mode === "ffa" || mode === "m7" ? team !== b.team : team !== b.team);
      if (dP < 30 && enemy(0) && player.hp > 0 && losClear(bp, new THREE.Vector3(player.x, player.y, player.z))) {
        target = { x: player.x, z: player.z, y: player.y, id: 0, dist: dP };
      }
      for (const a of allies) {
        if (!a.alive) continue;
        const d = Math.hypot(a.group.position.x - bp.x, a.group.position.z - bp.z);
        if (d < 30 && (!target || d < target.dist) && losClear(bp, a.group.position)) {
          target = { x: a.group.position.x, z: a.group.position.z, y: a.group.position.y, id: a.id, dist: d };
        }
      }
      for (const o of bots) {
        if (o.id === b.id || !o.alive || !enemy(o.team) || (o.shieldT ?? 0) > 0) continue;
        const d = Math.hypot(o.group.position.x - bp.x, o.group.position.z - bp.z);
        if (d < 30 && (!target || d < target.dist) && losClear(bp, o.group.position)) {
          target = { x: o.group.position.x, z: o.group.position.z, y: o.group.position.y, id: o.id, dist: d };
        }
      }

      // Bewegen
      let tx = bp.x, tz = bp.z;
      if (target) { tx = target.x; tz = target.z; }
      else {
        // Patrol zum nächsten Gegner/Spawnpunkt
        let bd = 1e9;
        for (const o of bots) {
          if (o.id === b.id || !o.alive || (mode !== "ffa" && o.team === b.team)) continue;
          const d = Math.hypot(o.group.position.x - bp.x, o.group.position.z - bp.z);
          if (d < bd) { bd = d; tx = o.group.position.x; tz = o.group.position.z; }
        }
        if (player.hp > 0 && (mode === "ffa" || true)) {
          const d = Math.hypot(player.x - bp.x, player.z - bp.z);
          if (d < bd) { bd = d; tx = player.x; tz = player.z; }
        }
      }
      const dx = tx - bp.x, dz = tz - bp.z;
      const dist = Math.hypot(dx, dz) || 0.001;
      b.strafeT -= dt;
      if (b.strafeT <= 0) { b.strafeT = 0.7 + Math.random(); b.strafeDir = Math.random() < 0.5 ? -1 : 1; }
      const p2 = { x: bp.x, z: bp.z };
      const bSpeed = ((b as unknown as { bossSpeed?: number }).bossSpeed ?? 4.5) * (weeklyEvent().id === "spore" ? 1.1 : 1) * (diffMul === 1.35 ? 1.15 : 1);
      const wantX = !target && dist > 1 ? (dx / dist) * bSpeed * dt : target && target.dist > 8 ? (dx / dist) * bSpeed * dt : 0;
      const wantZ = !target && dist > 1 ? (dz / dist) * bSpeed * dt : target && target.dist > 8 ? (dz / dist) * bSpeed * dt : 0;
      moveWithCollide(p2, wantX, wantZ, 0.5, b.y);
      const blockedB = (wantX !== 0 && Math.abs(p2.x - (bp.x + wantX)) > 0.001) || (wantZ !== 0 && Math.abs(p2.z - (bp.z + wantZ)) > 0.001);
      if (target) moveWithCollide(p2, (-dz / dist) * b.strafeDir * 2.2 * dt, (dx / dist) * b.strafeDir * 2.2 * dt, 0.5, b.y);
      // Bot-Parkour + Flanking: A* durch Breschen, Mantling, sonst Seitwaerts
      if (blockedB && b.mantle == null && dist > 0.5) {
        const probeTop = getSupport(bp.x + (dx / dist) * 0.7, bp.z + (dz / dist) * 0.7, b.y + 2);
        const dh = probeTop - b.y;
        if (dh > 0.1 && dh <= 1.5) { b.mantle = probeTop + 0.02; b.vy = 0; }
        else {
          const st = (b as unknown as { pathT?: number; px?: number; pz?: number });
          st.pathT = (st.pathT ?? 0) - dt;
          if ((st.pathT ?? 0) <= 0) {
            st.pathT = 2;
            const wp = astar(bp.x, bp.z, tx, tz);
            if (wp) { st.px = wp.x; st.pz = wp.z; }
          }
          if (st.px !== undefined && st.pz !== undefined && Math.hypot(st.px - bp.x, st.pz - bp.z) > 1) {
            const p3 = { x: bp.x, z: bp.z };
            const fdx = st.px - bp.x, fdz = st.pz - bp.z;
            const fd = Math.hypot(fdx, fdz) || 0.001;
            moveWithCollide(p3, (fdx / fd) * 4.5 * dt, (fdz / fd) * 4.5 * dt, 0.5, b.y);
            bp.x = p3.x; bp.z = p3.z;
          } else if (!b.flankT || b.flankT <= 0) {
            const side = Math.random() < 0.5 ? 1 : -1;
            b.flankX = bp.x + (-dz / dist) * 8 * side + (dx / dist) * 4;
            b.flankZ = bp.z + (dx / dist) * 8 * side + (dz / dist) * 4;
            b.flankT = 1.2 + Math.random() * 0.8;
          }
        }
      }
      if (b.flankT && b.flankT > 0) {
        b.flankT -= dt;
        const fdx = b.flankX - bp.x, fdz = b.flankZ - bp.z;
        const fd = Math.hypot(fdx, fdz) || 0.001;
        if (fd > 0.8) {
          const p3 = { x: bp.x, z: bp.z };
          moveWithCollide(p3, (fdx / fd) * 4.5 * dt, (fdz / fd) * 4.5 * dt, 0.5, b.y);
          bp.x = p3.x; bp.z = p3.z;
        }
      }
      if (b.mantle != null) {
        b.y += (b.mantle - b.y) * Math.min(1, 8 * dt);
        moveWithCollide(p2, (dx / dist) * 2 * dt, (dz / dist) * 2 * dt, 0.5, b.y);
        if (Math.abs(b.mantle - b.y) < 0.05) { b.y = b.mantle; b.mantle = null; }
      } else {
        b.vy -= 20 * dt;
        const nyB = b.y + b.vy * dt;
        const supB = getSupport(p2.x, p2.z, b.y);
        if (nyB <= supB && b.vy <= 0) { b.y = supB; b.vy = 0; } else b.y = Math.max(0, nyB);
      }
      b.group.position.set(p2.x, b.y, p2.z);
      b.group.rotation.y = Math.atan2(dx, dz);

      // Schießen
      // Lockdrock-Taunt: Bots laufen zum Drock
      const drock = drocks.find((d) => d.until > gameTime && Math.hypot(bp.x - d.x, bp.z - d.z) < 18);
      if (drock && mode === "m7") {
        const ddx = drock.x - bp.x, ddz = drock.z - bp.z;
        const dd = Math.hypot(ddx, ddz) || 0.001;
        if (dd > 1.5) {
          const pp = { x: bp.x, z: bp.z };
          moveWithCollide(pp, (ddx / dd) * 4 * dt, (ddz / dd) * 4 * dt, 0.5, b.y);
          bp.x = pp.x; bp.z = pp.z;
        }
        b.cd -= dt;
        if (b.cd <= 0) {
          b.cd = 0.4;
          for (const o of bots) {
            if (o.id !== b.id && o.alive && o.team !== b.team && Math.hypot(o.group.position.x - bp.x, o.group.position.z - bp.z) < 8) {
              tracer(bp.clone().add(new THREE.Vector3(0, 1.4, 0)), o.group.position.clone().add(new THREE.Vector3(0, 1.2, 0)), b.color.getHex());
              o.hp -= 8;
              if (o.hp <= 0) kill(b.id, o.id);
              break;
            }
          }
        }
        return;
      }
      // Maren als Ziel (M6)
      if (mode === "m6" && maren.alive) {
        const dm = Math.hypot(maren.group.position.x - bp.x, maren.group.position.z - bp.z);
        if (dm < 30 && (!target || dm < target.dist) && losClear(bp, maren.group.position)) {
          target = { x: maren.group.position.x, z: maren.group.position.z, y: 0, id: 200, dist: dm };
        }
      }
      // m5-Infiltration: Patrouille + Sichtkegel + Alarm
      if (mode === "m5" && !player.alarm) {
        target = null;
        const dP2 = Math.hypot(player.x - bp.x, player.z - bp.z);
        const detect = keys["ShiftLeft"] ? 16 : player.crouch ? 7 : 11;
        if (player.hp > 0 && dP2 < detect) {
          const vx_ = Math.sin(b.group.rotation.y), vz_ = Math.cos(b.group.rotation.y);
          const dxp = (player.x - bp.x) / (dP2 || 1), dzp = (player.z - bp.z) / (dP2 || 1);
          const dot = vx_ * dxp + vz_ * dzp;
          if (dot > 0.35 && losClear(bp, new THREE.Vector3(player.x, player.y, player.z))) {
            player.seenT += dt;
            if (player.seenT > 1.1) {
              player.alarm = true;
              pushFeed("⚠ ALARM! Du wurdest entdeckt!");
              sRadio(); player.shakeT = 0.5;
            }
          } else player.seenT = Math.max(0, player.seenT - dt);
        } else player.seenT = Math.max(0, player.seenT - dt);
        // Patrol-Wanderung
        if (!b.flankT || b.flankT <= 0) {
          b.flankX = (Math.random() - 0.5) * 60;
          b.flankZ = (Math.random() - 0.5) * 60;
          b.flankT = 2.5 + Math.random() * 2;
        }
      }
      // Reaktion + Burst-Feuer (AAA-Bot-Feel) – Spawn-Schutz respektieren
      if (player.spawnShield > 0 && target && target.id === 0) target = null;
      const hadTargetBefore = (b as unknown as { hadT?: boolean }).hadT ?? false;
      if (target && !hadTargetBefore && Math.random() < 0.3) {
        pushFeed(`📻 ${b.name}: „Kontakt!“`);
      }
      (b as unknown as { hadT?: boolean }).hadT = !!target;
      if (!target) b.reactT = 0.22 + Math.random() * 0.3;
      else if (b.reactT > 0) b.reactT -= dt;
      if (b.pauseT > 0) b.pauseT -= dt;
      if (target && b.reactT <= 0 && b.pauseT <= 0 && b.burstLeft <= 0) {
        b.burstLeft = 3 + Math.floor(Math.random() * 3);
      }
      b.shieldT = Math.max(0, (b.shieldT ?? 0) - dt);
      // ===== BOT-UTILITY: Granaten & Deckungs-Breaching =====
      const util = (b as unknown as { nadeCd2?: number; wallCd?: number });
      util.nadeCd2 = (util.nadeCd2 ?? 4 + Math.random() * 4) - dt;
      if (target && target.id === 0 && util.nadeCd2 <= 0 && target.dist > 5 && target.dist < 14 && nades3.length < 6) {
        util.nadeCd2 = 7 + Math.random() * 6;
        const ddn = target.dist || 1;
        nades3.push({ mesh: new THREE.Mesh(nadGeo, new THREE.MeshStandardMaterial({ color: 0xff5544, emissive: 0x661111 })), vel: new THREE.Vector3((dx / ddn) * 8, 3.2, (dz / ddn) * 8), t: 1.0, killer: b.id });
        pushFeed(`💣 ${b.name} wirft eine Granate!`);
      }
      util.wallCd = (util.wallCd ?? 5) - dt;
      if (util.wallCd <= 0 && target && target.id === 0 && target.dist > 3 && target.dist < 12 && !losClear(bp, new THREE.Vector3(player.x, player.y, player.z))) {
        util.wallCd = 4 + Math.random() * 3;
        // naechste sprengbare Wand zwischen Bot und Spieler beschiessen
        const wbs = walls.filter((w) => w.active && w.destructible);
        let bestW: (typeof wbs)[number] | null = null; let bdW = 1e9;
        for (const w of wbs) {
          const d1 = Math.hypot(w.mesh.position.x - bp.x, w.mesh.position.z - bp.z);
          const d2 = Math.hypot(w.mesh.position.x - player.x, w.mesh.position.z - player.z);
          if (d1 + d2 < bdW && d2 < 6) { bdW = d1 + d2; bestW = w; }
        }
        if (bestW) {
          bestW.hp -= 100;
          burst(bestW.mesh.position.clone(), 0xff5544, 10);
          sShot("brecher");
          pushFeed(`⚠ ${b.name} beschießt deine Deckung!`);
          if (bestW.hp <= 0) {
            bestW.active = false; occDirty = true; scene.remove(bestW.mesh);
            burst(bestW.mesh.position.clone(), 0x22ff55, 20);
            sBoom();
            pushFeed(`💥 ${b.name} hat eine Wand gesprengt!`);
          } else (bestW.mesh.material as THREE.MeshStandardMaterial).color.setHex(0x7a4d1f);
        }
      }
      // ===== BOSS-LOGIK =====
      if (b.isBoss) {
        const hpFrac = b.hp / (b.bossHp || 1);
        // Phasen
        if (mode === "m9") {
          if (hpFrac < 0.66 && b.phase === 1) { b.phase = 2; pushFeed("🪞 KADE: ‚Dein Recon? Mein Recon.‘"); sRadio(); for (const bb of bots) if (bb.alive) bb.markedT = gameTime + 5; }
          if (hpFrac < 0.33 && b.phase === 2) { b.phase = 3; pushFeed("🪞 KADE enraget – ER NUTZT DEINE RAGE!"); sRadio(); }
        }
        if (mode === "m12") {
          if (hpFrac < 0.66 && b.phase === 1) { b.phase = 2; pushFeed("🌿 DER GÄRTNER: Die Arena welkt."); sRadio(); }
          if (hpFrac < 0.33 && b.phase === 2) { b.phase = 3; pushFeed("🌿 ENRAGE – ER KOMMT SELBST!"); sRadio(); (b.body.material as THREE.MeshStandardMaterial).emissive.setHex(0xff3333); }
          // Summons P1
          if (b.phase === 1) {
            b.summonT -= dt;
            if (b.summonT <= 0) {
              b.summonT = 20;
              if (bots.filter((x) => x.alive && !x.isBoss).length < 4) {
                makeBot(bots.length, 1);
                pushFeed("🌱 Der Gärtner pflanzt Diener.");
              }
            }
          }
          // Arena-Zerfall P2+
          if (b.phase >= 2) {
            b.decayT -= dt;
            if (b.decayT <= 0) {
              b.decayT = 8;
              const des = walls.filter((w) => w.active && w.destructible);
              if (des.length) {
                const w = des[Math.floor(Math.random() * des.length)];
                w.active = false; scene.remove(w.mesh);
                burst(w.mesh.position.clone(), 0x22ff55, 20);
                sBoom();
                pushFeed("⚠ Die Arena zerfällt!");
              }
            }
          }
        }
        const bossSpeed = mode === "m12" && b.phase === 3 ? 6.5 : mode === "m9" && b.phase === 3 ? 6 : 4.5;
        const dmgMul = b.phase >= 2 ? 1.4 : 1;
        (b as unknown as { bossSpeed?: number }).bossSpeed = bossSpeed;
        (b as unknown as { dmgMul?: number }).dmgMul = dmgMul;
      }
      b.cd -= dt;
      if (target && b.burstLeft > 0 && b.cd <= 0 && b.reactT <= 0) {
        b.cd = 0.13;
        b.burstLeft--;
        if (b.burstLeft === 0) b.pauseT = 0.5 + Math.random() * 0.8;
        const from = bp.clone().add(new THREE.Vector3(0, 1.4, 0));
        const to = new THREE.Vector3(target.x, target.y, target.z);
        tracer(from, to, b.color.getHex());
        sShot("dorn");
        const stanceMul = player.prone ? 0.5 : player.crouch ? 0.75 : 1;
        const adapt = Math.max(0.75, Math.min(1.25, 1 + (player.deaths - player.kills) * 0.02)) * (mode === "m0" ? 0.5 : 1) * (ngOn && ngMods.includes("aggro") ? 1.5 : 1);
        const dmg = (6 + Math.random() * 8) * adapt * (perk === "panzer" ? 0.7 : 1) * stanceMul * ((b as unknown as { dmgMul?: number }).dmgMul ?? 1) * (b.isBoss ? 2.2 : 1);
        if (target.id === 0) {
          hurtPlayer(dmg, bp.x, bp.z, b.id);
        } else if (target.id === 200) {
          maren.hp -= dmg;
          if (maren.hp <= 0 && maren.alive) {
            maren.alive = false;
            maren.group.visible = false;
            pushFeed("💔 MAREN gefallen.");
            fillEnd();
            ended = true;
            setFailed(true);
            setWinner("MISSION GESCHEITERT – MAREN VERLOREN");
            setScreen("end");
          }
        } else if (target.id >= 100) {
          const a = allies.find((x) => x.id === target!.id)!;
          a.hp -= dmg;
          if (a.hp <= 0) kill(b.id, a.id);
        } else {
          const v = bots.find((x) => x.id === target!.id)!;
          v.hp -= dmg;
          if (v.hp <= 0) kill(b.id, v.id);
        }
      }
    };

    /* ---------- Kameraden-KI ---------- */
    const allyTick = (a: (typeof allies)[number], dt: number) => {
      if (!a.alive) {
        if (gameTime >= a.respawnAt) {
          a.group.position.set(player.x + (a.id === 101 ? 1.5 : -1.5), 0, player.z + 1.5);
          a.group.visible = true;
          a.hp = 100; a.alive = true; a.y = 0; a.vy = 0; a.mantle = null;
          pushFeed(`${a.name} wieder einsatzbereit`);
        }
        return;
      }
      const bp = a.group.position;
      // Ziel: nächster sichtbarer Bot
      let target: { x: number; z: number; y: number; id: number; dist: number } | null = null;
      for (const o of bots) {
        if (!o.alive) continue;
        const d = Math.hypot(o.group.position.x - bp.x, o.group.position.z - bp.z);
        if (d < 28 && (!target || d < target.dist) && losClear(bp, o.group.position)) {
          target = { x: o.group.position.x, z: o.group.position.z, y: o.group.position.y, id: o.id, dist: d };
        }
      }
      // Bewegungsziel laut Befehl
      let tx = player.x + (a.id === 101 ? 1.5 : -1.5);
      let tz = player.z + 1.5;
      if (a.wp) {
        if (a.wp.cmd === "hold") { tx = a.wp.x; tz = a.wp.z; }
        else {
          tx = a.wp.x; tz = a.wp.z;
          if (Math.hypot(tx - bp.x, tz - bp.z) < 1.2) a.wp = { x: a.wp.x, z: a.wp.z, cmd: "hold" };
        }
      }
      const dx = tx - bp.x, dz = tz - bp.z;
      const dist = Math.hypot(dx, dz) || 0.001;
      const holdStill = a.wp?.cmd === "hold" && dist < 0.5;
      if (!holdStill && dist > 1.1 && !(target && target.dist < 6)) {
        const p2 = { x: bp.x, z: bp.z };
        const wx_ = (dx / dist) * 5.2 * dt, wz_ = (dz / dist) * 5.2 * dt;
        moveWithCollide(p2, wx_, wz_, 0.5, a.y);
        const blockedA = Math.abs(p2.x - (bp.x + wx_)) > 0.001 || Math.abs(p2.z - (bp.z + wz_)) > 0.001;
        if (blockedA && a.mantle == null) {
          const probeTop = getSupport(bp.x + (dx / dist) * 0.7, bp.z + (dz / dist) * 0.7, a.y + 2);
          const dh = probeTop - a.y;
          if (dh > 0.1 && dh <= 1.5) { a.mantle = probeTop + 0.02; a.vy = 0; }
        }
        bp.x = p2.x; bp.z = p2.z;
      }
      if (a.mantle != null) {
        a.y += (a.mantle - a.y) * Math.min(1, 8 * dt);
        if (Math.abs(a.mantle - a.y) < 0.05) { a.y = a.mantle; a.mantle = null; }
      } else {
        a.vy -= 20 * dt;
        const nyA = a.y + a.vy * dt;
        const supA = getSupport(bp.x, bp.z, a.y);
        if (nyA <= supA && a.vy <= 0) { a.y = supA; a.vy = 0; } else a.y = Math.max(0, nyA);
      }
      bp.y = a.y;
      if (target) a.group.rotation.y = Math.atan2(target.x - bp.x, target.z - bp.z);
      else if (dist > 1.1) a.group.rotation.y = Math.atan2(dx, dz);
      // Feuern
      a.cd -= dt;
      if (target && a.cd <= 0) {
        a.cd = 0.5 + Math.random() * 0.4;
        const from = bp.clone().add(new THREE.Vector3(0, 1.4, 0));
        tracer(from, new THREE.Vector3(target.x, 1.4, target.z), 0x33ccff);
        sShot("dorn");
        const v = bots.find((x) => x.id === target!.id)!;
        v.hp -= 12;
        if (v.hp <= 0) kill(a.id, v.id);
      }
    };

    /* ---------- A*-Pathfinding: Bots nutzen Breschen ---------- */
    const GRID = 81; // -40..40
    let occ = new Uint8Array(GRID * GRID);
    let occDirty = true;
    const gIdx = (x: number, z: number) => (Math.min(GRID - 1, Math.max(0, Math.round(z + 40))) * GRID + Math.min(GRID - 1, Math.max(0, Math.round(x + 40))));
    const rebuildOcc = () => {
      occ = new Uint8Array(GRID * GRID);
      for (const w of walls) {
        if (!w.active) continue; // gesprengt = offen = Bot weiss es!
        const x0 = Math.max(0, Math.round(w.mesh.position.x - w.hw - 0.4) + 40);
        const x1 = Math.min(GRID - 1, Math.round(w.mesh.position.x + w.hw + 0.4) + 40);
        const z0 = Math.max(0, Math.round(w.mesh.position.z - w.hd - 0.4) + 40);
        const z1 = Math.min(GRID - 1, Math.round(w.mesh.position.z + w.hd + 0.4) + 40);
        for (let z = z0; z <= z1; z++) for (let x = x0; x <= x1; x++) occ[z * GRID + x] = 1;
      }
      occDirty = false;
    };
    const astar = (sx: number, sz: number, tx: number, tz: number): { x: number; z: number } | null => {
      if (occDirty) rebuildOcc();
      const start = gIdx(sx, sz), goal = gIdx(tx, tz);
      if (occ[goal]) return null;
      const open: number[] = [start];
      const came = new Int32Array(GRID * GRID).fill(-1);
      const gScore = new Float32Array(GRID * GRID).fill(Infinity);
      gScore[start] = 0;
      const f = (i: number) => gScore[i] + Math.abs((i % GRID) - (goal % GRID)) + Math.abs(Math.floor(i / GRID) - Math.floor(goal / GRID));
      let guard = 0;
      while (open.length && guard++ < 2500) {
        open.sort((a, b) => f(a) - f(b));
        const cur = open.shift()!;
        if (cur === goal) {
          // Pfad zurueck: ersten Schritt nach Start finden
          let node = goal;
          while (came[node] !== start && came[node] !== -1) node = came[node];
          return { x: (node % GRID) - 40, z: Math.floor(node / GRID) - 40 };
        }
        const cx = cur % GRID, cz = Math.floor(cur / GRID);
        for (const [dx, dz] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [1, -1], [-1, 1], [-1, -1]]) {
          const nx = cx + dx, nz = cz + dz;
          if (nx < 0 || nz < 0 || nx >= GRID || nz >= GRID) continue;
          const ni = nz * GRID + nx;
          if (occ[ni]) continue;
          const ng = gScore[cur] + (dx !== 0 && dz !== 0 ? 1.4 : 1);
          if (ng < gScore[ni]) { gScore[ni] = ng; came[ni] = cur; if (!open.includes(ni)) open.push(ni); }
        }
      }
      return null;
    };

    /* ---------- Replay-Recorder ---------- */
    const rec = { frames: [] as number[][], events: [] as { t: number; e: string }[], acc: 0 };
    const recEvent = (e: string) => { rec.events.push({ t: Math.round(gameTime * 10) / 10, e }); };
    const banterCd: Record<string, number> = {};
    const banter = (ctx: string) => {
      if ((banterCd[ctx] ?? 0) > gameTime) return;
      if (Math.random() > 0.65) return;
      banterCd[ctx] = gameTime + 12;
      const pool = BANTER[ctx];
      const line = pool[Math.floor(Math.random() * pool.length)];
      pushFeed(`🎙 ${line.spk}: „${line.text}“`);
      sRadio();
    };

    /* ---------- Cinematic-Engine (Story-Beats) ---------- */
    const story = STORY[mode];
    let m8Choice: "" | "vega" | "data" = "";
    let m8ChoiceDone = false;
    const firedEvents: StoryEvent[] = [];
    let hudSubtitle: { speaker: string; text: string } | null = null;
    let m8OfferState: "vega" | "data" | null = null;
    const setM8Offer = (v: "vega" | "data") => { m8OfferState = v; };
    const chooseM8 = (v: "vega" | "data") => {
      m8Choice = v; m8ChoiceDone = true; m8OfferState = null;
      try {
        const st = loadStory();
        st.flags = st.flags ?? {};
        st.flags.save_vega_chosen = true;
        st.flags.save_vega = v === "vega";
        localStorage.setItem(STORY_KEY, JSON.stringify(st));
      } catch { /* */ }
      pushFeed(v === "vega" ? "❤ VEGA: ‚…du Idiot. Danke.‘" : "📀 DATEN: Der Kern surrt in deiner Tasche. VEGA sieht weg.");
      sRadio();
    };
    chooseM8Ref.current = chooseM8;
    const cine = {
      active: !!(story && story.intro.length),
      list: story?.intro ?? [],
      i: 0, t: 0,
      lookCur: new THREE.Vector3(0, 2, 0),
    };
    const cineTick = (dt: number) => {
      const sc = cine.list[cine.i];
      cine.t += dt;
      camera.position.lerp(new THREE.Vector3(...sc.cam), Math.min(1, dt * 2.2));
      cine.lookCur.lerp(new THREE.Vector3(...sc.look), Math.min(1, dt * 2.5));
      camera.lookAt(cine.lookCur);
      hudSubtitle = sc.speaker && sc.text ? { speaker: sc.speaker, text: sc.text } : null;
      if (cine.t >= sc.dur) { cine.t = 0; cine.i++; if (cine.i >= cine.list.length) { cine.active = false; hudSubtitle = null; } }
    };
    const fillEnd = () => {
      // Story-Flag speichern
      if (mission && !ended) {
        try {
          const st = loadStory();
          if (!st.done.includes(mission.id)) st.done.push(mission.id);
          if (!st.flags) st.flags = {};
          localStorage.setItem(STORY_KEY, JSON.stringify(st));
        } catch { /* */ }
      }
      const rank = player.deaths === 0 && player.frags >= 3 ? "S" : player.deaths <= 1 ? "A" : player.deaths <= 3 ? "B" : "C";
      const medals: string[] = [];
      if (player.headshots >= 3) medals.push("🎯 Headhunter – 3+ Headshots");
      if (player.melees >= 2) medals.push("👊 Nahkampf-Dämon – 2+ Melee-Kills");
      if (missionDestroyed >= 2) medals.push("💥 Abrissbirne – 2+ Wände gesprengt");
      if (player.bestStreakM >= 5) medals.push("🔥 Spree-Meister – 5er-Streak");
      if (player.deaths === 0 && player.kills >= 5) medals.push("🛡 Unberührbar – 5 Kills, 0 Tode");
      let debrief = mission ? STORY[mission.id]?.debrief : undefined;
      if (mission?.id === "m12") {
        const sv = loadStory().flags?.save_vega;
        const fragsTotal = loadFrags();
        debrief = fragsTotal >= 18
          ? "GEHEIM-ENDE: Der Gärtner legt seine Waffe nieder. ‚Dann pflanze du.‘ Die Biomass wartet auf einen neuen Gärtner – auf dich."
          : sv
            ? "ENDE A: Die Biomass weicht. Die Erde bleibt nackt, aber frei. VEGA steht neben dir, als der Rauch sich legt."
            : "ENDE B: Die Daten brechen KORP. Doch nachts hörst du die Sporen atmen – und weißt: Du hättest wählen können.";
      }
      if (mission?.id === "m8") {
        const sv = loadStory().flags?.save_vega;
        debrief = sv
          ? "VEGA lebt. Der Datenkern schmort im Labor. Manche Türen öffnen sich nur mit einem Herzschlag."
          : "Der Datenkern ist gesichert. VEGAs Kapsel schloss sich lautlos. Manche Türen öffnen sich nie wieder.";
      }
      replayRef.current = { mode, frames: rec.frames, events: rec.events, date: new Date().toISOString() };
      try {
        if (rec.frames.length < 8000) localStorage.setItem("wirrwarr-lastreplay", JSON.stringify(replayRef.current));
      } catch { /* */ }
      endInfoExt.current = { debrief, medals, kills: player.kills, deaths: player.deaths, hs: player.headshots, frags: player.frags, rank: mission ? rank : undefined };
    };
    const storyTick = () => {
      if (!story || cine.active) return;
      for (const ev of story.events) {
        if (firedEvents.includes(ev)) continue;
        const val = ev.trigger === "time" ? gameTime : ev.trigger === "kills" ? player.kills : mode === "m5" ? player.terminals : missionDestroyed;
        if (val >= ev.at) {
          firedEvents.push(ev);
          pushFeed(`📻 ${ev.speaker}: ${ev.text}`);
          sRadio();
          if (ev.shake) player.shakeT = 0.6;
        }
      }
    };

    /* ---------- Loop ---------- */
    const clock = new THREE.Clock();
    let raf = 0;
    let fpsAcc = 0, fpsFrames = 0, fpsVal = 60;
    let hudAcc = 0;

    const loop = () => {
      raf = requestAnimationFrame(loop);
      const dt = Math.min(0.05, clock.getDelta());
      gameTime += dt;
      player.fireCd -= dt;
      muzzleLight.intensity = Math.max(0, muzzleLight.intensity - 20 * dt);

      // Spieler
      if (player.hp <= 0) {
        if (gameTime >= player.respawnAt) {
          const sp = pickSpawn(player.x, player.z, true);
          player.hp = 100; player.shield = 100; player.x = sp[0]; player.z = sp[1]; player.ammo = 24;
          player.usedStreaks = [];
          player.spawnShield = 2;
        }
      } else if (!cine.active) {
        // ===== Bewegungsfluss: Stance, Slide, Acceleration, Friction =====
        const support = getSupport(player.x, player.z, player.y);
        const onGround = player.y <= support + 0.02;
        if (onGround) player.coyote = 0.12; else player.coyote = Math.max(0, player.coyote - dt);

        const wantCrouch = !!(keys["KeyC"] || keys["ControlLeft"]);
        if (keys["KeyX"] && !player.proneHeld) player.prone = !player.prone;
        player.proneHeld = !!keys["KeyX"];
        player.crouch = player.prone || wantCrouch;

        const speedNow0 = Math.hypot(player.vx, player.vz);
        if (wantCrouch && !player.prevCrouch && speedNow0 > 5.5 && onGround) player.slideT = player.upg.l3 ? 1.15 : 0.75;
        player.prevCrouch = wantCrouch;
        player.slideT = Math.max(0, player.slideT - dt);
        const sliding = player.slideT > 0 && onGround;

        const sprinting = !!keys["ShiftLeft"] && !player.crouch && speedNow0 > 4;
        const attNow = loadout.attach?.[player.weapon] ?? [];
        const maxSp = (player.prone ? 1.5 : player.crouch ? 2.6 : sprinting ? (perk === "sprint" ? 9.6 : 8.6) : 5.2) * (player.upg.l1 && sprinting ? 1.1 : 1) * (player.rageT > 0 ? 1.2 : 1) * (attNow.includes("leicht") ? 1.08 : 1) * (attNow.includes("optic") ? 0.95 : 1);

        // Wish-Richtung + Acceleration (Air-Control)
        let wx = 0, wz = 0;
        const fy_ = yaw.rotation.y;
        if (keys["KeyW"]) { wx -= Math.sin(fy_); wz -= Math.cos(fy_); }
        if (keys["KeyS"]) { wx += Math.sin(fy_); wz += Math.cos(fy_); }
        if (keys["KeyA"]) { wx -= Math.cos(fy_); wz += Math.sin(fy_); }
        if (keys["KeyD"]) { wx += Math.cos(fy_); wz -= Math.sin(fy_); }
        const wl = Math.hypot(wx, wz);
        if (wl > 0.01) { wx /= wl; wz /= wl; }
        const accel = onGround ? 46 : 15;
        player.vx += wx * accel * dt;
        player.vz += wz * accel * dt;
        const damp = onGround ? (wl > 0.01 ? 1 - 1.4 * dt : 1 - 12 * dt) : 1 - 0.15 * dt;
        player.vx *= damp; player.vz *= damp;
        const spNow = Math.hypot(player.vx, player.vz);
        if (!sliding && spNow > maxSp && spNow > 0) { player.vx *= maxSp / spNow; player.vz *= maxSp / spNow; }
        if (sliding && spNow > 0) { const cap = Math.max(maxSp, spNow * (1 - 1.1 * dt)); if (spNow > cap) { player.vx *= cap / spNow; player.vz *= cap / spNow; } }

        // Jump-Buffer + Coyote-Time + Double-Jump
        if (keys["Space"] && !player.jumpHeld) player.jbuf = 0.14; else player.jbuf = Math.max(0, player.jbuf - dt);
        player.jumpHeld = !!keys["Space"];
        if (player.prone && player.jbuf > 0) { player.prone = false; player.jbuf = 0; }
        else if (player.jbuf > 0 && player.coyote > 0 && !player.crouch) { player.vy = player.upg.l2 ? 8.4 : 7.6; player.jumps = 1; player.coyote = 0; player.jbuf = 0; }
        else if (player.jbuf > 0 && !onGround && (perk === "sprung" || player.upg.l2) && player.jumps === 1 && !player.jumpHeld) { player.vy = 7; player.jumps = 2; player.jbuf = 0; }

        // Gravitation + weiche Landung
        player.vy -= 20 * dt;
        const newY = player.y + player.vy * dt;
        if (newY <= support && player.vy <= 0) {
          if (player.vy < -7) {
            player.landDip = 0.16; sLand();
            burst(new THREE.Vector3(player.x, support + 0.1, player.z), 0x88aa88, 8);
          }
          player.y = support; player.vy = 0; player.jumps = 0;
        } else player.y = Math.max(0, newY);

        // Horizontal + Step-Up + butterweiches Mantling
        const dxm = player.vx * dt, dzm = player.vz * dt;
        const bx = player.x, bz = player.z;
        if (player.mantleTarget == null) moveWithCollide(player, dxm, dzm, 0.5, player.y);
        const blockedX = Math.abs(player.x - (bx + dxm)) > 0.001;
        const blockedZ = Math.abs(player.z - (bz + dzm)) > 0.001;
        if ((blockedX || blockedZ) && player.mantleTarget == null) {
          const probeTop = getSupport(
            bx + (blockedX ? Math.sign(dxm) * 0.7 : 0),
            bz + (blockedZ ? Math.sign(dzm) * 0.7 : 0),
            player.y + 2
          );
          const dh = probeTop - player.y;
          if (dh > 0.02 && dh <= 0.55 && onGround) {
            player.y = probeTop; // Step-Up: kleine Kanten ohne Sprung
          } else if (dh > 0.55 && dh <= 1.5 && !player.crouch && onGround && wl > 0.01) {
            player.mantleTarget = probeTop + 0.02;
            player.vy = 0; player.jumps = 0;
          } else {
            if (blockedX) player.vx = 0;
            if (blockedZ) player.vz = 0;
          }
        }
        if (player.mantleTarget != null) {
          player.y += (player.mantleTarget - player.y) * Math.min(1, (player.upg.l1 ? 14 : 10) * dt);
          moveWithCollide(player, dxm * 1.5, dzm * 1.5, 0.5, player.y);
          if (Math.abs(player.mantleTarget - player.y) < 0.04) { player.y = player.mantleTarget; player.mantleTarget = null; }
        }

        // Footsteps
        const speedNow = Math.hypot(player.vx, player.vz);
        if (onGround && speedNow > 1.5 && !sliding) {
          player.stepAcc += speedNow * dt;
          if (player.stepAcc > 2.4) { player.stepAcc = 0; sStep(); }
        }

        if (player.reloading > 0) { player.reloading -= dt; if (player.reloading <= 0) player.ammo = player.weapon === "richter" ? 5 : 24; }
        const maxAm = player.weapon === "richter" ? 5 : 24;
        if (keys["KeyR"] && player.ammo < maxAm && player.reloading <= 0) player.reloading = player.weapon === "richter" ? 1.6 : 1.2;
        if (mouseDown) shoot();

        // Gun-Bob + Sway aus echtem Bewegungsphasenwert
        const gspd = Math.hypot(player.vx, player.vz);
        gun.position.y = -0.28 + Math.sin(player.bobPhase) * Math.min(0.014, gspd * 0.0018);
        gun.position.x = 0.3 + Math.cos(player.bobPhase * 0.5) * Math.min(0.01, gspd * 0.0012);
      }

      // ===== Killcam: Perspektive des Killers =====
      if (player.killcam) {
        const kb = bots.find((b) => b.id === player.killcam!.botId);
        player.killcam.t -= dt;
        if (kb && kb.alive && player.killcam.t > 0) {
          yaw.position.set(kb.group.position.x, kb.group.position.y + 1.7, kb.group.position.z);
          yaw.rotation.y += (kb.group.rotation.y - yaw.rotation.y) * Math.min(1, 10 * dt);
          pitch.rotation.x *= 1 - Math.min(1, 8 * dt);
        } else {
          player.killcam = null;
        }
      }

      // ===== Kamera-Flow: Maus-Smoothing, Stance-Höhe, Bob, Lande-Dip, FOV-Kick =====
      if (!player.killcam && !cine.active) {
      yaw.rotation.y += (targetYaw - yaw.rotation.y) * Math.min(1, 34 * dt);
      pitch.rotation.x += (targetPitch - pitch.rotation.x) * Math.min(1, 34 * dt);
      const heightT = player.prone ? 0.55 : player.crouch ? 1.15 : 1.7;
      player.camH += (heightT - player.camH) * Math.min(1, 12 * dt);
      const camSpd = Math.hypot(player.vx, player.vz);
      const camGrounded = player.y <= getSupport(player.x, player.z, player.y) + 0.05;
      if (camGrounded && camSpd > 1) player.bobPhase += camSpd * dt * 1.7;
      const bob = camGrounded ? Math.sin(player.bobPhase) * Math.min(0.045, camSpd * 0.005) : 0;
      player.landDip = Math.max(0, player.landDip - dt * 0.5);
      yaw.position.set(player.x, player.camH + player.y + bob - player.landDip * 0.4, player.z);
      const fovT = player.slideT > 0 ? 84 : keys["ShiftLeft"] && camSpd > 6 && !player.crouch ? 81 : player.prone ? 70 : 75;
      if (Math.abs(camera.fov - fovT) > 0.1) {
        camera.fov += (fovT - camera.fov) * Math.min(1, 9 * dt);
        camera.updateProjectionMatrix();
      }
      if (player.shakeT > 0) {
        camera.position.x += (Math.random() - 0.5) * player.shakeT * 0.15;
        camera.position.y += (Math.random() - 0.5) * player.shakeT * 0.12;
      }
      }

      // Shield-Regeneration (Halo-DNA) + Bloom-Decay
      if (gameTime - player.lastDmg > 3 && player.shield < 100 && player.hp > 0) {
        player.shield = Math.min(100, player.shield + 26 * dt);
      }
      player.bloom = Math.max(0, player.bloom - 2.2 * dt);
      player.meleeCd = Math.max(0, player.meleeCd - dt);
      player.nadeCd = Math.max(0, player.nadeCd - dt);
      player.dmgDirs = player.dmgDirs.filter((d) => gameTime - d.t < 0.8);
      for (let i = strikes.length - 1; i >= 0; i--) {
        if (gameTime >= strikes[i].at) {
          explode3(strikes[i].pos);
          strikes.splice(i, 1);
        }
      }
      player.rageT = Math.max(0, player.rageT - dt);
      player.spawnShield = Math.max(0, player.spawnShield - dt);
      musicTick(dt);
      dmgTick();
      nadesTick3(dt);
      pickupsTick(dt);
      flashLight.intensity = Math.max(0, flashLight.intensity - 30 * dt);

      // Dornen-Reflex-Aura
      if (player.upg.c2) {
        for (const b of bots) {
          if (b.alive && Math.hypot(b.group.position.x - player.x, b.group.position.z - player.z) < 1.6) {
            b.hp -= 6 * dt;
            if (b.hp <= 0) kill(0, b.id);
          }
        }
      }
      // Schwarm-Sinn / Markierungen
      for (const b of bots) {
        b.ghost.visible = !!player.upg.s1 && b.alive;
        const marked = (player.upg.s3 && b.alive && b.hp < 30) || (player.upg.s2 && b.markedT > gameTime);
        const hm = (b.group.children[1] as THREE.Mesh).material as THREE.MeshStandardMaterial;
        hm.emissive.setHex(marked ? 0xff2222 : b.color.getHex());
        hm.emissiveIntensity = marked ? 1.2 : 0.6;
      }

      // M10: Erinnerungen sammeln -> Twist -> Extraktion
      if (mode === "m10") {
        if (player.terminals < 4) {
          // Memories laufen ueber terminals-Zaehler
        }
        if (player.terminals >= 4 && !player.alarm) {
          player.alarm = true;
          pushFeed("🌅 DAS LIED STOPPT. Sie wissen, dass du es gehört hast.");
          sRadio(); player.shakeT = 0.8;
          for (let i = 0; i < 6; i++) makeBot(bots.length + i, 1);
        }
        if (player.alarm && Math.hypot(player.x - 0, player.z - 34) < 3.5) {
          fillEnd(); ended = true;
          setWinner("DER GARTEN HAT DICH GESEHEN");
          setScreen("end");
        }
      }
      // M6: Maren folgt/wartet + Extraktion
      if (mode === "m6" && maren.alive) {
        const mp = maren.group.position;
        if (maren.mode === "follow") {
          const dxm2 = player.x - mp.x, dzm2 = player.z - mp.z;
          const dm2 = Math.hypot(dxm2, dzm2);
          if (dm2 > 2.5) {
            const pp = { x: mp.x, z: mp.z };
            moveWithCollide(pp, (dxm2 / dm2) * 4.6 * dt, (dzm2 / dm2) * 4.6 * dt, 0.5, 0);
            mp.x = pp.x; mp.z = pp.z;
          }
          maren.group.rotation.y = Math.atan2(dxm2, dzm2);
        }
        if (Math.hypot(mp.x - 0, mp.z - 34) < 3.5 && Math.hypot(player.x - 0, player.z - 34) < 4.5) {
          fillEnd();
          ended = true;
          setWinner("EXFILTRATION MIT DR. MAREN");
          setScreen("end");
        }
      }
      // M8: Wahl + Zonen + Feuer-Timer
      if (mode === "m8") {
        player.rangeT -= dt; // Timer zweckentfremdet
        if (player.rangeT <= 80 && !player.alarm) {
          player.alarm = true;
          pushFeed("🔥 Das Labor brennt! Struktur kollabiert!");
          sRadio(); player.shakeT = 0.8;
        }
        if (player.rangeT <= 0 && !ended) {
          fillEnd(); ended = true; setFailed(true);
          setWinner("IM FEUER VERLOREN");
          setScreen("end");
        }
        if (!ended) {
          const chosen = loadStory().flags.save_vega_chosen;
          if (!m8Choice) {
            const nearCapsule = Math.hypot(player.x - -4, player.z - 0) < 1.8;
            const nearCore = Math.hypot(player.x - 4, player.z - 0) < 1.8;
            if (nearCapsule || nearCore) setM8Offer(nearCapsule ? "vega" : "data");
          } else if (m8ChoiceDone) {
            if (Math.hypot(player.x - 0, player.z - 34) < 3.5) {
              fillEnd(); ended = true;
              setWinner(m8Choice === "vega" ? "VEGA GERETTET" : "DATEN GESICHERT");
              setScreen("end");
            }
          }
        }
      }
      // m5: Terminals channeln + Extraktion
      if (mode === "m5" && mission) {
        const TERMS: [number, number][] = [[-6, 6], [6, -6], [0, -14]];
        const nextT = TERMS[player.terminals];
        if (nextT && player.terminals < 3) {
          if (Math.hypot(player.x - nextT[0], player.z - nextT[1]) < 1.6) {
            player.termProg += dt;
            if (player.termProg >= 1.5) {
              player.termProg = 0;
              player.terminals++;
              missionDestroyed = player.terminals;
              pushFeed(`⬇ Terminal ${player.terminals}/3 heruntergeladen`);
              sRadio();
            }
          } else player.termProg = 0;
        }
        if (player.terminals >= 3 && Math.hypot(player.x - 0, player.z - 34) < 3.5) {
          fillEnd();
          ended = true;
          setWinner("EXFILTRATION ERFOLGREICH");
          setScreen("end");
        }
      }
      const theBoss = bots.find((b) => b.isBoss);
      if (theBoss && !theBoss.alive && !ended) {
        fillEnd();
        ended = true;
        setWinner(theBoss.name + " ZERSTÖRT");
        setScreen("end");
      }
      storyTick();
      player.shakeT = Math.max(0, player.shakeT - dt);
      if (cine.active) cineTick(dt);
      if (!tactics && !player.bioOpen && !cine.active) {
        for (const b of bots) botTick(b, dt);
        for (const a of allies) allyTick(a, dt);
      }

      // Partikel & Tracer
      for (let i = particles.length - 1; i >= 0; i--) {
        const p = particles[i];
        p.life -= dt;
        p.vel.y -= 15 * dt;
        p.mesh.position.addScaledVector(p.vel, dt);
        if (p.mesh.position.y < 0.05) { p.mesh.position.y = 0.05; p.vel.set(0, 0, 0); }
        if (p.life <= 0) { scene.remove(p.mesh); particles.splice(i, 1); }
      }
      for (let i = tracers.length - 1; i >= 0; i--) {
        const t = tracers[i];
        t.life -= dt;
        (t.line.material as THREE.LineBasicMaterial).opacity = Math.max(0, t.life / 0.08);
        if (t.life <= 0) { scene.remove(t.line); t.line.geometry.dispose(); tracers.splice(i, 1); }
      }

      // Range-Timer
      if (mode === "range" && !ended) {
        player.rangeT -= dt;
        if (player.rangeT <= 0) {
          ended = true;
          fillEnd();
          const acc = player.shots > 0 ? Math.round((player.rangeHits / player.shots) * 100) : 0;
          addXp(player.rangeHits * 5 * xpMul);
          setWinner(`RANGE: ${player.rangeHits} Treffer · ${acc} % Genauigkeit`);
          setScreen("end");
        }
      }

      // Siegbedingung
      if (!ended) {
        if (mission) {
          if (mission.type === "kills" && missionKills >= mission.target) finishMission(true);
          else if (mission.type === "destroy" && mode !== "m5" && missionDestroyed >= mission.target) finishMission(true);
          else if (mission.type === "survive" && gameTime >= mission.target) finishMission(true);
          else if (mission.timeLimit && gameTime >= mission.timeLimit) finishMission(false);
        } else {
          const limit = mode === "ffa" ? 8 : 10;
          if (mode === "ffa") {
            const best = ffaScore.indexOf(Math.max(...ffaScore));
            if (ffaScore[best] >= limit) { ended = true; fillEnd(); setWinner(best === 0 ? "DU" : BOT_NAMES[best - 1]); setScreen("end"); }
          } else {
            if (teamScore[0] >= limit) { ended = true; fillEnd(); setWinner("TEAM GRÜN"); setScreen("end"); }
            if (teamScore[1] >= limit) { ended = true; fillEnd(); setWinner("TEAM ROT"); setScreen("end"); }
          }
        }
      }

      // HUD
      // Gegner-Footsteps (Stereo-Pan, Distanz) + Heartbeat
      for (const b of bots) {
        if (!b.alive) continue;
        const d = Math.hypot(b.group.position.x - player.x, b.group.position.z - player.z);
        if (d < 14) {
          const st = (b as unknown as { stepAcc?: number });
          st.stepAcc = (st.stepAcc ?? 0) + dt;
          if (st.stepAcc > 0.45) {
            st.stepAcc = 0;
            const rel = Math.atan2(b.group.position.x - player.x, b.group.position.z - player.z) - yaw.rotation.y - Math.PI;
            sStepPan(d, Math.sin(rel));
          }
        }
      }
      const heart = (player.hp < 30 && player.hp > 0 ? ((player as unknown as { heartAcc?: number }).heartAcc = ((player as unknown as { heartAcc?: number }).heartAcc ?? 0) + dt) : 0);
      if (player.hp < 30 && player.hp > 0 && heart > 1.0) {
        (player as unknown as { heartAcc?: number }).heartAcc = 0;
        sHeart();
      }
      {
        const arr = sporeGeo.attributes.position as THREE.BufferAttribute;
        for (let i = 0; i < arr.count; i++) {
          let y = arr.getY(i) + dt * 0.35;
          if (y > 8) y = 0;
          arr.setY(i, y);
          arr.setX(i, arr.getX(i) + Math.sin(gameTime * 0.7 + i) * dt * 0.15);
        }
        arr.needsUpdate = true;
      }
      rec.acc += dt;
      if (rec.acc >= 0.15) {
        rec.acc = 0;
        rec.frames.push([Math.round(gameTime * 10) / 10, Math.round(player.x * 100) / 100, Math.round(player.z * 100) / 100, Math.round(yaw.rotation.y * 1000) / 1000]);
      }
      fpsAcc += dt; fpsFrames++;
      if (fpsAcc >= 0.5) { fpsVal = Math.round(fpsFrames / fpsAcc); fpsAcc = 0; fpsFrames = 0; }
      hudAcc += dt;
      if (hudAcc > 0.12) {
        hudAcc = 0;
        while (feedExtraRef.current.length) pushFeed(feedExtraRef.current.shift()!);
        setHud({
          hp: Math.max(0, Math.round(player.hp)),
          ammo: player.ammo,
          reloading: player.reloading > 0,
          weapon: player.weapon === "brecher" ? "BRECHER-7" : "DORN",
          scores: mode === "ffa"
            ? ffaScore.map((v, i) => `${i === 0 ? "DU" : BOT_NAMES[i - 1]}:${v}`).join("  ")
            : `GRÜN ${teamScore[0]} : ${teamScore[1]} ROT`,
          feed: [...feedRef.current],
          kills: player.kills,
          m8Offer: mode === "m8" && !m8ChoiceDone ? m8OfferState : null,
          marenHp: mode === "m6" ? Math.max(0, Math.round(maren.hp)) : -1,
          objective: mode === "m8"
            ? `🔥 LABOR-BRAND · ⏱ ${Math.max(0, Math.ceil(player.rangeT))} s · ${m8ChoiceDone ? "Raus zur Extraktion (Norden)!" : "Kapsel (links) ODER Datenkern (rechts)?"}`
            : mode === "m6"
              ? `👩‍🔬 Eskortiere DR. MAREN zur Extraktion (Norden) · F = Befehl · Maren: ${Math.max(0, Math.round(maren.hp))} %`
            : mode === "range"
            ? `🎯 RANGE ·  ${Math.max(0, Math.ceil(player.rangeT))} s · Treffer ${player.rangeHits} · Shots ${player.shots}`
            : mode === "m5"
              ? player.alarm
                ? `🚨 ALARM · Terminals ${player.terminals}/3 · ${player.terminals >= 3 ? "RAUS ZUR EXTRAKTION (Norden)!" : "Sie kommen!"}`
                : `🤫 LEISE · Terminals ${player.terminals}/3 ${player.termProg > 0 ? "· LÄDT…" : ""} · ${player.terminals >= 3 ? "Extraktion: Norden!" : "Bleib ungesehen."}`
            : mission
            ? mission.type === "kills"
              ? `${mission.title} · Kills ${missionKills}/${mission.target}`
              : mission.type === "destroy"
                ? `${mission.title} · Strukturen ${missionDestroyed}/${mission.target} · ⏱ ${Math.max(0, (mission.timeLimit ?? 0) - gameTime).toFixed(0)} s`
                : `${mission.title} · Halte durch · ⏱ ${Math.max(0, mission.target - gameTime).toFixed(0)} s`
            : "",
          tactics,
          shield: Math.max(0, Math.round(player.shield)),
          bloom: player.bloom,
          grenades: player.grenades,
          dmgDirs: player.dmgDirs.map((d) => ({ a: d.a, age: gameTime - d.t })),
          codes: player.codes,
          upg: Object.keys(player.upg).filter((k) => player.upg[k]),
          bioOpen: player.bioOpen,
          announce: player.announce && gameTime - player.announce.t < 1.8 ? player.announce.text : null,
          skin: player.skinColor,
          fps: fpsVal,
          killcam: player.killcam ? (bots.find((b) => b.id === player.killcam!.botId)?.name ?? "?") : null,
          subtitle: hudSubtitle,
          lowhp: player.hp < 30 && player.hp > 0,
        });
      }

      renderer.render(scene, camera);
    };
    loop();

    const onResize = () => {
      renderer.setSize(mount.clientWidth, mount.clientHeight);
      if (composer) composer.setSize(mount.clientWidth, mount.clientHeight);
      camera.aspect = mount.clientWidth / mount.clientHeight;
      camera.updateProjectionMatrix();
    };
    window.addEventListener("resize", onResize);

    const buyUpg = (id: string) => {
      const def = UPG_DEFS.find((u) => u.id === id);
      if (!def || player.upg[id] || player.codes < def.cost) return;
      const prev = UPG_DEFS.find((u) => u.path === def.path && u.tier === def.tier - 1);
      if (prev && !player.upg[prev.id]) return;
      player.codes -= def.cost;
      player.upg[id] = true;
      sPickup();
      pushFeed(`🧬 ${def.name} integriert!`);
      if (id === "s1") for (const b of bots) b.ghost.visible = true;
    };

    apiRef.current = {
      upgrade: buyUpg,
      dispose: () => {
        cancelAnimationFrame(raf);
        window.removeEventListener("keydown", kd);
        window.removeEventListener("keyup", ku);
        window.removeEventListener("mouseup", mu);
        window.removeEventListener("mousemove", mm);
        window.removeEventListener("resize", onResize);
        el.removeEventListener("mousedown", md);
        el.removeEventListener("contextmenu", cm);
        if (document.pointerLockElement === el) document.exitPointerLock();
        renderer.dispose();
        if (el.parentElement === mount) mount.removeChild(el);
      },
    };
  };

  /* ================= UI ================= */
  const stats = typeof window !== "undefined" ? loadStats() : { kills: 0, headshots: 0, melees: 0, bestStreak: 0 };
  if (!introDone && screen === "menu") {
    const beats = [
      { t: "2041 // KORP TERRAFORMING DIVISION", s: "Sie nannten es „Saatgut“. Es war nie Saatgut." },
      { t: "SEKTOR 7 // QUARANTÄNE FEHLGESCHLAGEN", s: "Die Biomass singt. Und etwas unter ihr antwortet." },
      { t: "DEIN DROPSHIP // ABGESCHOSSEN", s: "Du bist nicht gelandet. Du bist gefallen." },
    ];
    return (
      <div
        className="min-h-screen bg-black flex flex-col items-center justify-center px-6 cursor-pointer"
        onClick={() => {
          if (introBeat < beats.length) setIntroBeat(introBeat + 1);
          else {
            try { localStorage.setItem("wirrwarr-intro", "1"); } catch { /* */ }
            setIntroDone(true);
          }
        }}
      >
        <p className="font-mono text-6xl md:text-8xl font-bold tracking-tight mb-10">
          <span className="text-primary glow-neon">WIRR</span><span className="text-foreground">WARR</span>
        </p>
        {introBeat < beats.length ? (
          <div key={introBeat} className="text-center animate-fade-in max-w-xl">
            <p className="font-mono text-[10px] tracking-[0.4em] uppercase text-primary mb-3">{beats[introBeat].t}</p>
            <p className="text-lg text-muted-foreground leading-relaxed">{beats[introBeat].s}</p>
          </div>
        ) : (
          <p className="font-mono text-sm text-primary tracking-[0.3em] uppercase animate-pulse-neon">[ Klicken zum Erwachen ]</p>
        )}
        <p className="absolute bottom-8 font-mono text-[9px] text-muted-foreground tracking-widest uppercase">Klick = weiter · Einmalig · Danach direkt ins Gefecht</p>
      </div>
    );
  }
  if (screen === "replay" && replayData) {
    return <ReplayView data={replayData} onExit={() => setScreen("menu")} />;
  }
  if (screen === "menu" || screen === "end") {
    return (
      <div className="min-h-screen bg-background text-foreground flex items-center justify-center px-6 py-16">
        <div className="max-w-2xl w-full">
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-3">
            WIRRWARR // Vertical Slice – echte 3D-Engine
          </p>
          <h1 className="text-4xl md:text-5xl font-bold mb-3">
            {screen === "end" ? (
              failed ? (
                <><span className="text-destructive">✖</span> <span className="text-destructive glow-neon-sm">{winner}</span></>
              ) : (
                <>Sieg: <span className="text-primary glow-neon">{winner}</span></>
              )
            ) : (
              <>Das <span className="text-primary glow-neon">echte Game</span> startet hier</>
            )}
          </h1>
          {screen === "end" && !failed && winner.startsWith("MISSION") && (
            <p className="font-mono text-xs text-accent tracking-wider uppercase mb-4">
              // Nächste Mission im Kampagnen-Block freigeschaltet
            </p>
          )}
          {screen === "end" && endInfoExt.current && (
            <div className="border border-border bg-card rounded-sm p-4 mb-6 text-left max-w-xl mx-auto">
              {endInfoExt.current.debrief && (
                <p className="text-sm text-muted-foreground italic leading-relaxed mb-3">„{endInfoExt.current.debrief}"</p>
              )}
              {endInfoExt.current.rank && (
                <p className="mb-2">
                  <span className={`font-mono text-3xl font-bold ${endInfoExt.current.rank === "S" ? "text-accent glow-neon" : endInfoExt.current.rank === "A" ? "text-primary" : "text-foreground"}`}>
                    {endInfoExt.current.rank}
                  </span>
                  <span className="font-mono text-[10px] text-muted-foreground tracking-wider uppercase ml-2">Rang</span>
                </p>
              )}
              <p className="font-mono text-[11px] text-foreground mb-2">
                K/D: {endInfoExt.current.kills}/{endInfoExt.current.deaths} · Headshots: {endInfoExt.current.hs} · Fragmente: {endInfoExt.current.frags ?? 0}/3
              </p>
              {replayRef.current && (
                <button
                  type="button"
                  onClick={() => {
                    const r = replayRef.current;
                    if (!r) return;
                    const blob = new Blob([JSON.stringify(r)], { type: "application/json" });
                    const a = document.createElement("a");
                    a.href = URL.createObjectURL(blob);
                    a.download = `wirrwarr-replay-${r.mode}-${Date.now()}.json`;
                    a.click();
                    URL.revokeObjectURL(a.href);
                  }}
                  className="font-mono text-[10px] text-primary border border-primary/50 bg-primary/10 hover:bg-primary/20 rounded-sm px-2 py-1 mb-2 min-h-[32px]"
                >
                  ⬇ Replay downloaden (JSON)
                </button>
              )}
              <div className="flex flex-wrap gap-2">
                {endInfoExt.current.medals.length > 0 ? (
                  endInfoExt.current.medals.map((m) => (
                    <span key={m} className="font-mono text-[10px] text-accent border border-accent/50 bg-accent/10 rounded-sm px-2 py-1">{m}</span>
                  ))
                ) : (
                  <span className="font-mono text-[10px] text-muted-foreground">Keine Medaillen dieses Mal.</span>
                )}
              </div>
            </div>
          )}
          <p className="text-muted-foreground mb-8 leading-relaxed">
            Three.js-Engine: echte 3D-Physik, Hitscan-Gunplay mit Tracern &amp; Partikeln,
            <span className="text-primary"> persistente 3D-Destruction</span> (BRECHER-7 reißt Löcher in die Arena),
            Bot-KI mit Line-of-Sight. Steuerung: <span className="font-mono text-foreground">Klick</span> = Maus fangen,{" "}
            <span className="font-mono text-foreground">WASD</span>, <span className="font-mono text-foreground">Shift</span> Sprint,{" "}
            <span className="font-mono text-foreground">C</span> Ducken (aus dem Sprint = <span className="text-primary">Slide</span>),{" "}
            <span className="font-mono text-foreground">X</span> Hinlegen, <span className="font-mono text-foreground">Space</span> Springen,{" "}
            <span className="font-mono text-foreground">Q/1/2/3</span> Waffen, <span className="font-mono text-foreground">G</span> Granate,{" "}
            <span className="font-mono text-foreground">V</span> Melee, <span className="font-mono text-foreground">R</span> laden,{" "}
            <span className="font-mono text-foreground">T</span> Taktik. Regenerierender Schild, Headshots, Waffen-Pickups – Halo-Feel.
          </p>
          {/* Arena-Wahl */}
          <div className="grid grid-cols-2 gap-3 mb-6">
            {(
              [
                ["sektor", "Sektor 7", "Barrikaden & Zentralblock"],
                ["garten", "Biomass-Garten", "Säulenring & Kuppel"],
              ] as [ArenaId, string, string][]
            ).map(([id, name, sub]) => (
              <button
                key={id}
                type="button"
                onClick={() => setArena(id)}
                aria-pressed={arena === id}
                className={`text-left rounded-sm border p-3 transition-all min-h-[44px] ${
                  arena === id ? "border-primary/70 bg-primary/10 box-glow-neon" : "border-border bg-card hover:border-primary/30"
                }`}
              >
                <p className={`font-bold text-sm ${arena === id ? "text-primary glow-neon-sm" : "text-foreground"}`}>{name}</p>
                <p className="font-mono text-[10px] text-muted-foreground">{sub}</p>
              </button>
            ))}
          </div>

          {/* Loadout / Progression */}
          <div className="border border-primary/40 bg-card rounded-sm p-4 mb-6 box-glow-neon">
            <div className="flex flex-wrap items-center justify-between gap-2 mb-2">
              <p className="font-mono text-xs tracking-[0.25em] uppercase text-primary glow-neon-sm">
                Loadout // Level {level}
              </p>
              <p className="font-mono text-[11px] text-muted-foreground">
                XP {profile.xp}{level < LEVEL_XP.length ? ` / nächstes Lv: ${LEVEL_XP[level]}` : " / MAX"}
              </p>
            </div>
            {level < LEVEL_XP.length && (
              <div className="w-full h-1.5 bg-secondary rounded-full overflow-hidden mb-4">
                <div className="h-full bg-primary rounded-full" style={{ width: `${Math.min(100, ((profile.xp - LEVEL_XP[level - 1]) / (LEVEL_XP[level] - LEVEL_XP[level - 1])) * 100)}%` }} />
              </div>
            )}
            <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-2">Killstreak-Slots (3/4/5/7/10er-Serie)</p>
            <div className="flex flex-wrap gap-2 mb-4">
              {KSTREAKS.map((ks) => {
                const sel = loadout.streaks.includes(ks.id);
                const locked = level < ks.level;
                return (
                  <button
                    key={ks.id}
                    type="button"
                    disabled={locked}
                    title={ks.desc}
                    onClick={() => {
                      setLoadout((lo) => {
                        const next = sel ? lo.streaks.filter((s) => s !== ks.id) : lo.streaks.length < 3 ? [...lo.streaks, ks.id] : lo.streaks;
                        const nl = { ...lo, streaks: next };
                        try { localStorage.setItem(LOAD_KEY, JSON.stringify(nl)); } catch { /* */ }
                        return nl;
                      });
                    }}
                    className={`text-left rounded-sm border px-3 py-2 transition-all min-h-[44px] ${
                      locked ? "opacity-40 cursor-not-allowed border-border/50" : sel ? "border-primary/70 bg-primary/15 box-glow-neon" : "border-border bg-secondary/40 hover:border-primary/40"
                    }`}
                  >
                    <p className={`font-bold text-xs ${sel ? "text-primary glow-neon-sm" : "text-foreground"}`}>
                      {locked ? "🔒 " : sel ? "✓ " : ""}{ks.num}er: {ks.name}
                    </p>
                    <p className="font-mono text-[9px] text-muted-foreground">{locked ? `ab Level ${ks.level}` : ks.desc}</p>
                  </button>
                );
              })}
            </div>
            <div className="flex flex-wrap items-center gap-x-6 gap-y-2">
              <div>
                <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-1.5">Charakter-Set</p>
                <div className="flex gap-2">
                  {SKINS.map((sk) => {
                    const locked = level < sk.level;
                    return (
                      <button
                        key={sk.id}
                        type="button"
                        disabled={locked}
                        title={locked ? `ab Level ${sk.level}` : sk.name}
                        onClick={() => {
                          setLoadout((lo) => {
                            const nl = { ...lo, skin: sk.id };
                            try { localStorage.setItem(LOAD_KEY, JSON.stringify(nl)); } catch { /* */ }
                            return nl;
                          });
                        }}
                        className={`w-8 h-8 rounded-sm border-2 transition-all ${locked ? "opacity-30 cursor-not-allowed" : "cursor-pointer"}`}
                        style={{ background: sk.color, borderColor: loadout.skin === sk.id ? "#fff" : "transparent" }}
                      />
                    );
                  })}
                </div>
              </div>
              <div>
                <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-1.5">Waffen-Unlocks</p>
                <p className="font-mono text-[11px] text-foreground">
                  DORN <span className="text-primary">Lv1</span> · BRECHER {level >= 2 ? <span className="text-primary">✓</span> : <span className="text-muted-foreground">🔒 Lv2</span>} · RICHTER {level >= 4 ? <span className="text-primary">✓</span> : <span className="text-muted-foreground">🔒 Lv4</span>}
                </p>
              </div>
            </div>
            {/* Attachments pro Waffe */}
            <div className="mt-4 space-y-2">
              <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground">Attachments (2 pro Waffe)</p>
              {(["dorn", "brecher", "richter"] as const).map((w) => (
                <div key={w} className="flex flex-wrap items-center gap-2">
                  <span className="font-mono text-[10px] text-foreground w-16 uppercase">{w}</span>
                  {ATTACHMENTS.map((at) => {
                    const cur = loadout.attach?.[w] ?? [];
                    const sel = cur.includes(at.id);
                    const locked = level < at.level || (level < WEAPON_LEVEL[w]);
                    return (
                      <button
                        key={at.id}
                        type="button"
                        disabled={locked}
                        title={`${at.desc}${level < at.level ? ` · ab Lv ${at.level}` : ""}`}
                        onClick={() => {
                          setLoadout((lo) => {
                            const have = lo.attach?.[w] ?? [];
                            const next = sel ? have.filter((x) => x !== at.id) : have.length < 2 ? [...have, at.id] : have;
                            const nl = { ...lo, attach: { ...lo.attach, [w]: next } };
                            try { localStorage.setItem(LOAD_KEY, JSON.stringify(nl)); } catch { /* */ }
                            return nl;
                          });
                        }}
                        className={`font-mono text-[10px] rounded-sm border px-2 py-1.5 transition-all min-h-[30px] ${
                          locked ? "opacity-40 cursor-not-allowed border-border/50 text-muted-foreground" : sel ? "border-primary/70 bg-primary/15 text-primary" : "border-border bg-secondary/40 text-secondary-foreground hover:border-primary/40"
                        }`}
                      >
                        {locked ? "🔒" : sel ? "✓" : "+"} {at.name}
                      </button>
                    );
                  })}
                </div>
              ))}
            </div>
          </div>

          {/* Daily Ops + Weekly Event */}
          {(() => {
            const d = loadDaily();
            const ev = weeklyEvent();
            return (
              <div className="border border-border bg-card rounded-sm p-4 mb-6">
                <div className="flex flex-wrap items-center justify-between gap-2 mb-3">
                  <p className="font-mono text-[10px] tracking-[0.25em] uppercase text-primary glow-neon-sm">📅 Daily Ops // {dayStr()}</p>
                  <p className="font-mono text-[10px] text-accent">🌐 Weekly-Event: {ev.name} – {ev.desc}</p>
                </div>
                <div className="grid sm:grid-cols-3 gap-2">
                  {dailyQuests().map((q) => {
                    const prog = Math.min(d.progress[q.metric] ?? 0, q.n);
                    const done = prog >= q.n;
                    const claimed = !!d.claimed[q.id];
                    return (
                      <div key={q.id} className={`rounded-sm border p-2.5 ${done ? "border-accent/60 bg-accent/10" : "border-border bg-secondary/30"}`}>
                        <p className="text-xs font-bold text-foreground mb-1">{q.text(q.n)}</p>
                        <div className="w-full h-1 bg-secondary rounded-full overflow-hidden mb-1.5">
                          <div className="h-full bg-primary rounded-full" style={{ width: `${(prog / q.n) * 100}%` }} />
                        </div>
                        <p className="font-mono text-[9px] text-muted-foreground mb-1">{prog}/{q.n} · +{q.xp} XP</p>
                        <button
                          type="button"
                          disabled={!done || claimed}
                          onClick={() => {
                            d.claimed[q.id] = true;
                            saveDaily(d);
                            setProfile((pr) => {
                              const np = { xp: pr.xp + q.xp };
                              try { localStorage.setItem(PROF_KEY, JSON.stringify(np)); } catch { /* */ }
                              return np;
                            });
                            setDailyTick((t) => t + 1);
                          }}
                          className={`font-mono text-[9px] rounded-sm border px-2 py-1 min-h-[26px] ${claimed ? "opacity-40 border-border text-muted-foreground" : done ? "border-accent/70 text-accent hover:bg-accent/10" : "border-border text-muted-foreground"}`}
                        >
                          {claimed ? "✓ CLAIMED" : done ? "CLAIM" : "OFFEN"}
                        </button>
                      </div>
                    );
                  })}
                </div>
              </div>
            );
          })()}

          {/* NG+ */}
          {MISSIONS.every((m) => doneMissions.includes(m.id)) ? (
            <div className="border border-accent/50 bg-accent/5 rounded-sm p-4 mb-6">
              <p className="font-mono text-[10px] tracking-[0.25em] uppercase text-accent glow-neon-sm mb-2">⭐ NG+ // Mutationen (Kampagne abgeschlossen)</p>
              <div className="grid sm:grid-cols-3 gap-2">
                {NG_MODS.map((mod) => {
                  const on = ngMods.includes(mod.id);
                  return (
                    <button
                      key={mod.id}
                      type="button"
                      onClick={() => {
                        setNgMods((cur) => {
                          const next = on ? cur.filter((x) => x !== mod.id) : [...cur, mod.id];
                          try { localStorage.setItem(NG_KEY, JSON.stringify({ mods: next })); } catch { /* */ }
                          return next;
                        });
                      }}
                      className={`text-left rounded-sm border p-2.5 transition-all min-h-[44px] ${on ? "border-accent/70 bg-accent/15" : "border-border bg-card hover:border-accent/40"}`}
                    >
                      <p className={`font-bold text-xs mb-0.5 ${on ? "text-accent" : "text-foreground"}`}>{on ? "☣ " : ""}{mod.name}</p>
                      <p className="font-mono text-[9px] text-muted-foreground">{mod.desc}</p>
                    </button>
                  );
                })}
              </div>
              <p className="font-mono text-[9px] text-muted-foreground mt-2 uppercase tracking-wider">Mutationen gelten für alle Modi. Eisen-Modus = nur Melee. Viel Glück.</p>
            </div>
          ) : (
            <p className="font-mono text-[10px] text-muted-foreground mb-6">⭐ NG+ // Schließe alle Missionen ab, um Mutationen freizuschalten.</p>
          )}

          {/* Kodex / Lore-Fragmente */}
          <div className="border border-border bg-card rounded-sm p-4 mb-6">
            <div className="flex flex-wrap items-center justify-between gap-2 mb-3">
              <p className="font-mono text-[10px] tracking-[0.25em] uppercase text-primary glow-neon-sm">📼 Kodex // Lore-Fragmente</p>
              <p className="font-mono text-[11px] text-muted-foreground">Gesamt: <span className="text-primary">{typeof window !== "undefined" ? loadFrags() : 0}</span></p>
            </div>
            <div className="grid sm:grid-cols-2 gap-2">
              {CODEX.map((c) => {
                const open = (typeof window !== "undefined" ? loadFrags() : 0) >= c.at;
                return (
                  <div key={c.at} className={`rounded-sm border p-2.5 ${open ? "border-primary/40 bg-primary/5" : "border-border/50 opacity-50"}`}>
                    <p className="font-mono text-[10px] text-foreground mb-0.5">{open ? "▸ " : "🔒 "}{c.title} <span className="text-muted-foreground">({c.at}◆)</span></p>
                    <p className="text-[11px] text-muted-foreground italic leading-relaxed">{open ? c.text : "Fragmente sammeln, um diesen Eintrag zu entschlüsseln."}</p>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Callsign */}
          <div className="flex flex-wrap items-center gap-2 mb-6">
            <span className="font-mono text-[10px] tracking-[0.25em] uppercase text-muted-foreground">Callsign:</span>
            <input
              value={callsign}
              maxLength={12}
              onChange={(e) => {
                const v = e.target.value.toUpperCase();
                setCallsign(v);
                try { localStorage.setItem(NAME_KEY, v); } catch { /* */ }
              }}
              placeholder="DEIN NAME"
              className="bg-black/60 border border-border rounded-sm px-3 py-2 font-mono text-sm text-primary outline-none focus:border-primary/60 min-h-[36px]"
            />
            <span className="font-mono text-[9px] text-muted-foreground">erscheint in Chat, Leaderboards & Kill-Feeds deiner Gegner</span>
          </div>

          {/* Stats */}
          <div className="border border-border bg-card rounded-sm px-4 py-3 mb-6 flex flex-wrap gap-x-6 gap-y-1">
            <p className="font-mono text-[11px] text-muted-foreground">🏆 DEINE STATS</p>
            <p className="font-mono text-[11px] text-foreground">Kills: <span className="text-primary">{stats.kills}</span></p>
            <p className="font-mono text-[11px] text-foreground">Headshots: <span className="text-primary">{stats.headshots}</span></p>
            <p className="font-mono text-[11px] text-foreground">Melee: <span className="text-primary">{stats.melees}</span></p>
            <p className="font-mono text-[11px] text-foreground">Beste Streak: <span className="text-primary">{stats.bestStreak}</span></p>
            {(() => {
              const r = loadRange();
              return (
                <>
                  <p className="font-mono text-[11px] text-foreground">Range-Accuracy: <span className="text-primary">{r.shots ? Math.round((r.hits / r.shots) * 100) : 0} %</span> (Best: {r.bestAcc} %)</p>
                  <p className="font-mono text-[11px] text-foreground">Range-Sessions: <span className="text-primary">{r.sessions}</span> · HS dort: <span className="text-primary">{r.hs}</span></p>
                </>
              );
            })()}
          </div>

          {/* Schwierigkeit */}
          <div className="flex flex-wrap items-center gap-2 mb-6">
            <span className="font-mono text-[10px] tracking-[0.25em] uppercase text-muted-foreground mr-1">Schwierigkeit:</span>
            {DIFFS.map((d) => (
              <button
                key={d.id}
                type="button"
                title={d.desc}
                onClick={() => { try { localStorage.setItem(DIFF_KEY, d.id); } catch { /* */ } setDailyTick((t) => t + 1); }}
                className={`font-mono text-[11px] tracking-wider uppercase rounded-sm border px-3 py-2 transition-all min-h-[36px] ${loadDiff() === d.id ? "border-primary/70 bg-primary/10 text-primary box-glow-neon" : "border-border bg-card text-muted-foreground hover:border-primary/30"}`}
              >
                {d.name}
              </button>
            ))}
          </div>

          {/* Performance */}
          <div className="flex flex-wrap items-center gap-2 mb-6">
            <span className="font-mono text-[10px] tracking-[0.25em] uppercase text-muted-foreground mr-1">Performance:</span>
            {(["low", "med", "high"] as const).map((q) => (
              <button
                key={q}
                type="button"
                onClick={() => setQuality(q)}
                aria-pressed={quality === q}
                className={`font-mono text-[11px] tracking-wider uppercase rounded-sm border px-3 py-2 transition-all min-h-[36px] ${
                  quality === q ? "border-primary/70 bg-primary/10 text-primary box-glow-neon" : "border-border bg-card text-muted-foreground hover:border-primary/30"
                }`}
              >
                {q === "low" ? "Low (240Hz)" : q === "med" ? "Med" : "High"}
              </button>
            ))}
          </div>

          {/* Arena */}
          <p className="font-mono text-[10px] tracking-[0.25em] uppercase text-muted-foreground mb-2">Arena</p>
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 mb-6">
            {(
              [
                ["sektor", "Sektor 7", "Barrikaden-Krieg"],
                ["garten", "Biomass-Garten", "Säulen & Kuppel"],
                ["stahl", "Stahlwiege", "Breschen-Paradies"],
                ["orbital", "Orbitaldock", "Container-Lanes"],
              ] as [ArenaId, string, string][]
            ).map(([id, nm, sub]) => (
              <button
                key={id}
                type="button"
                onClick={() => setArena(id)}
                aria-pressed={arena === id}
                className={`text-left rounded-sm border p-3 transition-all min-h-[44px] ${
                  arena === id ? "border-primary/70 bg-primary/10 box-glow-neon" : "border-border bg-card hover:border-primary/30"
                }`}
              >
                <p className={`font-bold text-sm ${arena === id ? "text-primary glow-neon-sm" : "text-foreground"}`}>{nm}</p>
                <p className="font-mono text-[10px] text-muted-foreground">{sub}</p>
              </button>
            ))}
          </div>

          {/* Perks */}
          <div className="grid grid-cols-3 gap-3 mb-6">
            {PERKS.map((pk) => (
              <button
                key={pk.id}
                type="button"
                onClick={() => setPerk(pk.id)}
                aria-pressed={perk === pk.id}
                className={`text-left rounded-sm border p-3 transition-all min-h-[44px] ${
                  perk === pk.id ? "border-primary/70 bg-primary/10 box-glow-neon" : "border-border bg-card hover:border-primary/30"
                }`}
              >
                <p className={`font-bold text-sm ${perk === pk.id ? "text-primary glow-neon-sm" : "text-foreground"}`}>{pk.name}</p>
                <p className="font-mono text-[10px] text-muted-foreground">{pk.desc}</p>
              </button>
            ))}
          </div>

          <div className="grid sm:grid-cols-2 gap-3 mb-8">
            <button
              type="button"
              onClick={() => start("tdm")}
              className="text-left border border-border bg-card rounded-sm p-4 hover:border-primary/60 hover:box-glow-neon transition-all min-h-[44px]"
            >
              <p className="font-bold text-foreground mb-1">Team-Deathmatch</p>
              <p className="font-mono text-[11px] text-muted-foreground">3v3 vs. Bots – 10 Kills.</p>
            </button>
            <button
              type="button"
              onClick={() => start("ffa")}
              className="text-left border border-border bg-card rounded-sm p-4 hover:border-primary/60 hover:box-glow-neon transition-all min-h-[44px]"
            >
              <p className="font-bold text-foreground mb-1">Frei für alle</p>
              <p className="font-mono text-[11px] text-muted-foreground">6 Kämpfer – 8 Kills.</p>
            </button>
            {typeof window !== "undefined" && localStorage.getItem("wirrwarr-lastreplay") && (
              <button
                type="button"
                onClick={() => {
                  try {
                    const r = JSON.parse(localStorage.getItem("wirrwarr-lastreplay")!);
                    setReplayData(r);
                    setScreen("replay");
                  } catch { /* */ }
                }}
                className="text-left border border-border bg-card rounded-sm p-4 hover:border-primary/60 transition-all min-h-[44px]"
              >
                <p className="font-bold text-foreground mb-1">🎥 Replay</p>
                <p className="font-mono text-[11px] text-muted-foreground">Letztes Match in der Engine abspielen.</p>
              </button>
            )}
            <button
              type="button"
              onClick={() => start("range")}
              className="text-left border border-accent/50 bg-accent/5 rounded-sm p-4 hover:bg-accent/10 transition-all min-h-[44px]"
            >
              <p className="font-bold text-accent mb-1">🎯 Aim-Range</p>
              <p className="font-mono text-[11px] text-muted-foreground">60 s Training: Treffer, Genauigkeit, XP.</p>
            </button>
          </div>

          {/* Kampagne */}
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-3">
            Kampagne // Biomass-Protokoll
          </p>
          <div className="grid sm:grid-cols-2 gap-3 mb-8">
            {MISSIONS.map((m, i) => {
              const done = doneMissions.includes(m.id);
              const unlocked = i === 0 || doneMissions.includes(MISSIONS[i - 1].id);
              return (
                <button
                  key={m.id}
                  type="button"
                  disabled={!unlocked}
                  onClick={() => start(m.id)}
                  className={`text-left rounded-sm border p-4 transition-all min-h-[44px] ${
                    !unlocked
                      ? "border-border/50 bg-card/40 opacity-40 cursor-not-allowed"
                      : done
                        ? "border-accent/50 bg-accent/5 hover:bg-accent/10"
                        : "border-primary/50 bg-primary/10 hover:bg-primary/20 box-glow-neon"
                  }`}
                >
                  <p className={`font-bold mb-1 ${done ? "text-accent" : unlocked ? "text-primary glow-neon-sm" : "text-muted-foreground"}`}>
                    {done ? "✅ " : unlocked ? "" : "🔒 "}{m.title}
                  </p>
                  <p className="font-mono text-[11px] text-muted-foreground leading-relaxed">{m.briefing}</p>
                </button>
              );
            })}
          </div>
          <a href="/" className="font-mono text-xs tracking-wider uppercase text-muted-foreground hover:text-primary transition-colors">
            ← Zurück zum GDD
          </a>
        </div>
      </div>
    );
  }

  return (
    <div className="relative h-screen w-full bg-black overflow-hidden select-none">
      <div ref={mountRef} className="w-full h-full" />
      {/* AAA-Look: Vignette + dezente Scanlines */}
      <div className="pointer-events-none absolute inset-0" style={{ background: "radial-gradient(ellipse at center, transparent 55%, rgba(0,0,0,0.55) 100%)" }} />
      <div className="pointer-events-none absolute inset-0" style={{ background: "repeating-linear-gradient(0deg, transparent, transparent 3px, rgba(34,255,85,0.02) 3px, rgba(34,255,85,0.02) 4px)" }} />
      {/* Crosshair mit Bloom */}
      <div className="pointer-events-none absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2">
        <div className="relative w-0 h-0">
          <div className="absolute rounded-full" style={{ width: 2, height: 8, left: -1, top: -(6 + hud.bloom * 16) - 8, background: hud.skin, boxShadow: `0 0 6px ${hud.skin}` }} />
          <div className="absolute rounded-full" style={{ width: 2, height: 8, left: -1, top: 6 + hud.bloom * 16, background: hud.skin, boxShadow: `0 0 6px ${hud.skin}` }} />
          <div className="absolute rounded-full" style={{ width: 8, height: 2, top: -1, left: -(6 + hud.bloom * 16) - 8, background: hud.skin, boxShadow: `0 0 6px ${hud.skin}` }} />
          <div className="absolute rounded-full" style={{ width: 8, height: 2, top: -1, left: 6 + hud.bloom * 16, background: hud.skin, boxShadow: `0 0 6px ${hud.skin}` }} />
          <div className="absolute rounded-full" style={{ width: 2, height: 2, left: -1, top: -1, background: hud.skin }} />
        </div>
      </div>
      {/* Damage-Richtungsanzeige */}
      {hud.dmgDirs.map((d, i) => (
        <div
          key={i}
          className="pointer-events-none absolute left-1/2 top-1/2"
          style={{ transform: `translate(-50%,-50%) rotate(${d.a}rad)`, opacity: Math.max(0, 1 - d.age / 0.8) }}
        >
          <div className="w-1.5 h-7 rounded-full bg-destructive" style={{ transform: "translateY(-52px)", boxShadow: "0 0 10px rgba(255,60,60,0.9)" }} />
        </div>
      ))}
      <div className="absolute top-3 left-1/2 -translate-x-1/2 text-center pointer-events-none max-w-[80%]">
        <p className="font-mono text-sm text-primary glow-neon-sm tracking-wider">{hud.scores}</p>
        {hud.objective && <p className="font-mono text-[10px] text-foreground/90 tracking-wider mt-1">{hud.objective}</p>}
      </div>
      {hud.lowhp && (
        <div className="absolute inset-0 pointer-events-none animate-pulse-neon" style={{ background: "radial-gradient(ellipse at center, transparent 55%, rgba(255,30,30,0.28) 100%)" }} />
      )}
      {/* Cinematic: Letterbox + Subtitles */}
      {hud.subtitle && (
        <>
          <div className="absolute inset-x-0 top-0 h-[9%] bg-black pointer-events-none" />
          <div className="absolute inset-x-0 bottom-0 h-[9%] bg-black pointer-events-none" />
          <div className="absolute inset-x-0 bottom-[11%] flex justify-center pointer-events-none px-6">
            <p className="text-center max-w-2xl">
              <span className="font-mono text-[10px] tracking-[0.3em] uppercase text-primary glow-neon-sm">{hud.subtitle.speaker}</span>
              <span className="block text-base md:text-lg text-foreground leading-relaxed">{hud.subtitle.text}</span>
            </p>
          </div>
          <p className="absolute top-[10%] right-4 font-mono text-[9px] text-muted-foreground tracking-wider uppercase pointer-events-none">Enter = überspringen</p>
        </>
      )}
      {hud.killcam && (
        <div className="absolute inset-x-0 bottom-24 flex justify-center pointer-events-none">
          <p className="font-mono text-sm tracking-[0.3em] uppercase text-destructive border border-destructive/50 bg-black/70 px-4 py-2 rounded-sm">
            ☠ KILLCAM // {hud.killcam}
          </p>
        </div>
      )}
      <div className="absolute top-3 right-3 text-right pointer-events-none">
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">FPS {hud.fps}</p>
      </div>
      {hud.announce && (
        <div className="absolute inset-x-0 top-[28%] flex justify-center pointer-events-none">
          <p className="font-mono text-4xl md:text-6xl font-bold text-primary glow-neon tracking-[0.25em] uppercase animate-pulse-neon">
            {hud.announce}
          </p>
        </div>
      )}
      {/* M8: DIE ENTSCHEIDUNG */}
      {hud.m8Offer && (
        <div className="absolute inset-0 flex items-center justify-center bg-black/60">
          <div className="border border-primary/50 bg-black/90 box-glow-neon rounded-sm p-6 max-w-md w-[92%] text-center">
            <p className="font-mono text-xs tracking-[0.3em] uppercase text-destructive animate-pulse-neon mb-2">Das Labor stirbt. Eine Hand voll Zeit.</p>
            <p className="text-lg text-foreground font-bold mb-4">Was trägst du raus?</p>
            <div className="grid grid-cols-2 gap-3">
              <button
                type="button"
                onClick={() => chooseM8Ref.current("vega")}
                className="border border-primary/60 bg-primary/10 hover:bg-primary/25 rounded-sm p-4 transition-all min-h-[44px]"
              >
                <p className="font-bold text-primary glow-neon-sm mb-1">❤ VEGA retten</p>
                <p className="font-mono text-[10px] text-muted-foreground">Der Kamerad. Die Stimme im Ohr.</p>
              </button>
              <button
                type="button"
                onClick={() => chooseM8Ref.current("data")}
                className="border border-border bg-secondary/40 hover:bg-secondary/70 rounded-sm p-4 transition-all min-h-[44px]"
              >
                <p className="font-bold text-foreground mb-1">📀 Daten sichern</p>
                <p className="font-mono text-[10px] text-muted-foreground">Die Wahrheit. Der Beweis gegen KORP.</p>
              </button>
            </div>
            <p className="font-mono text-[9px] text-muted-foreground mt-3 uppercase tracking-wider">Diese Entscheidung ist permanent.</p>
          </div>
        </div>
      )}
      {hud.bioOpen && (
        <div className="absolute inset-0 flex items-center justify-center bg-black/70 pointer-events-auto">
          <div className="border border-primary/50 bg-black/90 box-glow-neon rounded-sm p-6 max-w-2xl w-[92%]">
            <div className="flex items-center justify-between mb-4">
              <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm">
                🧬 Biomass-Integration // Zeit angehalten
              </p>
              <p className="font-mono text-sm text-primary">Codes: {hud.codes}</p>
            </div>
            <div className="grid grid-cols-3 gap-4">
              {[
                ["sinne", "Sinne"],
                ["carapax", "Carapax"],
                ["fort", "Fortbewegung"],
              ].map(([pid, pname]) => (
                <div key={pid}>
                  <p className="font-mono text-[10px] tracking-[0.2em] uppercase text-muted-foreground mb-2">{pname}</p>
                  <div className="space-y-2">
                    {UPG_DEFS.filter((u) => u.path === pid).map((u) => {
                      const owned = hud.upg.includes(u.id);
                      const prev = UPG_DEFS.find((x) => x.path === pid && x.tier === u.tier - 1);
                      const unlocked = !prev || hud.upg.includes(prev.id);
                      const affordable = hud.codes >= u.cost;
                      return (
                        <button
                          key={u.id}
                          type="button"
                          disabled={owned || !unlocked || !affordable}
                          onClick={() => apiRef.current?.upgrade?.(u.id)}
                          className={`w-full text-left rounded-sm border p-2.5 transition-all min-h-[44px] ${
                            owned
                              ? "border-accent/60 bg-accent/10"
                              : unlocked && affordable
                                ? "border-primary/50 bg-primary/10 hover:bg-primary/20 cursor-pointer"
                                : "border-border/50 opacity-40 cursor-not-allowed"
                          }`}
                        >
                          <p className={`font-bold text-xs ${owned ? "text-accent" : "text-foreground"}`}>
                            {owned ? "✓ " : ""}{u.name}
                          </p>
                          <p className="font-mono text-[9px] text-muted-foreground leading-relaxed">{u.desc}</p>
                          <p className="font-mono text-[9px] text-primary mt-0.5">{u.cost} Code{u.cost > 1 ? "s" : ""}</p>
                        </button>
                      );
                    })}
                  </div>
                </div>
              ))}
            </div>
            <p className="font-mono text-[10px] text-muted-foreground mt-4">B = schließen · Codes gibt es pro Kill</p>
          </div>
        </div>
      )}
      {hud.tactics && (
        <div className="absolute inset-x-0 top-16 flex justify-center pointer-events-none">
          <div className="border border-primary/60 bg-black/70 box-glow-neon rounded-sm px-4 py-2.5 text-center">
            <p className="font-mono text-xs text-primary tracking-[0.25em] uppercase animate-pulse-neon">🧠 Taktik // Zeit eingefroren</p>
            <p className="font-mono text-[10px] text-muted-foreground mt-1">Klick = Waypoint · 1 Folgen · 2 Halten · 3 Angreifen · T = Beenden</p>
          </div>
        </div>
      )}
      <div className="absolute top-12 left-3 pointer-events-none space-y-1">
        {hud.feed.map((f, i) => (
          <p key={i} className="font-mono text-[10px] text-primary/90">{f}</p>
        ))}
      </div>
      <div className="absolute bottom-3 left-3 pointer-events-none">
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">SCHILD // INTEGRITÄT</p>
        <div className="w-40 h-1 rounded-full overflow-hidden mt-1 bg-secondary">
          <div className="h-full rounded-full transition-all duration-150" style={{ width: `${hud.shield}%`, background: "#33ccff", boxShadow: "0 0 8px rgba(51,204,255,0.8)" }} />
        </div>
        <div className="w-40 h-1.5 bg-secondary rounded-full overflow-hidden mt-1">
          <div className={`h-full rounded-full ${hud.hp > 35 ? "bg-primary" : "bg-destructive"}`} style={{ width: `${hud.hp}%` }} />
        </div>
        <p className="font-mono text-xl leading-none mt-1">
          <span className="text-[#33ccff]">{hud.shield}</span>
          <span className="text-muted-foreground text-sm"> + </span>
          <span className="text-primary">{hud.hp}</span>
        </p>
      </div>
      <div className="absolute bottom-3 right-3 text-right pointer-events-none">
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">
          {hud.weapon} <span className="text-primary">[Q]</span> · 💣 {hud.grenades} <span className="text-primary">[G]</span> · 👊 <span className="text-primary">[V]</span>
        </p>
        <p className="font-mono text-2xl text-foreground leading-none">
          {hud.reloading ? <span className="text-primary">LÄDT…</span> : <>{hud.ammo}<span className="text-muted-foreground text-sm">/∞</span></>}
        </p>
      </div>
      <button
        type="button"
        onClick={() => setScreen("menu")}
        className="absolute top-3 left-3 font-mono text-[10px] tracking-wider uppercase text-muted-foreground hover:text-primary border border-border bg-black/50 rounded-sm px-2.5 py-1.5 min-h-[32px]"
      >
        ✕ Verlassen
      </button>
    </div>
  );
}
