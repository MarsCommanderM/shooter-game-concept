"use client";

import { useEffect, useRef, useState } from "react";
import * as THREE from "three";

/* ================================================================== */
/* WIRRWARR – Vertical Slice (echte 3D-Engine, Three.js)               */
/* Phase 1: Destruction + Gunfeel + Bots in einer Arena                */
/* ================================================================== */

type PerkId = "sprint" | "panzer" | "sprung";
const PERKS: { id: PerkId; name: string; desc: string }[] = [
  { id: "sprint", name: "Myzel-Sprint", desc: "+20 % Sprint-Tempo" },
  { id: "panzer", name: "Chitin-Panzer", desc: "-30 % erlittener Schaden" },
  { id: "sprung", name: "Mantis-Sprung", desc: "Doppelsprung" },
];

type VsMode = "tdm" | "ffa";
type GameKind = VsMode | "m1" | "m2" | "m3" | "m4";
type ArenaId = "sektor" | "garten";

interface Mission {
  id: GameKind; title: string; briefing: string;
  type: "kills" | "destroy" | "survive";
  target: number; timeLimit?: number; botCount: number;
}

const MISSIONS: Mission[] = [
  { id: "m1", title: "M1 // Erste Ernte", briefing: "Die Biomass testet dich. Eliminiere 8 Eindringlinge – sie kommen immer wieder.", type: "kills", target: 8, botCount: 4 },
  { id: "m2", title: "M2 // Abrissunternehmen", briefing: "Sprenge 6 sprengbare Strukturen in 3 Minuten. Der BRECHER-7 ist dein bester Freund.", type: "destroy", target: 6, timeLimit: 180, botCount: 4 },
  { id: "m3", title: "M3 // Stellung halten", briefing: "Halte 120 Sekunden gegen endlose Wellen. Niemand kommt zu dir durch. Niemand.", type: "survive", target: 120, botCount: 6 },
  { id: "m4", title: "M4 // Das Nest", briefing: "Das Finale: 15 Eindringlinge zwischen dir und dem Nest. Brenn es nieder.", type: "kills", target: 15, botCount: 6 },
];

const CAMP_KEY = "wirrwarr-campaign-done";
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
function sShot(kind: "dorn" | "brecher") {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  if (kind === "brecher") {
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
function sStep() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "triangle"; o.frequency.setValueAtTime(95, t); o.frequency.exponentialRampToValueAtTime(55, t + 0.05);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.05, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.06);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.07);
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

export function RealGame() {
  const mountRef = useRef<HTMLDivElement>(null);
  const [screen, setScreen] = useState<"menu" | "game" | "end">("menu");
  const [winner, setWinner] = useState("");
  const [arena, setArena] = useState<ArenaId>("sektor");
  const [perk, setPerk] = useState<PerkId>("sprint");
  const [doneMissions, setDoneMissions] = useState<string[]>([]);
  const [failed, setFailed] = useState(false);
  useEffect(() => {
    if (screen === "menu" || screen === "end") setDoneMissions(loadCampaign());
  }, [screen]);
  const [hud, setHud] = useState({
    hp: 100, ammo: 24, reloading: false, weapon: "DORN",
    scores: "", feed: [] as string[], kills: 0, objective: "", tactics: false,
  });
  const apiRef = useRef<{ dispose: () => void } | null>(null);
  const feedRef = useRef<string[]>([]);

  useEffect(() => () => apiRef.current?.dispose(), []);

  const start = (kind: GameKind) => {
    setFailed(false);
    setScreen("game");
    // Engine startet im nächsten Frame, sobald das DIV gemountet ist
    requestAnimationFrame(() => initEngine(kind, arena, perk));
  };

  const initEngine = (mode: GameKind, arenaId: ArenaId, perk: PerkId) => {
    const mission = MISSIONS.find((m) => m.id === mode) ?? null;
    const mount = mountRef.current;
    if (!mount) return;

    /* ---------- Renderer / Scene / Camera ---------- */
    const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: "high-performance" });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(mount.clientWidth, mount.clientHeight);
    renderer.shadowMap.enabled = false;
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
    scene.add(dir);

    /* ---------- Boden ---------- */
    const ground = new THREE.Mesh(
      new THREE.PlaneGeometry(ARENA * 2, ARENA * 2),
      new THREE.MeshStandardMaterial({ color: 0x0a0f0a, roughness: 1 })
    );
    ground.rotation.x = -Math.PI / 2;
    scene.add(ground);
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
        : [
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
      if (particles.length > 220) {
        const rm = particles.splice(0, particles.length - 220);
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
      group.add(body, head);
      const sp = SPAWNS[(i + 1) % SPAWNS.length];
      group.position.set(sp[0], 0, sp[1]);
      scene.add(group);
      bots.push({
        id: i + 1, name: BOT_NAMES[i], team, group, body,
        hp: 100, alive: true, respawnAt: 0, cd: 1 + Math.random(),
        kills: 0, strafeDir: 1, strafeT: 0, color,
      });
    };
    const botCount = mission ? mission.botCount : 5;
    for (let i = 0; i < botCount; i++) makeBot(i, mission ? 1 : i % 2 === 0 ? 1 : 0);

    /* ---------- KI-Kameraden ---------- */
    interface AllyEnt {
      id: number; name: string; group: THREE.Group; body: THREE.Mesh;
      hp: number; alive: boolean; respawnAt: number; cd: number; kills: number;
      wp: { x: number; z: number; cmd: "hold" | "attack" } | null;
    }
    const allies: AllyEnt[] = [];
    const ALLY_HEX = [0x33ccff, 0x66ffcc];
    const ALLY_NAMES = ["VEGA", "JUNO"];
    for (let i = 0; i < 2; i++) {
      const group = new THREE.Group();
      const color = new THREE.Color(ALLY_HEX[i]);
      const body = new THREE.Mesh(
        new THREE.BoxGeometry(0.7, 1.5, 0.4),
        new THREE.MeshStandardMaterial({ color, roughness: 0.6, emissive: color.clone().multiplyScalar(0.3) })
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
      allies.push({ id: 101 + i, name: ALLY_NAMES[i], group, body, hp: 100, alive: true, respawnAt: 0, cd: 1, kills: 0, wp: null });
    }
    let tactics = false;

    /* ---------- Spieler-State ---------- */
    const player = {
      x: SPAWNS[0][0], z: SPAWNS[0][1], y: 0, vy: 0,
      vx: 0, vz: 0,
      hp: 100, ammo: 24, reloading: 0, fireCd: 0,
      weapon: "dorn" as "dorn" | "brecher",
      kills: 0, deaths: 0, respawnAt: 0,
      jumps: 0, jumpHeld: false, jbuf: 0, coyote: 0,
      crouch: false, prone: false, proneHeld: false, prevCrouch: false,
      slideT: 0, camH: 1.7, bobPhase: 0, landDip: 0, stepAcc: 0,
      mantleTarget: null as number | null,
    };
    yaw.position.set(player.x, 1.7, player.z);

    const teamScore = [0, 0];
    const ffaScore = Array(6).fill(0);
    let gameTime = 0;
    let ended = false;
    let missionKills = 0;
    let missionDestroyed = 0;
    const finishMission = (win: boolean) => {
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
      if (victimId === 0) { player.hp = 0; player.deaths++; player.respawnAt = gameTime + 3; }
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
      if (killerId === 0) { player.kills++; missionKills++; }
      if (mode === "ffa") { if (killerId < 6) ffaScore[killerId]++; }
      else teamScore[killerId === 0 || killerId >= 100 ? 0 : bots.find((x) => x.id === killerId)!.team]++;
      pushFeed(`${nameOf(killerId)} ⚡ ${nameOf(victimId)}`);
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
      if (tactics) {
        if (e.code === "Digit1") { allies.forEach((a) => { a.wp = null; }); pushFeed("Befehl: FOLGEN"); }
        if (e.code === "Digit2") { allies.forEach((a) => { a.wp = { x: a.group.position.x, z: a.group.position.z, cmd: "hold" }; }); pushFeed("Befehl: STELLUNG HALTEN"); }
        if (e.code === "Digit3") { allies.forEach((a) => { a.wp = a.wp ? { ...a.wp, cmd: "attack" } : { x: a.group.position.x, z: a.group.position.z, cmd: "hold" }; }); pushFeed("Befehl: WAYPOINT ANGREIFEN"); }
        return;
      }
      if (e.code === "KeyQ" || e.code === "Digit1" || e.code === "Digit2") {
        player.weapon = e.code === "KeyQ" ? (player.weapon === "dorn" ? "brecher" : "dorn") : e.code === "Digit1" ? "dorn" : "brecher";
        player.ammo = 24; player.reloading = 0;
      }
    };
    const ku = (e: KeyboardEvent) => { keys[e.code] = false; };
    const md = (e: MouseEvent) => {
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
    window.addEventListener("keydown", kd);
    window.addEventListener("keyup", ku);
    el.addEventListener("mousedown", md);
    window.addEventListener("mouseup", mu);
    window.addEventListener("mousemove", mm);

    /* ---------- Waffen-Viewmodel ---------- */
    const gun = new THREE.Group();
    const gunBody = new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.14, 0.7), new THREE.MeshStandardMaterial({ color: 0x1a1f1a, emissive: 0x0a2a10 }));
    const gunTip = new THREE.Mesh(new THREE.BoxGeometry(0.05, 0.05, 0.2), new THREE.MeshStandardMaterial({ color: 0x22ff55, emissive: 0x22ff55 }));
    gunTip.position.set(0, 0.02, -0.45);
    gun.add(gunBody, gunTip);
    gun.position.set(0.3, -0.28, -0.6);
    camera.add(gun);
    const muzzleLight = new THREE.PointLight(0x88ff88, 0, 6);
    muzzleLight.position.set(0.3, -0.2, -1);
    camera.add(muzzleLight);

    /* ---------- Schießen ---------- */
    const raycaster = new THREE.Raycaster();
    const shoot = () => {
      if (tactics) return;
      const rate = player.weapon === "brecher" ? 0.9 : 0.18;
      if (player.fireCd > 0 || player.reloading > 0 || player.ammo <= 0) return;
      player.fireCd = rate;
      player.ammo--;
      sShot(player.weapon);
      muzzleLight.intensity = 3;

      raycaster.setFromCamera(new THREE.Vector2(0, 0), camera);
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
        if (botHit && (mode === "ffa" || botHit.team === 1)) {
          const dmg = player.weapon === "brecher" ? 80 : 26;
          botHit.hp -= dmg;
          burst(h.point, 0xff5544, 6);
          if (botHit.hp <= 0) kill(0, botHit.id);
        } else if (!botHit) {
          const wall = walls.find((w) => w.active && w.mesh === h.object);
          if (wall) {
            if (player.weapon === "brecher" && wall.destructible) {
              wall.hp -= 100;
              burst(h.point, 0x22ff55, 14);
              const m = wall.mesh.material as THREE.MeshStandardMaterial;
              m.color.setHex(0x7a4d1f);
              if (wall.hp <= 0) {
                wall.active = false;
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
      tracer(muzzle, end, player.weapon === "brecher" ? 0xffcc33 : 0x22ff55);
      if (player.ammo === 0) player.reloading = 1.2;
    };

    /* ---------- Bot-AI ---------- */
    const losClear = (from: THREE.Vector3, to: THREE.Vector3) => {
      const d = to.clone().sub(from);
      const len = d.length();
      raycaster.set(from.clone().add(new THREE.Vector3(0, 1.4, 0)), d.normalize());
      raycaster.far = len;
      const meshes = walls.filter((w) => w.active).map((w) => w.mesh);
      const hit = raycaster.intersectObjects(meshes, false);
      raycaster.far = Infinity;
      return hit.length === 0;
    };

    const botTick = (b: BotEnt, dt: number) => {
      if (!b.alive) {
        if (gameTime >= b.respawnAt) {
          const sp = SPAWNS[b.id % SPAWNS.length];
          b.group.position.set(sp[0], 0, sp[1]);
          b.group.visible = true;
          b.hp = 100; b.alive = true;
        }
        return;
      }
      // Ziel suchen
      let target: { x: number; z: number; id: number; dist: number } | null = null;
      const bp = b.group.position;
      const dP = Math.hypot(player.x - bp.x, player.z - bp.z);
      const enemy = (team: number) => (mode === "ffa" ? team !== b.team : team !== b.team);
      if (dP < 30 && enemy(0) && player.hp > 0 && losClear(bp, new THREE.Vector3(player.x, 0, player.z))) {
        target = { x: player.x, z: player.z, id: 0, dist: dP };
      }
      for (const a of allies) {
        if (!a.alive) continue;
        const d = Math.hypot(a.group.position.x - bp.x, a.group.position.z - bp.z);
        if (d < 30 && (!target || d < target.dist) && losClear(bp, a.group.position)) {
          target = { x: a.group.position.x, z: a.group.position.z, id: a.id, dist: d };
        }
      }
      for (const o of bots) {
        if (o.id === b.id || !o.alive || !enemy(o.team)) continue;
        const d = Math.hypot(o.group.position.x - bp.x, o.group.position.z - bp.z);
        if (d < 30 && (!target || d < target.dist) && losClear(bp, o.group.position)) {
          target = { x: o.group.position.x, z: o.group.position.z, id: o.id, dist: d };
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
      if (!target && dist > 1) moveWithCollide(p2, (dx / dist) * 4.5 * dt, (dz / dist) * 4.5 * dt, 0.5, 0);
      else if (target && target.dist > 8) moveWithCollide(p2, (dx / dist) * 4 * dt, (dz / dist) * 4 * dt, 0.5, 0);
      if (target) moveWithCollide(p2, (-dz / dist) * b.strafeDir * 2.2 * dt, (dx / dist) * b.strafeDir * 2.2 * dt, 0.5, 0);
      b.group.position.x = p2.x;
      b.group.position.z = p2.z;
      b.group.rotation.y = Math.atan2(dx, dz);

      // Schießen
      b.cd -= dt;
      if (target && b.cd <= 0) {
        b.cd = 0.6 + Math.random() * 0.6;
        const from = bp.clone().add(new THREE.Vector3(0, 1.4, 0));
        const to = new THREE.Vector3(target.x, 1.4, target.z);
        tracer(from, to, b.color.getHex());
        sShot("dorn");
        const stanceMul = player.prone ? 0.6 : player.crouch ? 0.8 : 1;
        const dmg = (7 + Math.random() * 9) * (perk === "panzer" ? 0.7 : 1) * stanceMul;
        if (target.id === 0) {
          player.hp -= dmg;
          if (player.hp <= 0) kill(b.id, 0);
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
          a.hp = 100; a.alive = true;
          pushFeed(`${a.name} wieder einsatzbereit`);
        }
        return;
      }
      const bp = a.group.position;
      // Ziel: nächster sichtbarer Bot
      let target: { x: number; z: number; id: number; dist: number } | null = null;
      for (const o of bots) {
        if (!o.alive) continue;
        const d = Math.hypot(o.group.position.x - bp.x, o.group.position.z - bp.z);
        if (d < 28 && (!target || d < target.dist) && losClear(bp, o.group.position)) {
          target = { x: o.group.position.x, z: o.group.position.z, id: o.id, dist: d };
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
        moveWithCollide(p2, (dx / dist) * 5.2 * dt, (dz / dist) * 5.2 * dt, 0.5, 0);
        bp.x = p2.x; bp.z = p2.z;
      }
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

    /* ---------- Loop ---------- */
    const clock = new THREE.Clock();
    let raf = 0;
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
          player.hp = 100; player.x = SPAWNS[0][0]; player.z = SPAWNS[0][1]; player.ammo = 24;
        }
      } else {
        // ===== Bewegungsfluss: Stance, Slide, Acceleration, Friction =====
        const support = getSupport(player.x, player.z, player.y);
        const onGround = player.y <= support + 0.02;
        if (onGround) player.coyote = 0.12; else player.coyote = Math.max(0, player.coyote - dt);

        const wantCrouch = !!(keys["KeyC"] || keys["ControlLeft"]);
        if (keys["KeyX"] && !player.proneHeld) player.prone = !player.prone;
        player.proneHeld = !!keys["KeyX"];
        player.crouch = player.prone || wantCrouch;

        const speedNow0 = Math.hypot(player.vx, player.vz);
        if (wantCrouch && !player.prevCrouch && speedNow0 > 5.5 && onGround) player.slideT = 0.75;
        player.prevCrouch = wantCrouch;
        player.slideT = Math.max(0, player.slideT - dt);
        const sliding = player.slideT > 0 && onGround;

        const sprinting = !!keys["ShiftLeft"] && !player.crouch && speedNow0 > 4;
        const maxSp = player.prone ? 1.5 : player.crouch ? 2.6 : sprinting ? (perk === "sprint" ? 9.6 : 8.6) : 5.2;

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
        else if (player.jbuf > 0 && player.coyote > 0 && !player.crouch) { player.vy = 7.6; player.jumps = 1; player.coyote = 0; player.jbuf = 0; }
        else if (player.jbuf > 0 && !onGround && perk === "sprung" && player.jumps === 1 && !player.jumpHeld) { player.vy = 7; player.jumps = 2; player.jbuf = 0; }

        // Gravitation + weiche Landung
        player.vy -= 20 * dt;
        const newY = player.y + player.vy * dt;
        if (newY <= support && player.vy <= 0) {
          if (player.vy < -7) { player.landDip = 0.16; sLand(); }
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
          player.y += (player.mantleTarget - player.y) * Math.min(1, 10 * dt);
          moveWithCollide(player, dxm * 1.5, dzm * 1.5, 0.5, player.y);
          if (Math.abs(player.mantleTarget - player.y) < 0.04) { player.y = player.mantleTarget; player.mantleTarget = null; }
        }

        // Footsteps
        const speedNow = Math.hypot(player.vx, player.vz);
        if (onGround && speedNow > 1.5 && !sliding) {
          player.stepAcc += speedNow * dt;
          if (player.stepAcc > 2.4) { player.stepAcc = 0; sStep(); }
        }

        if (player.reloading > 0) { player.reloading -= dt; if (player.reloading <= 0) player.ammo = 24; }
        if (keys["KeyR"] && player.ammo < 24 && player.reloading <= 0) player.reloading = 1.2;
        if (mouseDown) shoot();

        // Gun-Bob + Sway aus echtem Bewegungsphasenwert
        const gspd = Math.hypot(player.vx, player.vz);
        gun.position.y = -0.28 + Math.sin(player.bobPhase) * Math.min(0.014, gspd * 0.0018);
        gun.position.x = 0.3 + Math.cos(player.bobPhase * 0.5) * Math.min(0.01, gspd * 0.0012);
      }

      // ===== Kamera-Flow: Maus-Smoothing, Stance-Höhe, Bob, Lande-Dip, FOV-Kick =====
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

      if (!tactics) {
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

      // Siegbedingung
      if (!ended) {
        if (mission) {
          if (mission.type === "kills" && missionKills >= mission.target) finishMission(true);
          else if (mission.type === "destroy" && missionDestroyed >= mission.target) finishMission(true);
          else if (mission.type === "survive" && gameTime >= mission.target) finishMission(true);
          else if (mission.timeLimit && gameTime >= mission.timeLimit) finishMission(false);
        } else {
          const limit = mode === "ffa" ? 8 : 10;
          if (mode === "ffa") {
            const best = ffaScore.indexOf(Math.max(...ffaScore));
            if (ffaScore[best] >= limit) { ended = true; setWinner(best === 0 ? "DU" : BOT_NAMES[best - 1]); setScreen("end"); }
          } else {
            if (teamScore[0] >= limit) { ended = true; setWinner("TEAM GRÜN"); setScreen("end"); }
            if (teamScore[1] >= limit) { ended = true; setWinner("TEAM ROT"); setScreen("end"); }
          }
        }
      }

      // HUD
      hudAcc += dt;
      if (hudAcc > 0.12) {
        hudAcc = 0;
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
          objective: mission
            ? mission.type === "kills"
              ? `${mission.title} · Kills ${missionKills}/${mission.target}`
              : mission.type === "destroy"
                ? `${mission.title} · Strukturen ${missionDestroyed}/${mission.target} · ⏱ ${Math.max(0, (mission.timeLimit ?? 0) - gameTime).toFixed(0)} s`
                : `${mission.title} · Halte durch · ⏱ ${Math.max(0, mission.target - gameTime).toFixed(0)} s`
            : "",
          tactics,
        });
      }

      renderer.render(scene, camera);
    };
    loop();

    const onResize = () => {
      renderer.setSize(mount.clientWidth, mount.clientHeight);
      camera.aspect = mount.clientWidth / mount.clientHeight;
      camera.updateProjectionMatrix();
    };
    window.addEventListener("resize", onResize);

    apiRef.current = {
      dispose: () => {
        cancelAnimationFrame(raf);
        window.removeEventListener("keydown", kd);
        window.removeEventListener("keyup", ku);
        window.removeEventListener("mouseup", mu);
        window.removeEventListener("mousemove", mm);
        window.removeEventListener("resize", onResize);
        el.removeEventListener("mousedown", md);
        if (document.pointerLockElement === el) document.exitPointerLock();
        renderer.dispose();
        if (el.parentElement === mount) mount.removeChild(el);
      },
    };
  };

  /* ================= UI ================= */
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
          <p className="text-muted-foreground mb-8 leading-relaxed">
            Three.js-Engine: echte 3D-Physik, Hitscan-Gunplay mit Tracern &amp; Partikeln,
            <span className="text-primary"> persistente 3D-Destruction</span> (BRECHER-7 reißt Löcher in die Arena),
            Bot-KI mit Line-of-Sight. Steuerung: <span className="font-mono text-foreground">Klick</span> = Maus fangen,{" "}
            <span className="font-mono text-foreground">WASD</span>, <span className="font-mono text-foreground">Shift</span> Sprint,{" "}
            <span className="font-mono text-foreground">C</span> Ducken (aus dem Sprint = <span className="text-primary">Slide</span>),{" "}
            <span className="font-mono text-foreground">X</span> Hinlegen, <span className="font-mono text-foreground">Space</span> Springen,{" "}
            <span className="font-mono text-foreground">Q/1/2</span> Waffen, <span className="font-mono text-foreground">R</span> laden,{" "}
            <span className="font-mono text-foreground">T</span> Taktik. Klettern &amp; Mantling laufen automatisch im Bewegungsfluss.
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
      <div className="absolute top-3 left-1/2 -translate-x-1/2 text-center pointer-events-none max-w-[80%]">
        <p className="font-mono text-sm text-primary glow-neon-sm tracking-wider">{hud.scores}</p>
        {hud.objective && <p className="font-mono text-[10px] text-foreground/90 tracking-wider mt-1">{hud.objective}</p>}
      </div>
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
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">INTEGRITÄT</p>
        <div className="w-40 h-1.5 bg-secondary rounded-full overflow-hidden mt-1">
          <div className={`h-full rounded-full ${hud.hp > 35 ? "bg-primary" : "bg-destructive"}`} style={{ width: `${hud.hp}%` }} />
        </div>
        <p className="font-mono text-xl text-primary leading-none mt-1">{hud.hp}</p>
      </div>
      <div className="absolute bottom-3 right-3 text-right pointer-events-none">
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider">{hud.weapon} <span className="text-primary">[Q]</span></p>
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
