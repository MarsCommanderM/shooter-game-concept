"use client";

import { useEffect, useRef, useState } from "react";
import * as THREE from "three";

/* ================================================================== */
/* WIRRWARR – Vertical Slice (echte 3D-Engine, Three.js)               */
/* Phase 1: Destruction + Gunfeel + Bots in einer Arena                */
/* ================================================================== */

type VsMode = "tdm" | "ffa";

interface WallBox {
  mesh: THREE.Mesh;
  hw: number;
  hd: number;
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

/* ================================================================== */

export function RealGame() {
  const mountRef = useRef<HTMLDivElement>(null);
  const [screen, setScreen] = useState<"menu" | "game" | "end">("menu");
  const [winner, setWinner] = useState("");
  const [hud, setHud] = useState({
    hp: 100, ammo: 24, reloading: false, weapon: "DORN",
    scores: "", feed: [] as string[], kills: 0,
  });
  const apiRef = useRef<{ dispose: () => void } | null>(null);
  const feedRef = useRef<string[]>([]);

  useEffect(() => () => apiRef.current?.dispose(), []);

  const start = (mode: VsMode) => {
    setScreen("game");
    // Engine startet im nächsten Frame, sobald das DIV gemountet ist
    requestAnimationFrame(() => initEngine(mode));
  };

  const initEngine = (mode: VsMode) => {
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
      walls.push({ mesh, hw: w / 2, hd: d / 2, hp, maxHp: hp, destructible, active: true });
    };
    // Außenwände (massiv)
    addWall(0, 2, -ARENA, ARENA * 2, 4, 1, false);
    addWall(0, 2, ARENA, ARENA * 2, 4, 1, false);
    addWall(-ARENA, 2, 0, 1, 4, ARENA * 2, false);
    addWall(ARENA, 2, 0, 1, 4, ARENA * 2, false);
    // Deckungen & sprengbare Strukturen
    const structs: [number, number, number, number, number, number, boolean][] = [
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
    const collides = (x: number, z: number, r: number) => {
      for (const w of walls) {
        if (!w.active) continue;
        const p = w.mesh.position;
        const hw = w.hw + r, hd = w.hd + r;
        if (x > p.x - hw && x < p.x + hw && z > p.z - hd && z < p.z + hd) return true;
      }
      return false;
    };
    const moveWithCollide = (pos: { x: number; z: number }, dx: number, dz: number, r: number) => {
      if (!collides(pos.x + dx, pos.z, r)) pos.x += dx;
      if (!collides(pos.x, pos.z + dz, r)) pos.z += dz;
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
    for (let i = 0; i < 5; i++) makeBot(i, i % 2 === 0 ? 1 : 0);

    /* ---------- Spieler-State ---------- */
    const player = {
      x: SPAWNS[0][0], z: SPAWNS[0][1], y: 0, vy: 0,
      hp: 100, ammo: 24, reloading: 0, fireCd: 0,
      weapon: "dorn" as "dorn" | "brecher",
      kills: 0, deaths: 0, respawnAt: 0,
    };
    yaw.position.set(player.x, 1.7, player.z);

    const teamScore = [0, 0];
    const ffaScore = Array(6).fill(0);
    let gameTime = 0;
    let ended = false;

    const pushFeed = (t: string) => {
      feedRef.current.push(t);
      if (feedRef.current.length > 5) feedRef.current.shift();
    };

    const kill = (killerId: number, victimId: number) => {
      const kName = killerId === 0 ? "DU" : BOT_NAMES[killerId - 1] ?? "?";
      const vName = victimId === 0 ? "DU" : BOT_NAMES[victimId - 1] ?? "?";
      if (victimId === 0) { player.hp = 0; player.deaths++; player.respawnAt = gameTime + 3; }
      else {
        const b = bots.find((x) => x.id === victimId)!;
        b.hp = 0; b.alive = false; b.respawnAt = gameTime + 3;
        burst(b.group.position.clone().add(new THREE.Vector3(0, 1, 0)), b.color.getHex(), 18);
        b.group.visible = false;
      }
      if (killerId > 0) bots.find((x) => x.id === killerId)!.kills++;
      if (killerId === 0) player.kills++;
      if (mode === "ffa") ffaScore[killerId]++;
      else teamScore[killerId === 0 ? 0 : bots.find((x) => x.id === killerId)!.team]++;
      pushFeed(`${kName} ⚡ ${vName}`);
    };

    /* ---------- Input ---------- */
    const keys: Record<string, boolean> = {};
    let mouseDown = false;
    let lastMX = 0;
    const el = renderer.domElement;
    const kd = (e: KeyboardEvent) => {
      keys[e.code] = true;
      if (e.code === "KeyQ" || e.code === "Digit1" || e.code === "Digit2") {
        player.weapon = e.code === "KeyQ" ? (player.weapon === "dorn" ? "brecher" : "dorn") : e.code === "Digit1" ? "dorn" : "brecher";
        player.ammo = 24; player.reloading = 0;
      }
    };
    const ku = (e: KeyboardEvent) => { keys[e.code] = false; };
    const md = () => { mouseDown = true; try { el.requestPointerLock(); } catch { /* */ } };
    const mu = () => { mouseDown = false; };
    const mm = (e: MouseEvent) => {
      if (document.pointerLockElement === el) {
        yaw.rotation.y -= e.movementX * 0.0022;
        pitch.rotation.x = Math.max(-1.4, Math.min(1.4, pitch.rotation.x - e.movementY * 0.0022));
      } else if (mouseDown) {
        yaw.rotation.y -= (e.clientX - lastMX) * 0.004;
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
      if (!target && dist > 1) moveWithCollide(p2, (dx / dist) * 4.5 * dt, (dz / dist) * 4.5 * dt, 0.5);
      else if (target && target.dist > 8) moveWithCollide(p2, (dx / dist) * 4 * dt, (dz / dist) * 4 * dt, 0.5);
      if (target) moveWithCollide(p2, (-dz / dist) * b.strafeDir * 2.2 * dt, (dx / dist) * b.strafeDir * 2.2 * dt, 0.5);
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
        const dmg = 7 + Math.random() * 9;
        if (target.id === 0) {
          player.hp -= dmg;
          if (player.hp <= 0) kill(b.id, 0);
        } else {
          const v = bots.find((x) => x.id === target!.id)!;
          v.hp -= dmg;
          if (v.hp <= 0) kill(b.id, v.id);
        }
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
        const sprint = keys["ShiftLeft"] ? 1.5 : 1;
        const sp = 7 * dt * sprint;
        let fx = 0, fz = 0;
        const fy_ = yaw.rotation.y;
        if (keys["KeyW"]) { fx -= Math.sin(fy_); fz -= Math.cos(fy_); }
        if (keys["KeyS"]) { fx += Math.sin(fy_); fz += Math.cos(fy_); }
        if (keys["KeyA"]) { fx -= Math.cos(fy_); fz += Math.sin(fy_); }
        if (keys["KeyD"]) { fx += Math.cos(fy_); fz -= Math.sin(fy_); }
        const l = Math.hypot(fx, fz);
        if (l > 0.01) moveWithCollide(player, (fx / l) * sp, (fz / l) * sp, 0.5);
        // Jump/Gravity
        if (keys["Space"] && player.y === 0) player.vy = 7.5;
        player.vy -= 20 * dt;
        player.y = Math.max(0, player.y + player.vy * dt);
        if (player.y === 0) player.vy = 0;

        if (player.reloading > 0) { player.reloading -= dt; if (player.reloading <= 0) player.ammo = 24; }
        if (keys["KeyR"] && player.ammo < 24 && player.reloading <= 0) player.reloading = 1.2;
        if (mouseDown) shoot();

        // Gun-Bob
        const moving = l > 0.01;
        gun.position.y = -0.28 + (moving ? Math.sin(gameTime * 10) * 0.012 : 0);
      }
      yaw.position.set(player.x, 1.7 + player.y, player.z);

      for (const b of bots) botTick(b, dt);

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
        const limit = mode === "ffa" ? 8 : 10;
        if (mode === "ffa") {
          const best = ffaScore.indexOf(Math.max(...ffaScore));
          if (ffaScore[best] >= limit) { ended = true; setWinner(best === 0 ? "DU" : BOT_NAMES[best - 1]); setScreen("end"); }
        } else {
          if (teamScore[0] >= limit) { ended = true; setWinner("TEAM GRÜN"); setScreen("end"); }
          if (teamScore[1] >= limit) { ended = true; setWinner("TEAM ROT"); setScreen("end"); }
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
              <>Sieg: <span className="text-primary glow-neon">{winner}</span></>
            ) : (
              <>Das <span className="text-primary glow-neon">echte Game</span> startet hier</>
            )}
          </h1>
          <p className="text-muted-foreground mb-8 leading-relaxed">
            Three.js-Engine: echte 3D-Physik, Hitscan-Gunplay mit Tracern &amp; Partikeln,
            <span className="text-primary"> persistente 3D-Destruction</span> (BRECHER-7 reißt Löcher in die Arena),
            Bot-KI mit Line-of-Sight. Steuerung: <span className="font-mono text-foreground">Klick</span> = Maus fangen,{" "}
            <span className="font-mono text-foreground">WASD</span>, <span className="font-mono text-foreground">Space</span> Jump,{" "}
            <span className="font-mono text-foreground">Shift</span> Sprint, <span className="font-mono text-foreground">Q/1/2</span> Waffen,{" "}
            <span className="font-mono text-foreground">R</span> laden.
          </p>
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
      <div className="absolute top-3 left-1/2 -translate-x-1/2 text-center pointer-events-none">
        <p className="font-mono text-sm text-primary glow-neon-sm tracking-wider">{hud.scores}</p>
      </div>
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
