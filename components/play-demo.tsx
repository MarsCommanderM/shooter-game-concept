"use client";

import { useEffect, useRef, useState } from "react";

/* ================================================================== */
/* WIRRWARR Web-Demo v0.2: Raycaster-FPS                               */
/* Bots, 6 Modi, DESTRUCTION (BRECHER), Sounds, 2 Maps, Touch-Controls */
/* ================================================================== */

type ModeId = "tdm" | "ffa" | "hq" | "sabotage" | "ctf" | "domination";
type MapId = "spire" | "garten" | "stahl" | "orbital";

const MODE_INFO: { id: ModeId; name: string; desc: string }[] = [
  { id: "tdm", name: "Team-Deathmatch", desc: "3v3 – erstes Team mit 10 Kills gewinnt." },
  { id: "ffa", name: "Frei für alle", desc: "6 Kämpfer, jeder allein – 8 Kills gewinnen." },
  { id: "hq", name: "Hauptquartier", desc: "HQ in der Mitte halten: 1 Punkt/Sekunde, 60 Punkte gewinnen." },
  { id: "sabotage", name: "Sabotage", desc: "Du bist Angreifer: Site mit E sprengen (4 s halten). Bots entschärfen. 2 Runden-Siege." },
  { id: "ctf", name: "Capture the Flag", desc: "Rote Flagge (Südost) klauen, zur grünen Basis (Nordwest) bringen. 2 Caps gewinnen." },
  { id: "domination", name: "Herrschaft", desc: "Zonen A/B/C halten: 1 Punkt/Sekunde pro Zone, 60 Punkte gewinnen." },
];

const MAP_INFO: { id: MapId; name: string; desc: string }[] = [
  { id: "spire", name: "Sektor 7", desc: "Vier Blöcke, vier Barrikaden – alles sprengbar." },
  { id: "garten", name: "Biomass-Garten", desc: "Offene Kuppeln mit Säulenwald, mehr Sightlines." },
  { id: "stahl", name: "Stahlwiege", desc: "Fabrikhallen mit dünnen Doppelwänden – Brecher-Paradies." },
  { id: "orbital", name: "Orbitaldock", desc: "Offene Dockflächen mit Containerzeilen und langen Sichtlinien." },
];

interface Bot {
  id: number; name: string; team: number;
  x: number; y: number; hp: number; alive: boolean;
  respawnAt: number; cd: number; color: string; kills: number; deaths: number;
  strafeDir: number; strafeT: number; nadeCd: number; breachCd: number;
}

interface Nad { x: number; y: number; vx: number; vy: number; t: number; killer: number; }
interface Pickup { x: number; y: number; active: boolean; respawnAt: number; }
interface ScoreRow { name: string; team: number; kills: number; deaths: number; isPlayer: boolean; }
interface DmgNum { v: number; t: number; kill: boolean; }

interface HudState {
  hp: number; ammo: number; reloading: boolean; weapon: string; grenades: number;
  scores: string; objective: string; feed: string[];
  winner: string | null; planting: boolean;
  timeLeft: string; overtime: boolean; tab: boolean; scoreboard: ScoreRow[];
  split: boolean; p2hp: number; p2ammo: number;
}

type Records = Partial<Record<ModeId, { wins: number; bestKills: number }>>;
const REC_KEY = "wirrwarr-demo-records";
function loadRecords(): Records {
  try {
    const raw = localStorage.getItem(REC_KEY);
    return raw ? (JSON.parse(raw) as Records) : {};
  } catch {
    return {};
  }
}

const SIZE = 24;
const TEAM_COLORS = ["#22ff55", "#ff5544", "#ffcc33", "#33ccff", "#ff66cc", "#99ff33"];
const BOT_NAMES = ["VEGA", "JUNO", "RAZOR", "MORBID", "ECHO", "SABLE"];
const BOT_DMG = [
  { name: "Rekrut", dmgMul: 0.55, cdMul: 1.5, speed: 1.8 },
  { name: "Veteran", dmgMul: 1, cdMul: 1, speed: 2.2 },
  { name: "Apex", dmgMul: 1.5, cdMul: 0.65, speed: 2.7 },
];
const SKINS = [
  { id: "green", name: "Toxin-Grün", color: "#22ff55", wins: 0 },
  { id: "cyan", name: "Echo-Cyan", color: "#33ccff", wins: 1 },
  { id: "amber", name: "Funken-Amber", color: "#ffcc33", wins: 3 },
  { id: "pink", name: "Myzel-Pink", color: "#ff66cc", wins: 6 },
  { id: "white", name: "Apex-Weiß", color: "#ffffff", wins: 10 },
];

/* Zellen: 0 frei, 1 massiv, 2 intakt sprengbar, 3 angeknackst sprengbar */
function buildMap(id: MapId): number[][] {
  const m: number[][] = Array.from({ length: SIZE }, () => Array(SIZE).fill(0));
  for (let i = 0; i < SIZE; i++) { m[0][i] = 1; m[SIZE - 1][i] = 1; m[i][0] = 1; m[i][SIZE - 1] = 1; }
  const rect = (x: number, y: number, w: number, h: number, v: number) => {
    for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) m[j][i] = v;
  };
  if (id === "spire") {
    rect(4, 4, 2, 2, 2); rect(18, 4, 2, 2, 2); rect(4, 18, 2, 2, 2); rect(18, 18, 2, 2, 2);
    rect(11, 4, 2, 2, 2); rect(11, 18, 2, 2, 2); rect(4, 11, 2, 2, 2); rect(18, 11, 2, 2, 2);
  } else if (id === "garten") {
    rect(6, 6, 1, 1, 2); rect(12, 5, 1, 1, 2); rect(17, 6, 1, 1, 2);
    rect(5, 12, 1, 1, 2); rect(18, 12, 1, 1, 2);
    rect(6, 17, 1, 1, 2); rect(12, 18, 1, 1, 2); rect(17, 17, 1, 1, 2);
    rect(11, 11, 2, 2, 1);
  } else if (id === "stahl") {
    // Stahlwiege: drei Hallen, dünne sprengbare Doppelwände
    rect(8, 4, 1, 7, 2); rect(15, 4, 1, 7, 2);
    rect(8, 13, 1, 7, 2); rect(15, 13, 1, 7, 2);
    rect(4, 11, 3, 1, 2); rect(17, 11, 3, 1, 2);
    rect(11, 11, 2, 1, 1);
  } else {
    // Orbitaldock: Containerzeilen, lange Lanes
    rect(6, 6, 4, 1, 2); rect(14, 6, 4, 1, 2);
    rect(6, 17, 4, 1, 2); rect(14, 17, 4, 1, 2);
    rect(11, 9, 2, 1, 2); rect(11, 14, 2, 1, 2);
    rect(4, 11, 1, 2, 1); rect(19, 11, 1, 2, 1);
  }
  return m;
}

const SPAWNS: [number, number][] = [
  [2.5, 12.5], [21.5, 12.5], [2.5, 2.5], [21.5, 21.5], [12.5, 2.5], [12.5, 21.5],
];
const DOM_POINTS = [
  { x: 7.5, y: 7.5, label: "A" },
  { x: 12.5, y: 12.5, label: "B" },
  { x: 16.5, y: 16.5, label: "C" },
];

interface Particle { x: number; y: number; vx: number; vy: number; life: number; color: string; }
interface P2State { x: number; y: number; a: number; hp: number; ammo: number; reloading: number; fireCd: number; respawnAt: number; kills: number; deaths: number; hurt: number; }

interface GameState {
  split: boolean; p2: P2State;
  mode: ModeId; map: number[][]; diff: number; skinColor: string;
  particles: Particle[];
  px: number; py: number; pa: number;
  hp: number; ammo: number; reloading: number; flash: number; fireCd: number;
  weapon: "dorn" | "brecher" | "richter"; pRespawnAt: number;
  playerKills: number; playerDeaths: number; grenades: number; nades: Nad[];
  pickups: Pickup[];
  hitMarker: number; killConfirm: number; hurt: number;
  dmgNums: DmgNum[]; recorded: boolean;
  matchTime: number; overtime: boolean;
  bots: Bot[]; keys: Record<string, boolean>; mouseDown: boolean;
  touchVec: { x: number; y: number };
  teamScore: number[]; ffaScore: number[]; domOwner: number[]; hqOwner: number;
  flags: { owner: number; x: number; y: number; carried: boolean; carrierId: number | null }[];
  plantProgress: number; bombPlanted: boolean; bombTimer: number; defuseProgress: number;
  roundWins: number[]; feed: { text: string; t: number }[]; time: number; winner: string | null;
}

function makeGame(mode: ModeId, mapId: MapId, diff: number, skinColor: string): GameState {
  const bots: Bot[] = [];
  for (let i = 0; i < 5; i++) {
    const team = mode === "ffa" ? i + 1 : i % 2 === 0 ? 1 : 0;
    const sp = SPAWNS[(i + 1) % SPAWNS.length];
    bots.push({
      id: i + 1, name: BOT_NAMES[i], team, x: sp[0], y: sp[1],
      hp: 100, alive: true, respawnAt: 0, cd: 1 + Math.random(),
      color: TEAM_COLORS[mode === "ffa" ? i + 1 : team], kills: 0, deaths: 0,
      strafeDir: 1, strafeT: 0, nadeCd: 6 + Math.random() * 6, breachCd: 3,
    });
  }
  return {
    mode, map: buildMap(mapId), diff, skinColor,
    split: false,
    p2: { x: SPAWNS[1][0], y: SPAWNS[1][1], a: Math.PI, hp: 100, ammo: 24, reloading: 0, fireCd: 0, respawnAt: 0, kills: 0, deaths: 0, hurt: 0 },
    particles: [],
    px: SPAWNS[0][0], py: SPAWNS[0][1], pa: 0,
    hp: 100, ammo: 24, reloading: 0, flash: 0, fireCd: 0,
    weapon: "dorn", pRespawnAt: 0,
    playerKills: 0, playerDeaths: 0, grenades: 3, nades: [],
    pickups: [
      { x: 7.5, y: 16.5, active: true, respawnAt: 0 },
      { x: 16.5, y: 7.5, active: true, respawnAt: 0 },
    ],
    hitMarker: 0, killConfirm: 0, hurt: 0,
    dmgNums: [], recorded: false,
    matchTime: mode === "sabotage" ? 999 : 240, overtime: false,
    bots, keys: {}, mouseDown: false, touchVec: { x: 0, y: 0 },
    teamScore: [0, 0], ffaScore: Array(6).fill(0), domOwner: [-1, -1, -1], hqOwner: -1,
    flags: [
      { owner: 0, x: 2.5, y: 2.5, carried: false, carrierId: null },
      { owner: 1, x: 21.5, y: 21.5, carried: false, carrierId: null },
    ],
    plantProgress: 0, bombPlanted: false, bombTimer: 0, defuseProgress: 0,
    roundWins: [0, 0], feed: [], time: 0, winner: null,
  };
}

/* ---------------- WebAudio-Sounds ---------------- */
let actx: AudioContext | null = null;
function ac(): AudioContext | null {
  if (typeof window === "undefined") return null;
  const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
  if (!AC) return null;
  if (!actx) actx = new AC();
  if (actx.state === "suspended") void actx.resume();
  return actx;
}
function sShot(weapon: string, dist = 0, pan = 0) {
  const c = ac(); if (!c) return;
  const vol = Math.max(0.05, 1 - dist / 18);
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  let out: AudioNode = g;
  try {
    const p = c.createStereoPanner();
    p.pan.value = Math.max(-1, Math.min(1, pan));
    g.connect(p); out = p;
  } catch { /* Mono-Fallback */ }
  out.connect(c.destination);
  if (weapon === "brecher") {
    o.type = "square"; o.frequency.setValueAtTime(130, t); o.frequency.exponentialRampToValueAtTime(35, t + 0.18);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.3 * vol, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.2);
    o.connect(g); o.start(t); o.stop(t + 0.22);
  } else {
    o.type = "triangle"; o.frequency.setValueAtTime(720, t); o.frequency.exponentialRampToValueAtTime(180, t + 0.07);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.16 * vol, t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.08);
    o.connect(g); o.start(t); o.stop(t + 0.1);
  }
}
function sBoom() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const len = Math.floor(c.sampleRate * 0.5);
  const buf = c.createBuffer(1, len, c.sampleRate);
  const d = buf.getChannelData(0);
  for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / len);
  const src = c.createBufferSource(); src.buffer = buf;
  const f = c.createBiquadFilter(); f.type = "lowpass"; f.frequency.setValueAtTime(900, t); f.frequency.exponentialRampToValueAtTime(80, t + 0.5);
  const g = c.createGain(); g.gain.setValueAtTime(0.5, t); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.5);
  src.connect(f).connect(g).connect(c.destination); src.start(t);
  const o = c.createOscillator(); const og = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(90, t); o.frequency.exponentialRampToValueAtTime(28, t + 0.4);
  og.gain.setValueAtTime(0.4, t); og.gain.exponentialRampToValueAtTime(0.0001, t + 0.45);
  o.connect(og).connect(c.destination); o.start(t); o.stop(t + 0.5);
}
function sHit() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(1200, t); o.frequency.exponentialRampToValueAtTime(600, t + 0.05);
  g.gain.setValueAtTime(0.08, t); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.06);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.07);
}

/* ================================================================== */

export function PlayDemo() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const stateRef = useRef<GameState | null>(null);
  const [screen, setScreen] = useState<"menu" | "game" | "end">("menu");
  const [mapId, setMapId] = useState<MapId>("spire");
  const [diff, setDiff] = useState(1);
  const [skin, setSkin] = useState(() => {
    try { return localStorage.getItem("wirrwarr-demo-skin") ?? "green"; } catch { return "green"; }
  });
  const [hud, setHud] = useState<HudState>({
    hp: 100, ammo: 24, reloading: false, weapon: "DORN", grenades: 3, scores: "", objective: "",
    feed: [], winner: null, planting: false,
    timeLeft: "", overtime: false, tab: false, scoreboard: [],
    split: false, p2hp: 100, p2ammo: 24,
  });
  const [records, setRecords] = useState<Records>({});
  useEffect(() => {
    if (screen !== "game") setRecords(loadRecords());
  }, [screen]);

  const start = (mode: ModeId) => {
    const skinColor = SKINS.find((s) => s.id === skin)?.color ?? "#22ff55";
    stateRef.current = makeGame(mode, mapId, diff, skinColor);
    setHud((h) => ({ ...h, winner: null }));
    setScreen("game");
  };

  const startSplit = () => {
    const skinColor = SKINS.find((s) => s.id === skin)?.color ?? "#22ff55";
    const st = makeGame("tdm", mapId, diff, skinColor);
    st.split = true;
    st.bots = [];
    st.matchTime = 300;
    stateRef.current = st;
    setHud((h) => ({ ...h, winner: null }));
    setScreen("game");
  };

  useEffect(() => {
    if (screen !== "game") return;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const W = 320, H = 180;
    canvas.width = W; canvas.height = H;
    const s = () => stateRef.current!;
    let raf = 0;
    let last = performance.now();
    let lastMouseX = 0;

    const isWall = (x: number, y: number) => {
      const g = s();
      const cx = Math.floor(x), cy = Math.floor(y);
      if (cx < 0 || cy < 0 || cx >= SIZE || cy >= SIZE) return true;
      return g.map[cy][cx] > 0;
    };
    const los = (x0: number, y0: number, x1: number, y1: number) => {
      const dx = x1 - x0, dy = y1 - y0;
      const steps = Math.ceil(Math.hypot(dx, dy) * 4);
      for (let i = 1; i < steps; i++) {
        const t = i / steps;
        if (isWall(x0 + dx * t, y0 + dy * t)) return false;
      }
      return true;
    };

    /* ---------------- Input ---------------- */
    const kd = (e: KeyboardEvent) => {
      if (e.key === "Tab") e.preventDefault();
      const k = e.key.toLowerCase();
      s().keys[k] = true;
      if (k === "q" || k === "1" || k === "2") {
        const g = s();
        g.weapon = k === "q" ? (g.weapon === "dorn" ? "brecher" : "dorn") : k === "1" ? "dorn" : "brecher";
        g.ammo = 24; g.reloading = 0;
      }
      if (k === "g") {
        const g = s();
        if (g.grenades > 0 && g.nades.length < 3) {
          g.grenades--;
          g.nades.push({
            x: g.px, y: g.py,
            vx: Math.cos(g.pa) * 7, vy: Math.sin(g.pa) * 7,
            t: 1.1, killer: 0,
          });
        }
      }
    };
    const ku = (e: KeyboardEvent) => { s().keys[e.key.toLowerCase()] = false; };
    const md = () => {
      s().mouseDown = true;
      try { canvas.requestPointerLock?.(); } catch { /* ignore */ }
    };
    const mu = () => { s().mouseDown = false; };
    const mm = (e: MouseEvent) => {
      if (document.pointerLockElement === canvas) s().pa += e.movementX * 0.0025;
      else if (s().mouseDown) s().pa += (e.clientX - lastMouseX) * 0.004;
      lastMouseX = e.clientX;
    };

    /* Touch: linke Hälfte = bewegen, rechte Hälfte = schauen */
    let moveId: number | null = null, lookId: number | null = null;
    let lookX = 0, moveOX = 0, moveOY = 0;
    const ts = (e: TouchEvent) => {
      e.preventDefault();
      const r = canvas.getBoundingClientRect();
      for (const t of Array.from(e.changedTouches)) {
        if (t.clientX - r.left < r.width / 2 && moveId === null) {
          moveId = t.identifier; moveOX = t.clientX; moveOY = t.clientY;
        } else if (lookId === null) {
          lookId = t.identifier; lookX = t.clientX;
        }
      }
    };
    const tm = (e: TouchEvent) => {
      e.preventDefault();
      const g = s();
      for (const t of Array.from(e.changedTouches)) {
        if (t.identifier === moveId) {
          g.touchVec.x = Math.max(-1, Math.min(1, (t.clientX - moveOX) / 50));
          g.touchVec.y = Math.max(-1, Math.min(1, (t.clientY - moveOY) / 50));
        } else if (t.identifier === lookId) {
          g.pa += (t.clientX - lookX) * 0.006;
          lookX = t.clientX;
        }
      }
    };
    const te = (e: TouchEvent) => {
      for (const t of Array.from(e.changedTouches)) {
        if (t.identifier === moveId) { moveId = null; s().touchVec = { x: 0, y: 0 }; }
        if (t.identifier === lookId) lookId = null;
      }
    };

    window.addEventListener("keydown", kd);
    window.addEventListener("keyup", ku);
    canvas.addEventListener("mousedown", md);
    window.addEventListener("mouseup", mu);
    window.addEventListener("mousemove", mm);
    canvas.addEventListener("touchstart", ts, { passive: false });
    canvas.addEventListener("touchmove", tm, { passive: false });
    canvas.addEventListener("touchend", te);

    /* ---------------- Helpers ---------------- */
    const moveEntity = (e: { x: number; y: number }, dx: number, dy: number) => {
      const r = 0.25;
      if (!isWall(e.x + dx + Math.sign(dx) * r, e.y)) e.x += dx;
      if (!isWall(e.x, e.y + dy + Math.sign(dy) * r)) e.y += dy;
    };
    const addFeed = (text: string) => {
      const g = s();
      g.feed.push({ text, t: g.time });
      if (g.feed.length > 5) g.feed.shift();
    };
    const nameOf = (id: number) => {
      const g = s();
      if (id === 0) return g.split ? "P1" : "DU";
      if (id === 99) return "P2";
      return g.bots.find((b) => b.id === id)?.name ?? "?";
    };
    const kill = (killerId: number, victimId: number) => {
      const g = s();
      const victim = victimId === 0 || victimId === 99 ? null : g.bots.find((b) => b.id === victimId);
      const killer = killerId === 0 || killerId === 99 ? null : g.bots.find((b) => b.id === killerId);
      if (victim) { victim.hp = 0; victim.alive = false; victim.deaths++; victim.respawnAt = g.time + 3; }
      if (victimId === 0) { g.hp = 0; g.playerDeaths++; }
      if (victimId === 99) { g.p2.hp = 0; g.p2.deaths++; g.p2.respawnAt = g.time + 3; }
      if (killer) killer.kills++;
      if (killerId === 0) { g.playerKills++; g.killConfirm = 0.25; }
      if (killerId === 99) g.p2.kills++;
      if (g.mode === "ffa") { if (killerId !== 99) g.ffaScore[killerId]++; }
      else g.teamScore[killerId === 0 ? 0 : killerId === 99 ? 1 : killer!.team]++;
      addFeed(`${nameOf(killerId)} ⚡ ${nameOf(victimId)}`);
    };
    const spawnAt = (id: number) => {
      const g = s();
      const sp = SPAWNS[id === 99 ? 1 : id % SPAWNS.length];
      if (id === 0) { g.px = sp[0]; g.py = sp[1]; g.hp = 100; }
      else if (id === 99) { g.p2.x = sp[0]; g.p2.y = sp[1]; g.p2.a = Math.PI; g.p2.hp = 100; g.p2.ammo = 24; }
      else {
        const b = g.bots.find((x) => x.id === id)!;
        b.x = sp[0]; b.y = sp[1]; b.hp = 100; b.alive = true;
      }
    };
    const nearTeam = (x: number, y: number, r: number, team: number) => {
      const g = s();
      if (team === 0 && g.hp > 0 && Math.hypot(g.px - x, g.py - y) < r) return true;
      if (team === 1 && g.split && g.p2.hp > 0 && Math.hypot(g.p2.x - x, g.p2.y - y) < r) return true;
      return g.bots.some((b) => b.alive && b.team === team && Math.hypot(b.x - x, b.y - y) < r);
    };

    /* Wand im Fadenkreuz finden (DDA) */
    const wallInSight = (range: number): { cx: number; cy: number; dist: number; v: number } | null => {
      const g = s();
      const rdx = Math.cos(g.pa), rdy = Math.sin(g.pa);
      let mapX = Math.floor(g.px), mapY = Math.floor(g.py);
      const dDX = Math.abs(1 / (rdx || 1e-9)), dDY = Math.abs(1 / (rdy || 1e-9));
      let stepX: number, stepY: number, sDX: number, sDY: number;
      if (rdx < 0) { stepX = -1; sDX = (g.px - mapX) * dDX; } else { stepX = 1; sDX = (mapX + 1 - g.px) * dDX; }
      if (rdy < 0) { stepY = -1; sDY = (g.py - mapY) * dDY; } else { stepY = 1; sDY = (mapY + 1 - g.py) * dDY; }
      let side = 0;
      for (let i = 0; i < 64; i++) {
        if (sDX < sDY) { sDX += dDX; mapX += stepX; side = 0; } else { sDY += dDY; mapY += stepY; side = 1; }
        if (mapX < 0 || mapY < 0 || mapX >= SIZE || mapY >= SIZE) return null;
        const v = g.map[mapY][mapX];
        if (v > 0) {
          const dist = (side === 0 ? sDX - dDX : sDY - dDY);
          if (dist > range) return null;
          return { cx: mapX, cy: mapY, dist, v };
        }
      }
      return null;
    };

    /* ---------------- Granaten & Explosionen ---------------- */
    const burst = (x: number, y: number, n: number, colors: string[]) => {
      const g = s();
      for (let i = 0; i < n; i++) {
        const a = Math.random() * Math.PI * 2;
        const v = 1.5 + Math.random() * 4;
        g.particles.push({
          x, y, vx: Math.cos(a) * v, vy: Math.sin(a) * v,
          life: 0.4 + Math.random() * 0.5,
          color: colors[Math.floor(Math.random() * colors.length)],
        });
      }
      if (g.particles.length > 250) g.particles.splice(0, g.particles.length - 250);
    };

    const explode = (x: number, y: number, killer: number) => {
      const g = s();
      sBoom();
      burst(x, y, 26, ["#ffcc33", "#ff5544", "#888888", "#22ff55"]);
      // Bots
      for (const b of g.bots) {
        if (!b.alive) continue;
        const d = Math.hypot(b.x - x, b.y - y);
        if (d < 2.6) {
          const dmg = Math.round(90 * (1 - d / 3));
          b.hp -= dmg;
          if (b.hp <= 0) kill(killer, b.id);
        }
      }
      // Spieler (auch selbstverletzend – Respekt!)
      const dP = Math.hypot(g.px - x, g.py - y);
      if (dP < 2.6 && g.hp > 0) {
        g.hp -= Math.round(70 * (1 - dP / 3));
        g.hurt = 0.4;
        if (g.hp <= 0) {
          g.hp = 0;
          kill(killer === 0 ? 0 : killer, 0);
          if (killer !== 0) addFeed("Von Granade zerlegt 💥");
        }
      }
      // DESTRUCTION: Wände im Radius sprengen
      const R = 1.6;
      let breached = false;
      for (let cy = Math.floor(y - R); cy <= Math.ceil(y + R); cy++) {
        for (let cx = Math.floor(x - R); cx <= Math.ceil(x + R); cx++) {
          if (cx < 1 || cy < 1 || cx >= SIZE - 1 || cy >= SIZE - 1) continue;
          const v = g.map[cy][cx];
          if (v >= 2) { g.map[cy][cx] = v === 2 ? 3 : 0; breached = true; }
        }
      }
      if (breached) addFeed(killer === 0 ? "💥 Bresche gesprengt!" : `💥 ${g.bots.find((b) => b.id === killer)?.name ?? "BOT"} sprengt eine Wand!`);
      else addFeed("💥 DETONATION");
    };

    const nadesTick = (dt: number) => {
      const g = s();
      const done: Nad[] = [];
      for (const n of g.nades) {
        n.t -= dt;
        const nx = n.x + n.vx * dt, ny = n.y + n.vy * dt;
        if (!isWall(nx, n.y)) n.x = nx; else n.vx *= -0.4;
        if (!isWall(n.x, ny)) n.y = ny; else n.vy *= -0.4;
        n.vx *= 1 - 1.5 * dt; n.vy *= 1 - 1.5 * dt;
        if (n.t <= 0) done.push(n);
      }
      if (done.length) {
        g.nades = g.nades.filter((n) => !done.includes(n));
        for (const n of done) explode(n.x, n.y, n.killer);
      }
    };

    /* Zelle zwischen zwei Punkten finden (für Bot-Breaching) */
    const wallBetween = (x0: number, y0: number, x1: number, y1: number): { cx: number; cy: number; v: number } | null => {
      const dx = x1 - x0, dy = y1 - y0;
      const steps = Math.ceil(Math.hypot(dx, dy) * 4);
      for (let i = 1; i < steps; i++) {
        const t = i / steps;
        const cx = Math.floor(x0 + dx * t), cy = Math.floor(y0 + dy * t);
        if (cx < 1 || cy < 1 || cx >= SIZE - 1 || cy >= SIZE - 1) continue;
        const v = s().map[cy][cx];
        if (v >= 2) return { cx, cy, v };
        if (v === 1) return null; // massiv – kein Breach möglich
      }
      return null;
    };

    /* ---------------- Mode logic ---------------- */
    const modeTick = (dt: number) => {
      const g = s();

      // Match-Timer + Overtime
      if (g.mode !== "sabotage" && !g.winner) {
        g.matchTime -= dt;
        if (g.matchTime <= 0) {
          const tied =
            g.mode === "ffa"
              ? (() => { const sc = [...g.ffaScore].sort((a, b) => b - a); return sc[0] === sc[1]; })()
              : g.teamScore[0] === g.teamScore[1];
          if (!g.overtime && tied) {
            g.overtime = true; g.matchTime = 60;
            addFeed("⏱ OVERTIME – SUDDEN DEATH!");
          } else if (g.mode === "ffa") {
            const best = g.ffaScore.indexOf(Math.max(...g.ffaScore));
            g.winner = best === 0 ? "DU" : BOT_NAMES[best - 1];
          } else {
            g.winner = g.teamScore[0] > g.teamScore[1] ? "TEAM GRÜN" : g.teamScore[1] > g.teamScore[0] ? "TEAM ROT" : "UNENTSCHIEDEN";
          }
        }
      }

      if (g.mode === "hq") {
        const inA = nearTeam(12.5, 12.5, 2.5, 0), inB = nearTeam(12.5, 12.5, 2.5, 1);
        if (inA !== inB) { g.hqOwner = inA ? 0 : 1; g.teamScore[g.hqOwner] += dt; }
        if (g.teamScore[0] >= 60) g.winner = "TEAM GRÜN";
        if (g.teamScore[1] >= 60) g.winner = "TEAM ROT";
      }
      if (g.mode === "domination") {
        DOM_POINTS.forEach((p, i) => {
          const inA = nearTeam(p.x, p.y, 2, 0), inB = nearTeam(p.x, p.y, 2, 1);
          if (inA !== inB) { g.domOwner[i] = inA ? 0 : 1; g.teamScore[g.domOwner[i]] += dt; }
        });
        if (g.teamScore[0] >= 60) g.winner = "TEAM GRÜN";
        if (g.teamScore[1] >= 60) g.winner = "TEAM ROT";
      }
      if (g.mode === "ctf") {
        const f = g.flags[1];
        if (!f.carried && g.hp > 0 && Math.hypot(g.px - f.x, g.py - f.y) < 0.8) { f.carried = true; addFeed("DU hast die Flagge!"); }
        if (f.carried && Math.hypot(g.px - 2.5, g.py - 2.5) < 1.5) {
          g.teamScore[0]++; f.carried = false; f.x = 21.5; f.y = 21.5;
          addFeed(`DU capt! ${g.teamScore[0]}/2`);
          if (g.teamScore[0] >= 2) g.winner = "TEAM GRÜN";
        }
        const fg = g.flags[0];
        for (const b of g.bots) {
          if (b.team !== 1 || !b.alive) continue;
          if (!fg.carried && Math.hypot(b.x - fg.x, b.y - fg.y) < 0.8) { fg.carried = true; fg.carrierId = b.id; addFeed(`${b.name} hat DEINE Flagge!`); }
          if (fg.carried && fg.carrierId === b.id && Math.hypot(b.x - 21.5, b.y - 21.5) < 1.5) {
            g.teamScore[1]++; fg.carried = false; fg.carrierId = null; fg.x = 2.5; fg.y = 2.5;
            addFeed(`${b.name} capt! ${g.teamScore[1]}/2`);
            if (g.teamScore[1] >= 2) g.winner = "TEAM ROT";
          }
        }
      }
      if (g.mode === "sabotage") {
        const site = { x: 12.5, y: 12.5 };
        if (!g.bombPlanted) {
          const onSite = Math.hypot(g.px - site.x, g.py - site.y) < 1.6 && g.keys["e"];
          g.plantProgress = onSite ? g.plantProgress + dt : 0;
          if (g.plantProgress >= 4) { g.bombPlanted = true; g.bombTimer = 25; addFeed("LADUNG PLATZIERT!"); sBoom(); }
        } else {
          g.bombTimer -= dt;
          const defusing = g.bots.some((b) => b.alive && b.team === 1 && Math.hypot(b.x - site.x, b.y - site.y) < 1.6);
          if (defusing) {
            g.defuseProgress += dt;
            if (g.defuseProgress >= 3) {
              g.roundWins[1]++; addFeed("Entschärft! Runde an ROT");
              g.bombPlanted = false; g.plantProgress = 0; g.defuseProgress = 0; g.bombTimer = 0;
            }
          } else g.defuseProgress = 0;
          if (g.bombTimer <= 0 && g.bombPlanted) {
            g.roundWins[0]++; addFeed("DETONATION! Runde an GRÜN"); sBoom();
            g.bombPlanted = false; g.plantProgress = 0; g.bombTimer = 0;
          }
        }
        if (g.roundWins[0] >= 2) g.winner = "TEAM GRÜN";
        if (g.roundWins[1] >= 2) g.winner = "TEAM ROT";
      }
      if (g.mode === "tdm") {
        if (g.teamScore[0] >= 10) g.winner = "TEAM GRÜN";
        if (g.teamScore[1] >= 10) g.winner = "TEAM ROT";
      }
      if (g.mode === "ffa") {
        const best = g.ffaScore.indexOf(Math.max(...g.ffaScore));
        if (g.ffaScore[best] >= 8) g.winner = best === 0 ? "DU" : BOT_NAMES[best - 1];
      }
    };

    /* ---------------- Bot AI ---------------- */
    const botObjective = (b: Bot): [number, number] => {
      const g = s();
      switch (g.mode) {
        case "domination": {
          let bestI = 0, bestD = 99;
          DOM_POINTS.forEach((p, i) => {
            if (g.domOwner[i] === b.team) return;
            const d = Math.hypot(p.x - b.x, p.y - b.y);
            if (d < bestD) { bestD = d; bestI = i; }
          });
          return [DOM_POINTS[bestI].x, DOM_POINTS[bestI].y];
        }
        case "hq": return [12.5, 12.5];
        case "ctf": {
          if (b.team === 1) {
            const fg = g.flags[0];
            return fg.carried && fg.carrierId === b.id ? [21.5, 21.5] : [fg.x, fg.y];
          }
          return [g.flags[1].x, g.flags[1].y];
        }
        case "sabotage": return [12.5, 12.5];
        default: {
          let tx = b.x, ty = b.y, bd = 99;
          const dP = Math.hypot(g.px - b.x, g.py - b.y);
          if (dP < bd) { bd = dP; tx = g.px; ty = g.py; }
          for (const o of g.bots) {
            if (o.id === b.id || !o.alive) continue;
            if (g.mode !== "ffa" && o.team === b.team) continue;
            const d = Math.hypot(o.x - b.x, o.y - b.y);
            if (d < bd) { bd = d; tx = o.x; ty = o.y; }
          }
          return [tx, ty];
        }
      }
    };

    const botTick = (b: Bot, dt: number) => {
      const g = s();
      if (!b.alive) { if (g.time >= b.respawnAt) spawnAt(b.id); return; }
      const enemyOf = (team: number) => team !== b.team;
      let target: { x: number; y: number; id: number } | null = null;
      let td = 8;
      const dP = Math.hypot(g.px - b.x, g.py - b.y);
      if (dP < td && enemyOf(0) && g.hp > 0 && los(b.x, b.y, g.px, g.py)) { target = { x: g.px, y: g.py, id: 0 }; td = dP; }
      for (const o of g.bots) {
        if (o.id === b.id || !o.alive || !enemyOf(o.team)) continue;
        const d = Math.hypot(o.x - b.x, o.y - b.y);
        if (d < td && los(b.x, b.y, o.x, o.y)) { target = { x: o.x, y: o.y, id: o.id }; td = d; }
      }
      const [ox, oy] = target ? [target.x, target.y] : botObjective(b);
      const dx = ox - b.x, dy = oy - b.y;
      const dist = Math.hypot(dx, dy) || 0.001;
      b.strafeT -= dt;
      if (b.strafeT <= 0) { b.strafeT = 0.6 + Math.random() * 0.8; b.strafeDir = Math.random() < 0.5 ? -1 : 1; }
      const bsp = BOT_DMG[g.diff].speed;
      if (!target && dist > 0.5) moveEntity(b, (dx / dist) * bsp * dt, (dy / dist) * bsp * dt);
      else if (target && td > 3) moveEntity(b, (dx / dist) * (bsp - 0.2) * dt, (dy / dist) * (bsp - 0.2) * dt);
      if (target) {
        // Strafen im Kampf + Rückzug bei niedriger HP
        const sx = (-dy / dist) * b.strafeDir, sy = (dx / dist) * b.strafeDir;
        moveEntity(b, sx * 1.3 * dt, sy * 1.3 * dt);
        if (b.hp < 30) moveEntity(b, (-dx / dist) * 1.4 * dt, (-dy / dist) * 1.4 * dt);
      }
      b.cd -= dt;
      if (target && b.cd <= 0) {
        b.cd = (0.7 + Math.random() * 0.5) * BOT_DMG[g.diff].cdMul;
        sShot("dorn", td, Math.sin(Math.atan2(b.y - g.py, b.x - g.px) - g.pa));
        const dmg = (8 + Math.random() * 10) * BOT_DMG[g.diff].dmgMul;
        if (target.id === 0) { g.hp -= dmg; g.hurt = 0.3; if (g.hp <= 0) kill(b.id, 0); }
        else {
          const v = g.bots.find((x) => x.id === target!.id)!;
          v.hp -= dmg;
          if (v.hp <= 0) kill(b.id, v.id);
        }
      }

      // DESTRUCTION gegen den Spieler: Wand sprengen, wenn keine LOS
      b.breachCd -= dt;
      const dToPlayer = Math.hypot(g.px - b.x, g.py - b.y);
      if (b.breachCd <= 0 && dToPlayer < 6 && enemyOf(0) && g.hp > 0 && !los(b.x, b.y, g.px, g.py)) {
        const w = wallBetween(b.x, b.y, g.px, g.py);
        if (w) {
          b.breachCd = 5 + Math.random() * 4;
          g.map[w.cy][w.cx] = w.v === 2 ? 3 : 0;
          sBoom();
          addFeed(`⚠ ${b.name} sprengt eine Wand!`);
        } else b.breachCd = 2;
      }

      // Bot-Granaten auf sichtbare Ziele mittlere Distanz
      b.nadeCd -= dt;
      if (target && td > 3.5 && td < 8 && b.nadeCd <= 0 && g.nades.length < 6) {
        b.nadeCd = 9 + Math.random() * 6;
        const dxn = (target.x - b.x) / td, dyn = (target.y - b.y) / td;
        g.nades.push({ x: b.x, y: b.y, vx: dxn * 6.5, vy: dyn * 6.5, t: 0.9, killer: b.id });
      }
    };

    /* ---------------- Spieler 2 (Splitscreen) ---------------- */
    const p2Tick = (dt: number) => {
      const g = s(); const p = g.p2;
      p.hurt = Math.max(0, p.hurt - dt);
      if (p.hp <= 0) { if (g.time >= p.respawnAt) spawnAt(99); return; }
      const rot = 2.6 * dt;
      if (g.keys["arrowleft"]) p.a -= rot;
      if (g.keys["arrowright"]) p.a += rot;
      const sp = 4.2 * dt;
      let fx = 0, fy = 0;
      if (g.keys["arrowup"]) { fx += Math.cos(p.a); fy += Math.sin(p.a); }
      if (g.keys["arrowdown"]) { fx -= Math.cos(p.a); fy -= Math.sin(p.a); }
      const l = Math.hypot(fx, fy);
      if (l > 0.01) { const pl = { x: p.x, y: p.y }; moveEntity(pl, (fx / l) * sp, (fy / l) * sp); p.x = pl.x; p.y = pl.y; }
      if (p.reloading > 0) { p.reloading -= dt; if (p.reloading <= 0) p.ammo = 24; }
      if (g.keys["p"] && p.ammo < 24 && p.reloading <= 0) p.reloading = 1.2;
      p.fireCd -= dt;
      if (g.keys["enter"] && p.ammo > 0 && p.reloading <= 0 && p.fireCd <= 0) {
        p.fireCd = 0.22; p.ammo--;
        sShot("dorn");
        const dx = g.px - p.x, dy = g.py - p.y;
        const d = Math.hypot(dx, dy);
        if (d < 14 && g.hp > 0) {
          let ang = Math.atan2(dy, dx) - p.a;
          while (ang > Math.PI) ang -= 2 * Math.PI;
          while (ang < -Math.PI) ang += 2 * Math.PI;
          if (Math.abs(ang) < 0.06 + 0.25 / d && los(p.x, p.y, g.px, g.py)) {
            g.hp -= 26; g.hurt = 0.3; sHit();
            if (g.hp <= 0) kill(99, 0);
          }
        }
        if (p.ammo === 0) p.reloading = 1.2;
      }
    };

    /* ---------------- Player ---------------- */
    const playerTick = (dt: number) => {
      const g = s();
      if (g.hp <= 0) return;
      const rot = 2.6 * dt;
      if (!g.split && g.keys["arrowleft"]) g.pa -= rot;
      if (!g.split && g.keys["arrowright"]) g.pa += rot;
      const sprint = g.keys["shift"] ? 1.45 : 1;
      const sp = 4.2 * dt * sprint;
      const fwd = (g.keys["w"] || g.keys["arrowup"] ? 1 : 0) - (g.keys["s"] || g.keys["arrowdown"] ? 1 : 0) - g.touchVec.y;
      const str = (g.keys["d"] ? 1 : 0) - (g.keys["a"] ? 1 : 0) + g.touchVec.x;
      let fx = Math.cos(g.pa) * fwd + Math.cos(g.pa + Math.PI / 2) * str;
      let fy = Math.sin(g.pa) * fwd + Math.sin(g.pa + Math.PI / 2) * str;
      const l = Math.hypot(fx, fy);
      if (l > 0.01) {
        const pl = { x: g.px, y: g.py };
        moveEntity(pl, (fx / l) * sp, (fy / l) * sp);
        g.px = pl.x; g.py = pl.y;
      }

      if (g.reloading > 0) { g.reloading -= dt; if (g.reloading <= 0) g.ammo = g.weapon === "richter" ? 5 : 24; }
      const maxAmmo = g.weapon === "richter" ? 5 : 24;
      if (g.keys["r"] && g.ammo < maxAmmo && g.reloading <= 0) g.reloading = g.weapon === "richter" ? 1.6 : 1.2;

      // Pickups einsammeln
      for (const p of g.pickups) {
        if (!p.active && g.time >= p.respawnAt) p.active = true;
        if (p.active && Math.hypot(g.px - p.x, g.py - p.y) < 0.7) {
          p.active = false; p.respawnAt = g.time + 15;
          g.weapon = "richter"; g.ammo = 5; g.reloading = 0;
          addFeed("RICHTER-50 aufgenommen 🎯");
        }
      }

      g.flash = Math.max(0, g.flash - dt);
      g.fireCd -= dt;
      const rate = g.weapon === "brecher" ? 0.9 : g.weapon === "richter" ? 1.2 : 0.22;
      if (g.mouseDown && g.reloading <= 0 && g.ammo > 0 && g.fireCd <= 0) {
        g.fireCd = rate;
        g.ammo--;
        g.flash = 0.2;
        sShot(g.weapon === "richter" ? "brecher" : g.weapon);
        // Ziel bestimmen: Bot / P2 oder Wand?
        const wall = wallInSight(g.weapon === "brecher" ? 10 : 16);
        let best: Bot | null = null; let bd = g.weapon === "richter" ? 16 : 14;
        let hitP2 = false;
        if (g.split && g.p2.hp > 0) {
          const dx2 = g.p2.x - g.px, dy2 = g.p2.y - g.py;
          const d2 = Math.hypot(dx2, dy2);
          if (d2 < bd && (!wall || d2 < wall.dist) && los(g.px, g.py, g.p2.x, g.p2.y)) {
            let ang = Math.atan2(dy2, dx2) - g.pa;
            while (ang > Math.PI) ang -= 2 * Math.PI;
            while (ang < -Math.PI) ang += 2 * Math.PI;
            if (Math.abs(ang) < 0.06 + 0.25 / d2) {
              hitP2 = true;
              const dmg = g.weapon === "brecher" ? 70 : g.weapon === "richter" ? 100 : 26;
              g.p2.hp -= dmg; g.p2.hurt = 0.3; sHit(); g.hitMarker = 0.15;
              g.dmgNums.push({ v: dmg, t: g.time, kill: g.p2.hp <= 0 });
              if (g.p2.hp <= 0) kill(0, 99);
            }
          }
        }
        for (const b of g.bots) {
          if (!b.alive) continue;
          if (g.mode !== "ffa" && b.team === 0) continue;
          const dx = b.x - g.px, dy = b.y - g.py;
          const d = Math.hypot(dx, dy);
          if (d > bd) continue;
          let ang = Math.atan2(dy, dx) - g.pa;
          while (ang > Math.PI) ang -= 2 * Math.PI;
          while (ang < -Math.PI) ang += 2 * Math.PI;
          if (Math.abs(ang) < (g.weapon === "richter" ? 0.03 : 0.06) + 0.25 / d && los(g.px, g.py, b.x, b.y)) { best = b; bd = d; }
        }
        if (!hitP2) {
          if (best && (!wall || bd < wall.dist)) {
            const dmg = g.weapon === "brecher" ? 70 : g.weapon === "richter" ? 100 : 26;
            best.hp -= dmg;
            sHit();
            g.hitMarker = 0.15;
            g.dmgNums.push({ v: dmg, t: g.time, kill: best.hp <= 0 });
            if (best.hp <= 0) kill(0, best.id);
          } else if (wall && g.weapon === "brecher" && wall.v >= 2) {
            // DESTRUCTION: Wand sprengen
            g.map[wall.cy][wall.cx] = wall.v === 2 ? 3 : 0;
            sBoom();
            burst(wall.cx + 0.5, wall.cy + 0.5, 10, ["#ffcc33", "#888888"]);
            if (g.map[wall.cy][wall.cx] === 0) addFeed("BRESCHE GESPRENGT!");
          }
        }
        if (g.ammo === 0) g.reloading = 1.2;
      }
    };

    /* ---------------- Render ---------------- */
    const renderView = (cx: number, cy: number, ca: number, ox: number, ow: number, view: 0 | 1) => {
      const g = s();
      ctx.fillStyle = "#101510"; ctx.fillRect(ox, 0, ow, H / 2);
      ctx.fillStyle = "#0c0f0c"; ctx.fillRect(ox, H / 2, ow, H / 2);
      const zBuf = new Float32Array(ow);
      for (let i = 0; i < ow; i++) {
        const camX = (2 * i) / ow - 1;
        const rdx = Math.cos(ca) + Math.cos(ca + Math.PI / 2) * camX * 0.66;
        const rdy = Math.sin(ca) + Math.sin(ca + Math.PI / 2) * camX * 0.66;
        let mapX = Math.floor(cx), mapY = Math.floor(cy);
        const dDX = Math.abs(1 / (rdx || 1e-9)), dDY = Math.abs(1 / (rdy || 1e-9));
        let stepX: number, stepY: number, sDX: number, sDY: number;
        if (rdx < 0) { stepX = -1; sDX = (cx - mapX) * dDX; } else { stepX = 1; sDX = (mapX + 1 - cx) * dDX; }
        if (rdy < 0) { stepY = -1; sDY = (cy - mapY) * dDY; } else { stepY = 1; sDY = (mapY + 1 - cy) * dDY; }
        let side = 0, hit = 0, v = 1;
        for (let k = 0; k < 48 && !hit; k++) {
          if (sDX < sDY) { sDX += dDX; mapX += stepX; side = 0; } else { sDY += dDY; mapY += stepY; side = 1; }
          if (mapX < 0 || mapY < 0 || mapX >= SIZE || mapY >= SIZE) { hit = 1; break; }
          v = g.map[mapY][mapX];
          if (v > 0) hit = 1;
        }
        const dist = side === 0 ? sDX - dDX : sDY - dDY;
        zBuf[i] = dist;
        const lineH = Math.min(H * 2, H / (dist || 0.01));
        const shade = Math.max(0.12, 1 - dist / 14) * (side === 1 ? 0.75 : 1);
        let r = 36, gr = 90, b = 45;
        if (v === 3) { r = 120; gr = 70; b = 30; } else if (v === 2) { r = 60; gr = 110; b = 55; }
        ctx.fillStyle = `rgb(${Math.floor(r * shade)},${Math.floor(gr * shade)},${Math.floor(b * shade)})`;
        ctx.fillRect(ox + i, (H - lineH) / 2, 1, lineH);
      }

      const sprites: { x: number; y: number; color: string; label: string; h: number }[] = [];
      for (const b of g.bots) if (b.alive) sprites.push({ x: b.x, y: b.y, color: b.color, label: b.name, h: 0.8 });
      for (const n of g.nades) sprites.push({ x: n.x, y: n.y, color: n.t < 0.4 && Math.floor(g.time * 10) % 2 === 0 ? "#ff3333" : "#ffcc33", label: "", h: 0.15 });
      for (const pk of g.pickups) if (pk.active) sprites.push({ x: pk.x, y: pk.y, color: "#33ccff", label: "R50", h: 0.3 });
      if (g.mode === "ctf") for (const f of g.flags) if (!f.carried) sprites.push({ x: f.x, y: f.y, color: f.owner === 0 ? "#22ff55" : "#ff5544", label: "FLAG", h: 0.5 });
      if (g.mode === "domination") DOM_POINTS.forEach((pt, i) => sprites.push({ x: pt.x, y: pt.y, color: g.domOwner[i] === 0 ? "#22ff55" : g.domOwner[i] === 1 ? "#ff5544" : "#888888", label: pt.label, h: 0.4 }));
      if (g.mode === "hq") sprites.push({ x: 12.5, y: 12.5, color: g.hqOwner === 0 ? "#22ff55" : g.hqOwner === 1 ? "#ff5544" : "#888888", label: "HQ", h: 0.5 });
      if (g.mode === "sabotage") sprites.push({ x: 12.5, y: 12.5, color: g.bombPlanted ? "#ffcc33" : "#888888", label: "SITE", h: 0.4 });
      if (g.split) {
        if (view === 0) sprites.push({ x: g.p2.x, y: g.p2.y, color: "#ff5544", label: "P2", h: 0.8 });
        else sprites.push({ x: g.px, y: g.py, color: g.skinColor, label: "P1", h: 0.8 });
      }

      sprites.sort((a, b) => Math.hypot(b.x - cx, b.y - cy) - Math.hypot(a.x - cx, a.y - cy));
      const dirX = Math.cos(ca), dirY = Math.sin(ca);
      const planeX = Math.cos(ca + Math.PI / 2) * 0.66, planeY = Math.sin(ca + Math.PI / 2) * 0.66;
      const det = dirX * planeY - dirY * planeX;
      for (const sp of sprites) {
        const dx = sp.x - cx, dy = sp.y - cy;
        const tx = (planeY * dx - planeX * dy) / det;
        const ty = (-dirY * dx + dirX * dy) / det;
        if (ty <= 0.1) continue;
        const screenX = (ow / 2) * (1 + tx / ty);
        const full = H / ty;
        const size = full * sp.h;
        const sx = Math.floor(screenX - size / 4);
        const colW = Math.max(1, Math.floor(size / 2));
        const top = (H - full) / 2 + full * (1 - sp.h) * 0.5;
        ctx.globalAlpha = Math.max(0.35, 1 - ty / 16);
        ctx.fillStyle = sp.color;
        for (let c = 0; c < colW; c++) {
          const xx = sx + c;
          if (xx < 0 || xx >= ow || zBuf[xx] < ty) continue;
          ctx.fillRect(ox + xx, top, 1, size);
        }
        ctx.globalAlpha = 1;
        if (ty < 12 && screenX > 0 && screenX < ow) {
          ctx.fillStyle = sp.color;
          ctx.font = "6px monospace";
          ctx.textAlign = "center";
          ctx.fillText(sp.label, ox + screenX, Math.max(6, top - 2));
        }
      }

      // Partikel
      for (const pt of g.particles) {
        const dx = pt.x - cx, dy = pt.y - cy;
        const tx = (planeY * dx - planeX * dy) / det;
        const ty = (-dirY * dx + dirX * dy) / det;
        if (ty <= 0.15) continue;
        const screenX = Math.floor((ow / 2) * (1 + tx / ty));
        if (screenX < 0 || screenX >= ow || zBuf[screenX] < ty) continue;
        const size = Math.max(1, Math.floor(2 / ty));
        ctx.globalAlpha = Math.min(1, pt.life * 2);
        ctx.fillStyle = pt.color;
        ctx.fillRect(ox + screenX, H / 2 + Math.floor(6 / ty), size, size);
      }
      ctx.globalAlpha = 1;

      // Waffe + Crosshair
      const midX = ox + ow / 2;
      const weap = view === 0 ? g.weapon : "dorn";
      const bw = weap === "brecher";
      const rw = weap === "richter";
      ctx.fillStyle = rw ? "#0a1a24" : bw ? "#241a10" : "#1a1f1a";
      ctx.fillRect(midX - (bw ? 18 : rw ? 10 : 14), H - 26, bw ? 36 : rw ? 20 : 28, 26);
      if (rw) ctx.fillRect(midX - 2, H - 44, 4, 20);
      ctx.fillStyle = rw ? "#33ccff" : bw ? "#ffcc33" : view === 0 ? g.skinColor : "#ff5544";
      ctx.fillRect(midX - (bw ? 4 : 2), H - (rw ? 46 : 30), bw ? 8 : 4, 8);
      if (view === 0 && g.flash > 0.1) {
        ctx.fillStyle = bw ? "rgba(255,200,80,0.85)" : "rgba(120,255,120,0.8)";
        ctx.beginPath();
        ctx.arc(midX, H - 32, bw ? 10 : 7, 0, Math.PI * 2);
        ctx.fill();
      }
      const chCol = view === 0 ? g.skinColor : "#ff5544";
      ctx.strokeStyle = chCol;
      ctx.beginPath();
      ctx.moveTo(midX - 4, H / 2); ctx.lineTo(midX - 1, H / 2);
      ctx.moveTo(midX + 1, H / 2); ctx.lineTo(midX + 4, H / 2);
      ctx.moveTo(midX, H / 2 - 4); ctx.lineTo(midX, H / 2 - 1);
      ctx.moveTo(midX, H / 2 + 1); ctx.lineTo(midX, H / 2 + 4);
      ctx.stroke();

      if (view === 0) {
        if (g.hitMarker > 0 || g.killConfirm > 0) {
          ctx.strokeStyle = g.killConfirm > 0 ? "#ff5544" : "#ffffff";
          ctx.beginPath();
          ctx.moveTo(midX - 6, H / 2 - 6); ctx.lineTo(midX - 3, H / 2 - 3);
          ctx.moveTo(midX + 3, H / 2 - 3); ctx.lineTo(midX + 6, H / 2 - 6);
          ctx.moveTo(midX - 6, H / 2 + 6); ctx.lineTo(midX - 3, H / 2 + 3);
          ctx.moveTo(midX + 3, H / 2 + 3); ctx.lineTo(midX + 6, H / 2 + 6);
          ctx.stroke();
        }
        g.dmgNums.forEach((d, i) => {
          const age = g.time - d.t;
          ctx.font = "7px monospace";
          ctx.textAlign = "center";
          ctx.fillStyle = d.kill ? "#ff5544" : "#ffffff";
          ctx.globalAlpha = Math.max(0, 1 - age / 0.6);
          ctx.fillText(`${d.v}`, midX + 12, H / 2 - 8 - age * 20 - i * 7);
          ctx.globalAlpha = 1;
        });
      }
      const hurt = view === 0 ? g.hurt : g.p2.hurt;
      if (hurt > 0) {
        ctx.fillStyle = `rgba(255,30,30,${(hurt * 0.5).toFixed(3)})`;
        ctx.fillRect(ox, 0, ow, H);
      }
    };

    const render = () => {
      const g = s();
      if (g.split) {
        renderView(g.px, g.py, g.pa, 0, W / 2, 0);
        renderView(g.p2.x, g.p2.y, g.p2.a, W / 2, W / 2, 1);
        ctx.fillStyle = "#000000";
        ctx.fillRect(W / 2 - 1, 0, 2, H);
      } else {
        renderView(g.px, g.py, g.pa, 0, W, 0);
      }
      // Minimap (aus P1-Sicht)
      const mmS = 72, oxm = W - mmS - 4, oym = 4, cs = mmS / SIZE;
      ctx.fillStyle = "rgba(0,0,0,0.6)";
      ctx.fillRect(oxm - 1, oym - 1, mmS + 2, mmS + 2);
      for (let y = 0; y < SIZE; y++) {
        for (let x = 0; x < SIZE; x++) {
          const v = g.map[y][x];
          if (v === 0) continue;
          ctx.fillStyle = v === 1 ? "hsl(130 40% 22%)" : v === 2 ? "hsl(130 70% 35%)" : "hsl(25 80% 45%)";
          ctx.fillRect(oxm + x * cs, oym + y * cs, cs, cs);
        }
      }
      const obj: { x: number; y: number }[] = [];
      if (g.mode === "domination") DOM_POINTS.forEach((pt) => obj.push(pt));
      if (g.mode === "hq" || g.mode === "sabotage") obj.push({ x: 12.5, y: 12.5 });
      if (g.mode === "ctf") g.flags.forEach((f) => { if (!f.carried) obj.push(f); });
      ctx.fillStyle = "#ffcc33";
      for (const o of obj) ctx.fillRect(oxm + o.x * cs - 1, oym + o.y * cs - 1, 2, 2);
      for (const b of g.bots) {
        if (!b.alive) continue;
        ctx.fillStyle = b.color;
        ctx.fillRect(oxm + b.x * cs - 1, oym + b.y * cs - 1, 2, 2);
      }
      if (g.split) {
        ctx.fillStyle = "#ff5544";
        ctx.fillRect(oxm + g.p2.x * cs - 1, oym + g.p2.y * cs - 1, 2, 2);
      }
      ctx.fillStyle = g.skinColor;
      ctx.fillRect(oxm + g.px * cs - 1, oym + g.py * cs - 1, 2.5, 2.5);
      ctx.strokeStyle = g.skinColor;
      ctx.beginPath();
      ctx.moveTo(oxm + g.px * cs, oym + g.py * cs);
      ctx.lineTo(oxm + (g.px + Math.cos(g.pa) * 2.2) * cs, oym + (g.py + Math.sin(g.pa) * 2.2) * cs);
      ctx.stroke();
    };

    /* ---------------- HUD sync ---------------- */
    const syncHud = () => {
      const g = s();
      let scores = "", objective = "";
      if (g.mode === "ffa") {
        scores = g.ffaScore.map((v, i) => `${i === 0 ? "DU" : BOT_NAMES[i - 1]}:${v}`).join("  ");
        objective = "Erster mit 8 Kills gewinnt";
      } else {
        scores = `GRÜN ${Math.floor(g.teamScore[0])} : ${Math.floor(g.teamScore[1])} ROT`;
        if (g.mode === "sabotage") objective = g.bombPlanted ? `DETONATION in ${Math.max(0, g.bombTimer).toFixed(0)} s` : `Runden ${g.roundWins[0]}:${g.roundWins[1]} – SITE (Mitte): halte E`;
        if (g.mode === "ctf") objective = "Hol die ROTE Flagge (SO) in deine Basis (NW)";
        if (g.mode === "domination") objective = `Zonen: ${g.domOwner.map((o, i) => `${DOM_POINTS[i].label}${o === 0 ? "🟩" : o === 1 ? "🟥" : "⬜"}`).join(" ")}`;
        if (g.mode === "hq") objective = `HQ: ${g.hqOwner === 0 ? "GRÜN" : g.hqOwner === 1 ? "ROT" : "neutral"} – halten!`;
      }
      const scoreboard: ScoreRow[] = [
        { name: g.split ? "P1" : "DU", team: 0, kills: g.playerKills, deaths: g.playerDeaths, isPlayer: true },
        ...(g.split ? [{ name: "P2", team: 1, kills: g.p2.kills, deaths: g.p2.deaths, isPlayer: false }] : []),
        ...g.bots.map((b) => ({ name: b.name, team: b.team, kills: b.kills, deaths: b.deaths, isPlayer: false })),
      ].sort((a, b) => b.kills - a.kills);
      const tm = Math.max(0, Math.floor(g.matchTime));
      setHud({
        hp: Math.max(0, Math.round(g.hp)),
        ammo: g.ammo,
        reloading: g.reloading > 0,
        weapon: g.weapon === "brecher" ? "BRECHER-7" : g.weapon === "richter" ? "RICHTER-50" : "DORN",
        grenades: g.grenades,
        scores, objective,
        feed: g.feed.filter((f) => g.time - f.t < 5).map((f) => f.text),
        winner: g.winner,
        planting: g.plantProgress > 0 && !g.bombPlanted,
        timeLeft: g.mode === "sabotage" ? "" : `${Math.floor(tm / 60)}:${String(tm % 60).padStart(2, "0")}`,
        overtime: g.overtime,
        tab: !!g.keys["tab"],
        scoreboard,
        split: g.split,
        p2hp: Math.max(0, Math.round(g.p2.hp)),
        p2ammo: g.p2.ammo,
      });
      if (g.winner && !g.recorded) {
        g.recorded = true;
        try {
          const rec = loadRecords();
          const cur = rec[g.mode] ?? { wins: 0, bestKills: 0 };
          const win = g.winner === "DU" || g.winner === "TEAM GRÜN";
          rec[g.mode] = {
            wins: cur.wins + (win ? 1 : 0),
            bestKills: Math.max(cur.bestKills, g.playerKills),
          };
          localStorage.setItem(REC_KEY, JSON.stringify(rec));
        } catch { /* Speicher nicht verfügbar */ }
      }
      if (g.winner) setScreen("end");
    };

    /* ---------------- Loop ---------------- */
    let hudAcc = 0;
    const loop = (now: number) => {
      const dt = Math.min(0.05, (now - last) / 1000);
      last = now;
      const g = s();
      g.time += dt;
      g.hitMarker = Math.max(0, g.hitMarker - dt);
      g.killConfirm = Math.max(0, g.killConfirm - dt);
      g.hurt = Math.max(0, g.hurt - dt);
      g.dmgNums = g.dmgNums.filter((d) => g.time - d.t < 0.6);
      for (const p of g.particles) { p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 1 - 3 * dt; p.vy *= 1 - 3 * dt; p.life -= dt; }
      g.particles = g.particles.filter((p) => p.life > 0);
      nadesTick(dt);
      if (g.hp <= 0 && !g.winner) {
        if (!g.pRespawnAt) g.pRespawnAt = g.time + 3;
        if (g.time >= g.pRespawnAt) { g.pRespawnAt = 0; spawnAt(0); }
      } else playerTick(dt);
      if (g.split) p2Tick(dt);
      for (const b of g.bots) botTick(b, dt);
      modeTick(dt);
      render();
      hudAcc += dt;
      if (hudAcc > 0.12) { hudAcc = 0; syncHud(); }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);

    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener("keydown", kd);
      window.removeEventListener("keyup", ku);
      canvas.removeEventListener("mousedown", md);
      window.removeEventListener("mouseup", mu);
      window.removeEventListener("mousemove", mm);
      canvas.removeEventListener("touchstart", ts);
      canvas.removeEventListener("touchmove", tm);
      canvas.removeEventListener("touchend", te);
      if (document.pointerLockElement === canvas) document.exitPointerLock();
    };
  }, [screen]);

  /* ================= UI ================= */
  const totalWins = Object.values(records).reduce((s, r) => s + (r?.wins ?? 0), 0);
  if (screen === "menu" || screen === "end") {
    return (
      <div className="min-h-screen bg-background text-foreground flex items-center justify-center px-6 py-16">
        <div className="max-w-3xl w-full">
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-3">
            WIRRWARR // Web-Demo v0.4
          </p>
          <h1 className="text-4xl md:text-5xl font-bold mb-2">
            {screen === "end" ? (
              <>Sieg: <span className="text-primary glow-neon">{hud.winner}</span></>
            ) : (
              <>Spiel die <span className="text-primary glow-neon">Prototyp-Arena</span></>
            )}
          </h1>
          <p className="text-muted-foreground mb-6 leading-relaxed">
            Raycaster-Prototyp v0.4: Minimap, RICHTER-50-Pickups, Bot-Breaching &amp; Bot-Granaten,
            Match-Timer mit Overtime, Tab-Scoreboard – dazu 6 Modi, DESTRUCTION, Granaten, Hitmarker,
            Rekorde, 3 Maps &amp; Touch-Controls. Steuerung:{" "}
            <span className="text-foreground font-mono text-sm">WASD</span> +{" "}
            <span className="text-foreground font-mono text-sm">Shift</span> Sprint,{" "}
            <span className="text-foreground font-mono text-sm">Klick/Maus</span> schießen,{" "}
            <span className="text-foreground font-mono text-sm">Q/1/2</span> Waffe,{" "}
            <span className="text-foreground font-mono text-sm">R</span> laden,{" "}
            <span className="text-foreground font-mono text-sm">E</span> pflanzen,{" "}
            <span className="text-foreground font-mono text-sm">Tab</span> Scoreboard. Blaue Pickups = RICHTER-50.
            Mobil: links ziehen = laufen, rechts = schauen.
          </p>

          {/* Schwierigkeit */}
          <div className="grid grid-cols-3 gap-3 mb-6">
            {BOT_DMG.map((d, i) => (
              <button
                key={d.name}
                type="button"
                onClick={() => setDiff(i)}
                aria-pressed={diff === i}
                className={`rounded-sm border px-3 py-2.5 font-mono text-xs tracking-wider uppercase transition-all min-h-[44px] ${
                  diff === i ? "border-primary/70 bg-primary/10 text-primary box-glow-neon" : "border-border bg-card text-muted-foreground hover:border-primary/30"
                }`}
              >
                {d.name}
              </button>
            ))}
          </div>

          {/* Skins */}
          <div className="flex flex-wrap items-center gap-2 mb-6">
            <span className="font-mono text-[10px] tracking-[0.25em] uppercase text-muted-foreground mr-1">Skin ({totalWins} Siege):</span>
            {SKINS.map((sk) => {
              const unlocked = totalWins >= sk.wins;
              return (
                <button
                  key={sk.id}
                  type="button"
                  disabled={!unlocked}
                  title={unlocked ? sk.name : `Ab ${sk.wins} Siegen`}
                  onClick={() => {
                    setSkin(sk.id);
                    try { localStorage.setItem("wirrwarr-demo-skin", sk.id); } catch { /* ignore */ }
                  }}
                  aria-pressed={skin === sk.id}
                  className={`w-9 h-9 rounded-sm border-2 transition-all ${skin === sk.id ? "scale-110" : ""} ${unlocked ? "cursor-pointer" : "opacity-30 cursor-not-allowed"}`}
                  style={{ background: sk.color, borderColor: skin === sk.id ? "#ffffff" : "transparent" }}
                />
              );
            })}
          </div>

          {/* Map-Auswahl */}
          <div className="grid grid-cols-2 sm:grid-cols-3 gap-3 mb-6">
            {MAP_INFO.map((m) => (
              <button
                key={m.id}
                type="button"
                onClick={() => setMapId(m.id)}
                aria-pressed={mapId === m.id}
                className={`text-left rounded-sm border p-4 transition-all min-h-[44px] ${
                  mapId === m.id ? "border-primary/70 bg-primary/10 box-glow-neon" : "border-border bg-card hover:border-primary/30"
                }`}
              >
                <p className={`font-bold mb-1 ${mapId === m.id ? "text-primary glow-neon-sm" : "text-foreground"}`}>{m.name}</p>
                <p className="font-mono text-[11px] text-muted-foreground leading-relaxed">{m.desc}</p>
              </button>
            ))}
          </div>

          {/* Splitscreen */}
          <button
            type="button"
            onClick={startSplit}
            className="w-full text-left border border-primary/50 bg-primary/10 rounded-sm p-4 mb-6 hover:bg-primary/20 hover:box-glow-neon transition-all min-h-[44px]"
          >
            <p className="font-bold text-primary glow-neon-sm mb-1">📡 Splitscreen-Duell (2 Spieler, 1 Gerät)</p>
            <p className="font-mono text-[11px] text-muted-foreground leading-relaxed">
              P1: WASD + Maus + Klick · P2: Pfeiltasten + Enter (Feuer) + P (laden). Erstes Duell auf 10 Kills.
            </p>
          </button>

          <div className="grid sm:grid-cols-2 gap-3 mb-8">
            {MODE_INFO.map((m) => (
              <button
                key={m.id}
                type="button"
                onClick={() => start(m.id)}
                className="text-left border border-border bg-card rounded-sm p-4 hover:border-primary/60 hover:box-glow-neon transition-all min-h-[44px]"
              >
                <p className="font-bold text-foreground mb-1">{m.name}</p>
                <p className="font-mono text-[11px] text-muted-foreground leading-relaxed">{m.desc}</p>
              </button>
            ))}
          </div>
          {/* Rekorde */}
          {Object.keys(records).length > 0 && (
            <div className="border border-border bg-card rounded-sm p-4 mb-6">
              <p className="font-mono text-[10px] tracking-[0.25em] uppercase text-primary glow-neon-sm mb-3">
                🏆 Deine Rekorde
              </p>
              <div className="flex flex-wrap gap-x-6 gap-y-1.5">
                {MODE_INFO.filter((m) => records[m.id]).map((m) => (
                  <p key={m.id} className="font-mono text-[11px] text-muted-foreground">
                    <span className="text-foreground">{m.name}</span>{" "}
                    · Siege: <span className="text-primary">{records[m.id]!.wins}</span>{" "}
                    · Best-Kills: <span className="text-primary">{records[m.id]!.bestKills}</span>
                  </p>
                ))}
              </div>
            </div>
          )}

          <a href="/" className="font-mono text-xs tracking-wider uppercase text-muted-foreground hover:text-primary transition-colors">
            ← Zurück zum GDD
          </a>
        </div>
      </div>
    );
  }

  return (
    <div className="relative h-screen w-full bg-black overflow-hidden select-none">
      <canvas ref={canvasRef} className="w-full h-full [image-rendering:pixelated] cursor-crosshair touch-none" />
      <div className="absolute top-3 left-1/2 -translate-x-1/2 text-center pointer-events-none">
        <p className={`font-mono text-sm tracking-wider ${hud.overtime ? "text-destructive animate-pulse-neon" : "text-primary glow-neon-sm"}`}>
          {hud.scores}
          {hud.timeLeft && <span className="text-foreground"> · ⏱ {hud.timeLeft}</span>}
          {hud.overtime && <span> · OT</span>}
        </p>
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider mt-1">{hud.objective}</p>
      </div>
      <div className="absolute top-12 left-3 text-left pointer-events-none space-y-1 max-w-[45%]">
        {hud.feed.map((f, i) => (
          <p key={i} className="font-mono text-[10px] text-primary/90">{f}</p>
        ))}
      </div>
      {/* Scoreboard (Tab) */}
      {hud.tab && (
        <div className="absolute inset-x-0 top-1/2 -translate-y-1/2 flex justify-center pointer-events-none">
          <div className="bg-black/85 border border-primary/40 rounded-sm p-4 min-w-[280px] box-glow-neon">
            <p className="font-mono text-[10px] tracking-[0.25em] uppercase text-primary mb-2">Scoreboard</p>
            {hud.scoreboard.map((r) => (
              <div key={r.name} className={`flex items-center justify-between gap-6 font-mono text-[11px] py-0.5 ${r.isPlayer ? "text-primary" : "text-foreground"}`}>
                <span className="flex items-center gap-2">
                  <span className="w-2 h-2 rounded-full" style={{ background: r.isPlayer ? "#ffffff" : r.team === 0 ? "#22ff55" : r.team >= 2 ? r.team === 2 ? "#ffcc33" : "#33ccff" : "#ff5544" }} />
                  {r.name}
                </span>
                <span className="text-muted-foreground">{r.kills} / {r.deaths}</span>
              </div>
            ))}
          </div>
        </div>
      )}
      <div className="absolute bottom-3 left-3 pointer-events-none">
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">{hud.split ? "P1 // INTEGRITÄT" : "INTEGRITÄT"}</p>
        <div className="w-40 h-1.5 bg-secondary rounded-full overflow-hidden mt-1">
          <div className={`h-full rounded-full ${hud.hp > 35 ? "bg-primary" : "bg-destructive"}`} style={{ width: `${hud.hp}%` }} />
        </div>
        <p className="font-mono text-xl leading-none mt-1" style={{ color: undefined }}>
          <span className="text-primary">{hud.hp}</span>
          {hud.split && <span className="text-foreground text-sm"> · {hud.reloading ? "LÄDT…" : `${hud.ammo} Schuss`}</span>}
        </p>
      </div>
      <div className="absolute bottom-3 right-3 text-right pointer-events-none">
        {hud.split ? (
          <>
            <p className="font-mono text-[10px] text-muted-foreground tracking-wider">P2 // INTEGRITÄT</p>
            <div className="w-40 h-1.5 bg-secondary rounded-full overflow-hidden mt-1 ml-auto">
              <div className={`h-full rounded-full ${hud.p2hp > 35 ? "bg-destructive" : "bg-primary"}`} style={{ width: `${hud.p2hp}%` }} />
            </div>
            <p className="font-mono text-xl text-destructive leading-none mt-1">
              {hud.p2hp}<span className="text-foreground text-sm"> · {hud.p2ammo} Schuss</span>
            </p>
          </>
        ) : (
          <>
            <p className="font-mono text-[10px] text-muted-foreground tracking-wider">
              {hud.weapon} <span className="text-primary">[Q]</span> · 💣 {hud.grenades} <span className="text-primary">[G]</span>
            </p>
            <p className="font-mono text-2xl text-foreground leading-none">
              {hud.reloading ? <span className="text-primary">LÄDT…</span> : <>{hud.ammo}<span className="text-muted-foreground text-sm">/∞</span></>}
            </p>
          </>
        )}
      </div>
      {/* Touch-Buttons */}
      {!hud.split && (
      <>
      <button
        type="button"
        className="lg:hidden absolute bottom-20 right-4 w-20 h-20 rounded-full border-2 border-primary/60 bg-primary/20 text-primary font-mono text-xs tracking-widest active:bg-primary/50"
        onTouchStart={() => { if (stateRef.current) stateRef.current.mouseDown = true; }}
        onTouchEnd={() => { if (stateRef.current) stateRef.current.mouseDown = false; }}
      >
        FEUER
      </button>
      <button
        type="button"
        className="lg:hidden absolute bottom-24 right-28 w-14 h-14 rounded-full border-2 border-border bg-secondary/40 text-foreground font-mono text-lg active:bg-primary/30"
        onTouchStart={() => {
          const g = stateRef.current;
          if (g && g.grenades > 0 && g.nades.length < 3) {
            g.grenades--;
            g.nades.push({ x: g.px, y: g.py, vx: Math.cos(g.pa) * 7, vy: Math.sin(g.pa) * 7, t: 1.1, killer: 0 });
          }
        }}
      >
        💣
      </button>
      </>
      )}
      {hud.planting && (
        <div className="absolute bottom-16 left-1/2 -translate-x-1/2 font-mono text-xs text-primary border border-primary/50 bg-black/60 px-3 py-1.5 rounded-sm animate-pulse-neon pointer-events-none">
          LADUNG WIRD PLATZIERT …
        </div>
      )}
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
