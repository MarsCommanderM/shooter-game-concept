"use client";

import { useEffect, useRef, useState } from "react";

/* ================================================================== */
/* WIRRWARR Web-Demo: Raycaster-FPS mit Bots & 6 Modi                  */
/* ================================================================== */

type ModeId = "tdm" | "ffa" | "hq" | "sabotage" | "ctf" | "domination";

const MODE_INFO: { id: ModeId; name: string; desc: string }[] = [
  { id: "tdm", name: "Team-Deathmatch", desc: "3v3 – erstes Team mit 10 Kills gewinnt." },
  { id: "ffa", name: "Frei für alle", desc: "6 Kämpfer, jeder allein – 8 Kills gewinnen." },
  { id: "hq", name: "Hauptquartier", desc: "HQ in der Mitte halten: 1 Punkt/Sekunde, 60 Punkte gewinnen." },
  { id: "sabotage", name: "Sabotage", desc: "Du bist Angreifer: Site mit E sprengen (4 s halten). Bots entschärfen. 2 Runden-Siege." },
  { id: "ctf", name: "Capture the Flag", desc: "Rote Flagge (Südost) klauen, zur grünen Basis (Nordwest) bringen. 2 Caps gewinnen." },
  { id: "domination", name: "Herrschaft", desc: "Zonen A/B/C halten: 1 Punkt/Sekunde pro Zone, 60 Punkte gewinnen." },
];

interface Bot {
  id: number;
  name: string;
  team: number;
  x: number;
  y: number;
  hp: number;
  alive: boolean;
  respawnAt: number;
  cd: number;
  color: string;
  kills: number;
  deaths: number;
}

interface HudState {
  hp: number;
  ammo: number;
  reloading: boolean;
  scores: string;
  objective: string;
  feed: string[];
  winner: string | null;
  planting: boolean;
}

const SIZE = 24;
const TEAM_COLORS = ["#22ff55", "#ff5544", "#ffcc33", "#33ccff", "#ff66cc", "#99ff33"];
const BOT_NAMES = ["VEGA", "JUNO", "RAZOR", "MORBID", "ECHO", "SABLE"];

function buildMap(): number[][] {
  const m: number[][] = Array.from({ length: SIZE }, () => Array(SIZE).fill(0));
  for (let i = 0; i < SIZE; i++) {
    m[0][i] = 1; m[SIZE - 1][i] = 1; m[i][0] = 1; m[i][SIZE - 1] = 1;
  }
  const rect = (x: number, y: number, w: number, h: number) => {
    for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) m[j][i] = 1;
  };
  rect(4, 4, 2, 2); rect(18, 4, 2, 2); rect(4, 18, 2, 2); rect(18, 18, 2, 2);
  rect(11, 4, 2, 2); rect(11, 18, 2, 2); rect(4, 11, 2, 2); rect(18, 11, 2, 2);
  return m;
}
const MAP = buildMap();

const SPAWNS: [number, number][] = [
  [2.5, 12.5], [21.5, 12.5], [2.5, 2.5], [21.5, 21.5], [12.5, 2.5], [12.5, 21.5],
];
const DOM_POINTS: { x: number; y: number; label: string }[] = [
  { x: 7.5, y: 7.5, label: "A" },
  { x: 12.5, y: 12.5, label: "B" },
  { x: 16.5, y: 16.5, label: "C" },
];

function isWall(x: number, y: number) {
  const cx = Math.floor(x), cy = Math.floor(y);
  if (cx < 0 || cy < 0 || cx >= SIZE || cy >= SIZE) return true;
  return MAP[cy][cx] === 1;
}

function los(x0: number, y0: number, x1: number, y1: number): boolean {
  const dx = x1 - x0, dy = y1 - y0;
  const dist = Math.hypot(dx, dy);
  const steps = Math.ceil(dist * 4);
  for (let i = 1; i < steps; i++) {
    const t = i / steps;
    if (isWall(x0 + dx * t, y0 + dy * t)) return false;
  }
  return true;
}

interface GameState {
  mode: ModeId;
  px: number; py: number; pa: number;
  hp: number; ammo: number; reloading: number; flash: number;
  pRespawnAt: number;
  bots: Bot[];
  keys: Record<string, boolean>;
  mouseDown: boolean;
  teamScore: number[];
  ffaScore: number[];
  domOwner: number[];
  hqOwner: number;
  flags: { owner: number; x: number; y: number; carried: boolean; carrierId: number | null }[];
  plantProgress: number;
  bombPlanted: boolean;
  bombTimer: number;
  defuseProgress: number;
  roundWins: number[];
  feed: { text: string; t: number }[];
  time: number;
  winner: string | null;
}

function makeGame(mode: ModeId): GameState {
  const bots: Bot[] = [];
  for (let i = 0; i < 5; i++) {
    const team = mode === "ffa" ? i + 1 : i % 2 === 0 ? 1 : 0;
    const sp = SPAWNS[(i + 1) % SPAWNS.length];
    bots.push({
      id: i + 1,
      name: BOT_NAMES[i],
      team,
      x: sp[0], y: sp[1], hp: 100, alive: true, respawnAt: 0, cd: 1 + Math.random(),
      color: TEAM_COLORS[mode === "ffa" ? i + 1 : team],
      kills: 0, deaths: 0,
    });
  }
  const pSpawn = SPAWNS[0];
  return {
    mode,
    px: pSpawn[0], py: pSpawn[1], pa: 0,
    hp: 100, ammo: 24, reloading: 0, flash: 0, pRespawnAt: 0,
    bots,
    keys: {},
    mouseDown: false,
    teamScore: [0, 0],
    ffaScore: Array(6).fill(0),
    domOwner: [-1, -1, -1],
    hqOwner: -1,
    flags: [
      { owner: 0, x: 2.5, y: 2.5, carried: false, carrierId: null },
      { owner: 1, x: 21.5, y: 21.5, carried: false, carrierId: null },
    ],
    plantProgress: 0, bombPlanted: false, bombTimer: 0, defuseProgress: 0,
    roundWins: [0, 0],
    feed: [], time: 0, winner: null,
  };
}

/* ------------------------------------------------------------------ */

export function PlayDemo() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const stateRef = useRef<GameState | null>(null);
  const [screen, setScreen] = useState<"menu" | "game" | "end">("menu");
  const [hud, setHud] = useState<HudState>({
    hp: 100, ammo: 24, reloading: false, scores: "", objective: "",
    feed: [], winner: null, planting: false,
  });

  const start = (mode: ModeId) => {
    stateRef.current = makeGame(mode);
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

    /* ---------------- Input ---------------- */
    const kd = (e: KeyboardEvent) => {
      if (e.key === "Tab") e.preventDefault();
      s().keys[e.key.toLowerCase()] = true;
    };
    const ku = (e: KeyboardEvent) => { s().keys[e.key.toLowerCase()] = false; };
    const md = () => {
      s().mouseDown = true;
      try { canvas.requestPointerLock?.(); } catch { /* ignore */ }
    };
    const mu = () => { s().mouseDown = false; };
    const mm = (e: MouseEvent) => {
      if (document.pointerLockElement === canvas) {
        s().pa += e.movementX * 0.0025;
      } else if (s().mouseDown) {
        s().pa += (e.clientX - lastMouseX) * 0.004;
      }
      lastMouseX = e.clientX;
    };
    window.addEventListener("keydown", kd);
    window.addEventListener("keyup", ku);
    canvas.addEventListener("mousedown", md);
    window.addEventListener("mouseup", mu);
    window.addEventListener("mousemove", mm);

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

    const kill = (killerId: number, victimId: number) => {
      const g = s();
      const victim = victimId === 0 ? null : g.bots.find((b) => b.id === victimId);
      const killer = killerId === 0 ? null : g.bots.find((b) => b.id === killerId);
      const kName = killerId === 0 ? "DU" : killer?.name ?? "?";
      const vName = victimId === 0 ? "DU" : victim?.name ?? "?";
      if (victim) { victim.hp = 0; victim.alive = false; victim.deaths++; victim.respawnAt = g.time + 3; }
      if (victimId === 0) g.hp = 0;
      if (killer) killer.kills++;
      if (g.mode === "ffa") g.ffaScore[killerId]++;
      else {
        const team = killerId === 0 ? 0 : killer!.team;
        g.teamScore[team]++;
      }
      addFeed(`${kName} ⚡ ${vName}`);
    };

    const spawnAt = (id: number) => {
      const g = s();
      const sp = SPAWNS[id % SPAWNS.length];
      if (id === 0) { g.px = sp[0]; g.py = sp[1]; g.hp = 100; }
      else {
        const b = g.bots.find((x) => x.id === id)!;
        b.x = sp[0]; b.y = sp[1]; b.hp = 100; b.alive = true;
      }
    };

    const nearTeam = (x: number, y: number, r: number, team: number) => {
      const g = s();
      if (team === 0 && Math.hypot(g.px - x, g.py - y) < r && g.hp > 0) return true;
      return g.bots.some((b) => b.alive && b.team === team && Math.hypot(b.x - x, b.y - y) < r);
    };

    /* ---------------- Mode logic ---------------- */
    const modeTick = (dt: number) => {
      const g = s();
      if (g.mode === "hq") {
        const inA = nearTeam(12.5, 12.5, 2.5, 0);
        const inB = nearTeam(12.5, 12.5, 2.5, 1);
        if (inA !== inB) {
          g.hqOwner = inA ? 0 : 1;
          g.teamScore[g.hqOwner] += dt;
        }
        if (g.teamScore[0] >= 60) g.winner = "TEAM GRÜN";
        if (g.teamScore[1] >= 60) g.winner = "TEAM ROT";
      }
      if (g.mode === "domination") {
        DOM_POINTS.forEach((p, i) => {
          const inA = nearTeam(p.x, p.y, 2, 0);
          const inB = nearTeam(p.x, p.y, 2, 1);
          if (inA !== inB) {
            g.domOwner[i] = inA ? 0 : 1;
            g.teamScore[g.domOwner[i]] += dt;
          }
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
          if (g.plantProgress >= 4) { g.bombPlanted = true; g.bombTimer = 25; addFeed("LADUNG PLATZIERT!"); }
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
            g.roundWins[0]++; addFeed("DETONATION! Runde an GRÜN");
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
      if (!b.alive) {
        if (g.time >= b.respawnAt) spawnAt(b.id);
        return;
      }
      const enemyOf = (team: number) =>
        g.mode === "ffa" ? team !== b.team : team !== b.team;
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
      if (!target && dist > 0.5) moveEntity(b, (dx / dist) * 2.2 * dt, (dy / dist) * 2.2 * dt);
      else if (target && td > 3) moveEntity(b, (dx / dist) * 2.0 * dt, (dy / dist) * 2.0 * dt);

      b.cd -= dt;
      if (target && b.cd <= 0) {
        b.cd = 0.7 + Math.random() * 0.5;
        const dmg = 8 + Math.random() * 10;
        if (target.id === 0) {
          g.hp -= dmg;
          if (g.hp <= 0) kill(b.id, 0);
        } else {
          const v = g.bots.find((x) => x.id === target!.id)!;
          v.hp -= dmg;
          if (v.hp <= 0) kill(b.id, v.id);
        }
      }
    };

    /* ---------------- Player ---------------- */
    const playerTick = (dt: number) => {
      const g = s();
      if (g.hp <= 0) return;
      const rot = 2.6 * dt;
      if (g.keys["arrowleft"]) g.pa -= rot;
      if (g.keys["arrowright"]) g.pa += rot;
      const sp = 4.2 * dt;
      let fx = 0, fy = 0;
      if (g.keys["w"] || g.keys["arrowup"]) { fx += Math.cos(g.pa); fy += Math.sin(g.pa); }
      if (g.keys["s"] || g.keys["arrowdown"]) { fx -= Math.cos(g.pa); fy -= Math.sin(g.pa); }
      if (g.keys["a"]) { fx += Math.cos(g.pa - Math.PI / 2); fy += Math.sin(g.pa - Math.PI / 2); }
      if (g.keys["d"]) { fx += Math.cos(g.pa + Math.PI / 2); fy += Math.sin(g.pa + Math.PI / 2); }
      const l = Math.hypot(fx, fy);
      if (l > 0) {
        const pl = { x: g.px, y: g.py };
        moveEntity(pl, (fx / l) * sp, (fy / l) * sp);
        g.px = pl.x; g.py = pl.y;
      }

      if (g.reloading > 0) {
        g.reloading -= dt;
        if (g.reloading <= 0) g.ammo = 24;
      }
      if (g.keys["r"] && g.ammo < 24 && g.reloading <= 0) g.reloading = 1.2;

      g.flash = Math.max(0, g.flash - dt);
      if (g.mouseDown && g.reloading <= 0 && g.ammo > 0 && g.flash <= 0.12) {
        g.ammo--;
        g.flash = 0.25;
        let best: Bot | null = null; let bd = 14;
        for (const b of g.bots) {
          if (!b.alive) continue;
          if (g.mode !== "ffa" && b.team === 0) continue;
          const dx = b.x - g.px, dy = b.y - g.py;
          const d = Math.hypot(dx, dy);
          if (d > bd) continue;
          let ang = Math.atan2(dy, dx) - g.pa;
          while (ang > Math.PI) ang -= 2 * Math.PI;
          while (ang < -Math.PI) ang += 2 * Math.PI;
          if (Math.abs(ang) < 0.06 + 0.25 / d && los(g.px, g.py, b.x, b.y)) { best = b; bd = d; }
        }
        if (best) {
          best.hp -= 26;
          if (best.hp <= 0) kill(0, best.id);
        }
        if (g.ammo === 0) g.reloading = 1.2;
      }
    };

    /* ---------------- Render ---------------- */
    const render = () => {
      const g = s();
      ctx.fillStyle = "#101510";
      ctx.fillRect(0, 0, W, H / 2);
      ctx.fillStyle = "#0c0f0c";
      ctx.fillRect(0, H / 2, W, H / 2);

      const zBuf = new Float32Array(W);
      for (let x = 0; x < W; x++) {
        const camX = (2 * x) / W - 1;
        const rdx = Math.cos(g.pa) + Math.cos(g.pa + Math.PI / 2) * camX * 0.66;
        const rdy = Math.sin(g.pa) + Math.sin(g.pa + Math.PI / 2) * camX * 0.66;
        let mapX = Math.floor(g.px), mapY = Math.floor(g.py);
        const dDX = Math.abs(1 / (rdx || 1e-9)), dDY = Math.abs(1 / (rdy || 1e-9));
        let stepX: number, stepY: number, sDX: number, sDY: number;
        if (rdx < 0) { stepX = -1; sDX = (g.px - mapX) * dDX; } else { stepX = 1; sDX = (mapX + 1 - g.px) * dDX; }
        if (rdy < 0) { stepY = -1; sDY = (g.py - mapY) * dDY; } else { stepY = 1; sDY = (mapY + 1 - g.py) * dDY; }
        let side = 0, hit = 0;
        for (let i = 0; i < 48 && !hit; i++) {
          if (sDX < sDY) { sDX += dDX; mapX += stepX; side = 0; } else { sDY += dDY; mapY += stepY; side = 1; }
          if (mapX < 0 || mapY < 0 || mapX >= SIZE || mapY >= SIZE) { hit = 1; break; }
          if (MAP[mapY][mapX] === 1) hit = 1;
        }
        const dist = side === 0 ? sDX - dDX : sDY - dDY;
        zBuf[x] = dist;
        const lineH = Math.min(H * 2, H / (dist || 0.01));
        const shade = Math.max(0.12, 1 - dist / 14) * (side === 1 ? 0.75 : 1);
        ctx.fillStyle = `rgb(${Math.floor(36 * shade)},${Math.floor(90 * shade)},${Math.floor(45 * shade)})`;
        ctx.fillRect(x, (H - lineH) / 2, 1, lineH);
      }

      const sprites: { x: number; y: number; color: string; label: string; h: number }[] = [];
      for (const b of g.bots) if (b.alive) sprites.push({ x: b.x, y: b.y, color: b.color, label: b.name, h: 0.8 });
      if (g.mode === "ctf") for (const f of g.flags) if (!f.carried) sprites.push({ x: f.x, y: f.y, color: f.owner === 0 ? "#22ff55" : "#ff5544", label: "FLAG", h: 0.5 });
      if (g.mode === "domination") DOM_POINTS.forEach((p, i) => sprites.push({ x: p.x, y: p.y, color: g.domOwner[i] === 0 ? "#22ff55" : g.domOwner[i] === 1 ? "#ff5544" : "#888888", label: p.label, h: 0.4 }));
      if (g.mode === "hq") sprites.push({ x: 12.5, y: 12.5, color: g.hqOwner === 0 ? "#22ff55" : g.hqOwner === 1 ? "#ff5544" : "#888888", label: "HQ", h: 0.5 });
      if (g.mode === "sabotage") sprites.push({ x: 12.5, y: 12.5, color: g.bombPlanted ? "#ffcc33" : "#888888", label: "SITE", h: 0.4 });

      sprites.sort((a, b) => Math.hypot(b.x - g.px, b.y - g.py) - Math.hypot(a.x - g.px, a.y - g.py));
      const dirX = Math.cos(g.pa), dirY = Math.sin(g.pa);
      const planeX = Math.cos(g.pa + Math.PI / 2) * 0.66, planeY = Math.sin(g.pa + Math.PI / 2) * 0.66;
      const det = dirX * planeY - dirY * planeX;
      for (const sp of sprites) {
        const dx = sp.x - g.px, dy = sp.y - g.py;
        const tx = (planeY * dx - planeX * dy) / det;
        const ty = (-dirY * dx + dirX * dy) / det;
        if (ty <= 0.1) continue;
        const screenX = (W / 2) * (1 + tx / ty);
        const full = H / ty;
        const size = full * sp.h;
        const sx = Math.floor(screenX - size / 4);
        const colW = Math.max(1, Math.floor(size / 2));
        const top = (H - full) / 2 + full * (1 - sp.h) * 0.5;
        ctx.globalAlpha = Math.max(0.35, 1 - ty / 16);
        ctx.fillStyle = sp.color;
        for (let c = 0; c < colW; c++) {
          const xx = sx + c;
          if (xx < 0 || xx >= W || zBuf[xx] < ty) continue;
          ctx.fillRect(xx, top, 1, size);
        }
        ctx.globalAlpha = 1;
        if (ty < 12 && screenX > 0 && screenX < W) {
          ctx.fillStyle = sp.color;
          ctx.font = "6px monospace";
          ctx.textAlign = "center";
          ctx.fillText(sp.label, screenX, Math.max(6, top - 2));
        }
      }

      // Waffe + Muzzle-Flash
      ctx.fillStyle = "#1a1f1a";
      ctx.fillRect(W / 2 - 14, H - 26, 28, 26);
      ctx.fillStyle = "#22ff55";
      ctx.fillRect(W / 2 - 2, H - 30, 4, 8);
      if (g.flash > 0.15) {
        ctx.fillStyle = "rgba(120,255,120,0.8)";
        ctx.beginPath();
        ctx.arc(W / 2, H - 32, 7, 0, Math.PI * 2);
        ctx.fill();
      }
      // Crosshair
      ctx.strokeStyle = "#22ff55";
      ctx.beginPath();
      ctx.moveTo(W / 2 - 4, H / 2); ctx.lineTo(W / 2 - 1, H / 2);
      ctx.moveTo(W / 2 + 1, H / 2); ctx.lineTo(W / 2 + 4, H / 2);
      ctx.moveTo(W / 2, H / 2 - 4); ctx.lineTo(W / 2, H / 2 - 1);
      ctx.moveTo(W / 2, H / 2 + 1); ctx.lineTo(W / 2, H / 2 + 4);
      ctx.stroke();
    };

    /* ---------------- HUD sync ---------------- */
    const syncHud = () => {
      const g = s();
      let scores = "";
      let objective = "";
      if (g.mode === "ffa") {
        scores = g.ffaScore.map((v, i) => `${i === 0 ? "DU" : BOT_NAMES[i - 1]}:${v}`).join("  ");
        objective = "Erster mit 8 Kills gewinnt";
      } else {
        scores = `GRÜN ${Math.floor(g.teamScore[0])} : ${Math.floor(g.teamScore[1])} ROT`;
        if (g.mode === "sabotage")
          objective = g.bombPlanted
            ? `DETONATION in ${Math.max(0, g.bombTimer).toFixed(0)} s`
            : "SITE (Mitte): halte E zum Pflanzen";
        if (g.mode === "ctf") objective = `Hol die ROTE Flagge (SO) in deine Basis (NW)`;
        if (g.mode === "domination")
          objective = `Zonen: ${g.domOwner.map((o, i) => `${DOM_POINTS[i].label}${o === 0 ? "🟩" : o === 1 ? "🟥" : "⬜"}`).join(" ")}`;
        if (g.mode === "hq") objective = `HQ: ${g.hqOwner === 0 ? "GRÜN" : g.hqOwner === 1 ? "ROT" : "neutral"} – halten!`;
        if (g.mode === "sabotage") objective = g.bombPlanted ? `DETONATION in ${Math.max(0, g.bombTimer).toFixed(0)} s` : `Runden ${g.roundWins[0]}:${g.roundWins[1]} – SITE (Mitte): halte E`;
      }
      setHud({
        hp: Math.max(0, Math.round(g.hp)),
        ammo: g.ammo,
        reloading: g.reloading > 0,
        scores,
        objective,
        feed: g.feed.filter((f) => g.time - f.t < 5).map((f) => f.text),
        winner: g.winner,
        planting: g.plantProgress > 0 && !g.bombPlanted,
      });
      if (g.winner) setScreen("end");
    };

    /* ---------------- Loop ---------------- */
    let hudAcc = 0;
    const loop = (now: number) => {
      const dt = Math.min(0.05, (now - last) / 1000);
      last = now;
      const g = s();
      g.time += dt;

      if (g.hp <= 0 && !g.winner) {
        if (!g.pRespawnAt) g.pRespawnAt = g.time + 3;
        if (g.time >= g.pRespawnAt) { g.pRespawnAt = 0; spawnAt(0); }
      } else {
        playerTick(dt);
      }
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
      if (document.pointerLockElement === canvas) document.exitPointerLock();
    };
  }, [screen]);

  /* ================= UI ================= */
  if (screen === "menu" || screen === "end") {
    return (
      <div className="min-h-screen bg-background text-foreground flex items-center justify-center px-6 py-16">
        <div className="max-w-3xl w-full">
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-3">
            WIRRWARR // Web-Demo v0.1
          </p>
          <h1 className="text-4xl md:text-5xl font-bold mb-2">
            {screen === "end" ? (
              <>Sieg: <span className="text-primary glow-neon">{hud.winner}</span></>
            ) : (
              <>Spiel die <span className="text-primary glow-neon">Prototyp-Arena</span></>
            )}
          </h1>
          <p className="text-muted-foreground mb-8 leading-relaxed">
            Raycaster-Prototyp mit Bot-Gegnern – alle sechs Modi aus dem GDD, runtergebrochen
            auf eine Arena. Steuerung: <span className="text-foreground font-mono text-sm">WASD</span> bewegen,{" "}
            <span className="text-foreground font-mono text-sm">Klick/Maus</span> zielen &amp; schießen,{" "}
            <span className="text-foreground font-mono text-sm">←/→</span> drehen,{" "}
            <span className="text-foreground font-mono text-sm">R</span> nachladen,{" "}
            <span className="text-foreground font-mono text-sm">E</span> pflanzen (Sabotage).
          </p>
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
          <a href="/" className="font-mono text-xs tracking-wider uppercase text-muted-foreground hover:text-primary transition-colors">
            ← Zurück zum GDD
          </a>
        </div>
      </div>
    );
  }

  return (
    <div className="relative h-screen w-full bg-black overflow-hidden select-none">
      <canvas ref={canvasRef} className="w-full h-full [image-rendering:pixelated] cursor-crosshair" />
      <div className="absolute top-3 left-1/2 -translate-x-1/2 text-center pointer-events-none">
        <p className="font-mono text-sm text-primary glow-neon-sm tracking-wider">{hud.scores}</p>
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider mt-1">{hud.objective}</p>
      </div>
      <div className="absolute top-3 right-3 text-right pointer-events-none space-y-1">
        {hud.feed.map((f, i) => (
          <p key={i} className="font-mono text-[10px] text-primary/90">{f}</p>
        ))}
      </div>
      <div className="absolute bottom-3 left-3 pointer-events-none">
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">INTEGRITÄT</p>
        <div className="w-40 h-1.5 bg-secondary rounded-full overflow-hidden mt-1">
          <div className={`h-full rounded-full ${hud.hp > 35 ? "bg-primary" : "bg-destructive"}`} style={{ width: `${hud.hp}%` }} />
        </div>
        <p className="font-mono text-xl text-primary leading-none mt-1">{hud.hp}</p>
      </div>
      <div className="absolute bottom-3 right-3 text-right pointer-events-none">
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">DORN</p>
        <p className="font-mono text-2xl text-foreground leading-none">
          {hud.reloading ? <span className="text-primary">LÄDT…</span> : <>{hud.ammo}<span className="text-muted-foreground text-sm">/∞</span></>}
        </p>
      </div>
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
