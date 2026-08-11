"use client";
/* NOVA Web-Client: Three.js (bewährt auf iOS) gegen den Rust-Dedicated-Server (WS JSON).
   Optik-Paket: ACES, Dämmerungs-Sky, prozedurale Fenster/Asphalt-Textures, Shadows,
   Environment-Reflections, humanoide Soldaten, Muzzle-Flash. */
import { useEffect, useRef, useState } from "react";
import * as THREE from "three";
import { NOVA_BOXES } from "./nova-boxes";

const WEAPONS = [
  { name: "M16", rate: 0.11 },
  { name: "AK-47", rate: 0.14 },
  { name: "MP5", rate: 0.075 },
  { name: "MG4", rate: 0.1 },
  { name: "PUMP", rate: 0.8 },
];

/* ---------------- Audio ---------------- */
let actx: AudioContext | null = null;
function ac(): AudioContext | null {
  if (typeof window === "undefined") return null;
  const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
  if (!AC) return null;
  if (!actx) actx = new AC();
  if (actx.state === "suspended") void actx.resume();
  return actx;
}
function shotSound(w: number) {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const len = Math.floor(c.sampleRate * (w === 4 ? 0.3 : 0.12));
  const buf = c.createBuffer(1, len, c.sampleRate);
  const d = buf.getChannelData(0);
  for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / len);
  const src = c.createBufferSource(); src.buffer = buf;
  const f = c.createBiquadFilter(); f.type = "lowpass";
  f.frequency.setValueAtTime(w === 4 ? 900 : w === 2 ? 3200 : 2200, t);
  const g = c.createGain();
  g.gain.setValueAtTime(w === 4 ? 0.8 : 0.5, t);
  g.gain.exponentialRampToValueAtTime(0.0001, t + (w === 4 ? 0.3 : 0.12));
  src.connect(f); f.connect(g); g.connect(c.destination);
  src.start(t);
  const o = c.createOscillator(); const og = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(110, t); o.frequency.exponentialRampToValueAtTime(40, t + 0.15);
  og.gain.setValueAtTime(0.4, t); og.gain.exponentialRampToValueAtTime(0.0001, t + 0.18);
  o.connect(og); og.connect(c.destination); o.start(t); o.stop(t + 0.2);
}
function blip(f0: number, f1: number, vol: number) {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "sine"; o.frequency.setValueAtTime(f0, t); o.frequency.exponentialRampToValueAtTime(Math.max(1, f1), t + 0.08);
  g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.1);
  o.connect(g); g.connect(c.destination); o.start(t); o.stop(t + 0.12);
}

/* ---------------- prozedurale Texturen ---------------- */
function canvasTex(size: number, draw: (c: CanvasRenderingContext2D, s: number) => void): THREE.CanvasTexture {
  const cv = document.createElement("canvas");
  cv.width = cv.height = size;
  const c = cv.getContext("2d")!;
  draw(c, size);
  const t = new THREE.CanvasTexture(cv);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
function asphaltTex(): THREE.CanvasTexture {
  return canvasTex(256, (c, s) => {
    c.fillStyle = "#2b2b2e"; c.fillRect(0, 0, s, s);
    for (let i = 0; i < 2200; i++) {
      c.fillStyle = `rgba(${120 + Math.random() * 60},${120 + Math.random() * 60},${125 + Math.random() * 60},${Math.random() * 0.08})`;
      c.fillRect(Math.random() * s, Math.random() * s, 1.5, 1.5);
    }
    for (let i = 0; i < 8; i++) {
      c.fillStyle = `rgba(20,20,22,${0.2 + Math.random() * 0.2})`;
      c.beginPath(); c.arc(Math.random() * s, Math.random() * s, 6 + Math.random() * 18, 0, 7); c.fill();
    }
  });
}
function roadTex(): THREE.CanvasTexture {
  return canvasTex(256, (c, s) => {
    c.fillStyle = "#26262a"; c.fillRect(0, 0, s, s);
    for (let i = 0; i < 1500; i++) {
      c.fillStyle = `rgba(140,140,150,${Math.random() * 0.07})`;
      c.fillRect(Math.random() * s, Math.random() * s, 1.5, 1.5);
    }
    c.fillStyle = "#8a7a20";
    for (let y = 0; y < s; y += 64) c.fillRect(s / 2 - 4, y, 8, 36); // Mittelstreifen gestrichelt
    c.fillStyle = "#55555a"; c.fillRect(8, 0, 6, s); c.fillRect(s - 14, 0, 6, s); // Randstreifen
  });
}
function facadeTex(): { map: THREE.CanvasTexture; emi: THREE.CanvasTexture } {
  const cv = document.createElement("canvas"); cv.width = 128; cv.height = 256;
  const c = cv.getContext("2d")!;
  const ev = document.createElement("canvas"); ev.width = 128; ev.height = 256;
  const e = ev.getContext("2d")!;
  c.fillStyle = "#3a3d45"; c.fillRect(0, 0, 128, 256);
  e.fillStyle = "#000"; e.fillRect(0, 0, 128, 256);
  for (let i = 0; i < 900; i++) {
    c.fillStyle = `rgba(255,255,255,${Math.random() * 0.05})`;
    c.fillRect(Math.random() * 128, Math.random() * 256, 2, 2);
  }
  for (let y = 12; y < 256; y += 28) {
    for (let x = 10; x < 128; x += 24) {
      c.fillStyle = "#14161c"; c.fillRect(x, y, 14, 16);
      if (Math.random() < 0.45) {
        const warm = Math.random() < 0.75;
        e.fillStyle = warm ? "#ffb066" : "#9fd8ff";
        e.fillRect(x, y, 14, 16);
        c.fillStyle = warm ? "#c08050" : "#7090a8"; c.fillRect(x, y, 14, 16);
      }
    }
  }
  const map = new THREE.CanvasTexture(cv); map.colorSpace = THREE.SRGBColorSpace;
  const emi = new THREE.CanvasTexture(ev); emi.colorSpace = THREE.SRGBColorSpace;
  return { map, emi };
}
function crateTex(): THREE.CanvasTexture {
  return canvasTex(128, (c, s) => {
    c.fillStyle = "#7a4d1f"; c.fillRect(0, 0, s, s);
    c.strokeStyle = "#5a3512"; c.lineWidth = 6;
    c.strokeRect(4, 4, s - 8, s - 8);
    c.beginPath(); c.moveTo(0, 0); c.lineTo(s, s); c.moveTo(s, 0); c.lineTo(0, s); c.stroke();
    for (let i = 0; i < 400; i++) {
      c.fillStyle = `rgba(40,20,5,${Math.random() * 0.15})`;
      c.fillRect(Math.random() * s, Math.random() * s, 2, 2);
    }
  });
}

/* ---------------- humanoider Soldat ---------------- */
function makeSoldier(color: number): THREE.Group {
  const g = new THREE.Group();
  const armor = new THREE.MeshStandardMaterial({ color: 0x23282e, roughness: 0.55, metalness: 0.4 });
  const trim = new THREE.MeshStandardMaterial({ color, roughness: 0.4, metalness: 0.3, emissive: color, emissiveIntensity: 0.35 });
  const body = new THREE.Mesh(new THREE.BoxGeometry(0.55, 0.85, 0.32), armor);
  body.position.y = 1.05; body.castShadow = true;
  const head = new THREE.Mesh(new THREE.BoxGeometry(0.32, 0.34, 0.34), armor);
  head.position.y = 1.66; head.castShadow = true;
  const visor = new THREE.Mesh(new THREE.BoxGeometry(0.26, 0.08, 0.05), new THREE.MeshStandardMaterial({ color, emissive: color, emissiveIntensity: 2.5 }));
  visor.position.set(0, 1.68, 0.17);
  const core = new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.2, 0.05), new THREE.MeshStandardMaterial({ color, emissive: color, emissiveIntensity: 1.4 }));
  core.position.set(0, 1.12, 0.17);
  const shL = new THREE.Mesh(new THREE.BoxGeometry(0.22, 0.12, 0.26), trim); shL.position.set(-0.4, 1.42, 0);
  const shR = shL.clone(); shR.position.x = 0.4;
  const legG = new THREE.BoxGeometry(0.18, 0.65, 0.22); legG.translate(0, -0.32, 0);
  const lL = new THREE.Mesh(legG, armor); lL.position.set(-0.15, 0.65, 0);
  const lR = new THREE.Mesh(legG, armor); lR.position.set(0.15, 0.65, 0);
  const armG = new THREE.BoxGeometry(0.13, 0.55, 0.15); armG.translate(0, -0.25, 0);
  const aL = new THREE.Mesh(armG, armor); aL.position.set(-0.4, 1.4, 0);
  const aR = new THREE.Mesh(armG, armor); aR.position.set(0.4, 1.4, 0);
  const gun = new THREE.Mesh(new THREE.BoxGeometry(0.08, 0.1, 0.6), new THREE.MeshStandardMaterial({ color: 0x111318, metalness: 0.7, roughness: 0.35 }));
  gun.position.set(0.05, -0.45, 0.25); aR.add(gun);
  g.add(body, head, visor, core, shL, shR, lL, lR, aL, aR);
  return g;
}

export function NovaWeb() {
  const mountRef = useRef<HTMLDivElement>(null);
  const [hud, setHud] = useState({ hp: 100, weapon: 0, score: "GRÜN 0 : 0 ROT", feed: [] as string[], conn: false });
  const fireRef = useRef(false);

  useEffect(() => {
    const mount = mountRef.current;
    if (!mount) return;
    const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: "high-performance" });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(mount.clientWidth, mount.clientHeight);
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.3;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    mount.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    scene.fog = new THREE.FogExp2(0x1a1410, 0.008);
    const camera = new THREE.PerspectiveCamera(75, mount.clientWidth / mount.clientHeight, 0.1, 600);

    /* ---------- Dämmerungs-Sky ---------- */
    const sky = new THREE.Mesh(
      new THREE.SphereGeometry(400, 24, 16),
      new THREE.ShaderMaterial({
        side: THREE.BackSide,
        uniforms: {},
        vertexShader: "varying vec3 vP; void main(){ vP = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }",
        fragmentShader: `varying vec3 vP;
          void main(){
            vec3 d = normalize(vP);
            float h = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
            vec3 col = mix(vec3(0.55, 0.22, 0.08), vec3(0.03, 0.05, 0.14), pow(h, 0.5));
            vec3 sund = normalize(vec3(-0.55, 0.22, -0.35));
            float sd = max(dot(d, sund), 0.0);
            col += vec3(1.0, 0.45, 0.15) * (pow(sd, 500.0) * 6.0 + pow(sd, 6.0) * 0.35);
            vec3 st = floor(d * 260.0);
            float hsh = fract(sin(dot(st.xz + st.y, vec2(12.9898, 78.233))) * 43758.5453);
            col += vec3(0.8, 0.9, 1.0) * step(0.9985, hsh) * clamp(d.y * 3.0, 0.0, 1.0) * 0.6;
            gl_FragColor = vec4(col, 1.0);
          }`,
      })
    );
    scene.add(sky);

    /* ---------- Licht ---------- */
    const hemi = new THREE.HemisphereLight(0x667799, 0x33241a, 0.7);
    scene.add(hemi);
    const sun = new THREE.DirectionalLight(0xffa060, 2.2);
    sun.position.set(-55, 30, -35);
    sun.castShadow = true;
    sun.shadow.mapSize.set(1024, 1024);
    sun.shadow.camera.left = -70; sun.shadow.camera.right = 70;
    sun.shadow.camera.top = 70; sun.shadow.camera.bottom = -70;
    sun.shadow.camera.far = 200;
    sun.shadow.bias = -0.0005;
    scene.add(sun);

    /* ---------- Environment-Reflections (Metall glänzt) ---------- */
    const pmrem = new THREE.PMREMGenerator(renderer);
    const envScene = new THREE.Scene();
    envScene.add(sky.clone());
    const gl = new THREE.Mesh(new THREE.PlaneGeometry(200, 200), new THREE.MeshBasicMaterial({ color: 0x443322 }));
    gl.rotation.x = -Math.PI / 2; envScene.add(gl);
    scene.environment = pmrem.fromScene(envScene, 0.04).texture;

    /* ---------- Texturen ---------- */
    const asp = asphaltTex(); asp.repeat.set(24, 24);
    const road = roadTex(); road.repeat.set(2, 2);
    const fac = facadeTex(); fac.map.repeat.set(2, 1); fac.emi.repeat.set(2, 1);
    const crt = crateTex();

    const MATS: Record<number, THREE.MeshStandardMaterial> = {
      0: new THREE.MeshStandardMaterial({ map: asp, roughness: 0.95 }),
      1: new THREE.MeshStandardMaterial({ color: 0x22ff55, emissive: 0x22ff55, emissiveIntensity: 2.2 }),
      2: new THREE.MeshStandardMaterial({ map: crt, roughness: 0.75 }),
      3: new THREE.MeshStandardMaterial({ color: 0x9aa0a8, metalness: 0.85, roughness: 0.3 }),
      4: new THREE.MeshStandardMaterial({ color: 0x33ccff, emissive: 0x33ccff, emissiveIntensity: 1.6 }),
      5: new THREE.MeshStandardMaterial({ color: 0x2a2e36, metalness: 0.6, roughness: 0.4 }),
      6: new THREE.MeshStandardMaterial({ map: fac.map, emissiveMap: fac.emi, emissive: 0xffffff, emissiveIntensity: 1.1, roughness: 0.85 }),
      7: new THREE.MeshStandardMaterial({ color: 0x6b4a2a, roughness: 0.9 }),
      8: new THREE.MeshStandardMaterial({ color: 0x7a3020, metalness: 0.75, roughness: 0.45 }),
      9: new THREE.MeshStandardMaterial({ color: 0x2e6b2a, roughness: 0.95 }),
      10: new THREE.MeshStandardMaterial({ color: 0x33363c, metalness: 0.6, roughness: 0.5 }),
      11: new THREE.MeshStandardMaterial({ color: 0xffcc88, emissive: 0xffaa44, emissiveIntensity: 3.5 }),
    };
    for (const [x, y, z, hx, hy, hz, kind] of NOVA_BOXES) {
      const m = new THREE.Mesh(new THREE.BoxGeometry(hx * 2, hy * 2, hz * 2), MATS[kind] ?? MATS[0]);
      m.position.set(x, y, z);
      if (kind === 0 && hy < 1) { m.material = new THREE.MeshStandardMaterial({ map: road, roughness: 0.95 }); (m.material.map as THREE.CanvasTexture).repeat.set(4, 4); }
      m.receiveShadow = true;
      scene.add(m);
    }
    // Straßen-Overlay: zwei Road-Strips über dem Boden
    const stripGeo = new THREE.PlaneGeometry(10, 96);
    const stripMat = new THREE.MeshStandardMaterial({ map: road, roughness: 0.9 });
    (stripMat.map as THREE.CanvasTexture).repeat.set(1, 6);
    const s1 = new THREE.Mesh(stripGeo, stripMat); s1.rotation.x = -Math.PI / 2; s1.position.set(0, 0.02, 0); s1.receiveShadow = true;
    const s2 = s1.clone(); s2.rotation.z = Math.PI / 2; s2.rotation.x = -Math.PI / 2; s2.rotation.order = "ZYX";
    scene.add(s1, s2);

    /* ---------- State ---------- */
    const pos = new THREE.Vector3(40, 1.7, 4);
    let yaw = Math.PI / 2, pitch = -0.1;
    let hp = 100, weapon = 0, fireCd = 0, seq = 0, myId = 0;
    const joy = { id: -1, ox: 0, oy: 0, x: 0, y: 0 };
    const look = { id: -1, lx: 0, ly: 0 };
    const remotes = new Map<number, { g: THREE.Group; tx: number; tz: number; tyaw: number }>();
    const tracers: { line: THREE.Line; life: number }[] = [];
    const feed: string[] = [];
    const pushFeed = (s: string) => { feed.unshift(s); if (feed.length > 5) feed.pop(); };
    const muzzle = new THREE.PointLight(0xffcc66, 0, 8);
    scene.add(muzzle);
    let muzzleT = 0;

    /* ---------- WebSocket ---------- */
    const host = typeof window !== "undefined" ? window.location.hostname : "169.58.152.88";
    let ws: WebSocket | null = null;
    let connOk = false;
    let lastTick = 0;
    const connect = () => {
      ws = new WebSocket(`ws://${host}:27016/?enc=json`);
      ws.binaryType = "arraybuffer";
      ws.onopen = () => { connOk = true; ws?.send(JSON.stringify({ Join: { name: "PILOT" } })); };
      ws.onclose = () => { connOk = false; setTimeout(connect, 2000); };
      ws.onmessage = (ev) => {
        let m: any;
        try {
          if (typeof ev.data === "string") m = JSON.parse(ev.data);
          else m = JSON.parse(new TextDecoder().decode(ev.data));
        } catch { return; }
        if (m.Welcome) { myId = m.Welcome.id; }
        else if (m.Snapshot && typeof m.Snapshot.tick === "number") { lastTick = m.Snapshot.tick; }
        else if (m.Snapshot) {
          for (const p of m.Snapshot.players) {
            if (p.id === myId) {
              hp = p.hp;
              const dx = p.pos[0] - pos.x, dz = p.pos[2] - pos.z;
              if (dx * dx + dz * dz > 0.36) { pos.x = p.pos[0]; pos.z = p.pos[2]; } // Reconciliation-Snap
              continue;
            }
            let r = remotes.get(p.id);
            if (!r) {
              const g = makeSoldier(p.team === 0 ? 0x22ff55 : 0xff4433);
              scene.add(g);
              r = { g, tx: p.pos[0], tz: p.pos[2], tyaw: p.yaw };
              remotes.set(p.id, r);
            }
            r.tx = p.pos[0]; r.tz = p.pos[2]; r.tyaw = p.yaw;
            r.g.visible = p.hp > 0;
          }
          for (const ev2 of m.Snapshot.events ?? []) {
            if (ev2.Kill) pushFeed(`⚡ KILL ${ev2.Kill.killer} → ${ev2.Kill.victim}`);
            if (ev2.Damage) {
              if (ev2.Damage.to === myId) blip(140, 60, 0.3);
              if (ev2.Damage.from === myId) blip(1200, 700, 0.12);
            }
          }
          const sc = m.Snapshot.scores ?? [0, 0];
          setHud((h) => ({ ...h, hp, conn: connOk, score: `GRÜN ${sc[0]} : ${sc[1]} ROT`, feed: [...feed] }));
        }
      };
    };
    connect();

    /* ---------- Touch ---------- */
    const el = renderer.domElement;
    el.style.touchAction = "none";
    const tStart = (e: TouchEvent) => {
      const r = el.getBoundingClientRect();
      for (const t of Array.from(e.changedTouches)) {
        if (t.clientX - r.left < r.width / 2 && joy.id === -1) { joy.id = t.identifier; joy.ox = t.clientX; joy.oy = t.clientY; }
        else if (look.id === -1) { look.id = t.identifier; look.lx = t.clientX; look.ly = t.clientY; }
      }
    };
    const tMove = (e: TouchEvent) => {
      e.preventDefault();
      for (const t of Array.from(e.changedTouches)) {
        if (t.identifier === joy.id) {
          const R = 60;
          const dx = t.clientX - joy.ox, dy = t.clientY - joy.oy;
          const l = Math.hypot(dx, dy) || 1;
          const c = Math.min(l, R);
          joy.x = (dx / l) * (c / R); joy.y = -(dy / l) * (c / R);
        } else if (t.identifier === look.id) {
          yaw -= (t.clientX - look.lx) * 0.005;
          pitch = Math.max(-1.4, Math.min(1.4, pitch - (t.clientY - look.ly) * 0.0045));
          look.lx = t.clientX; look.ly = t.clientY;
        }
      }
    };
    const tEnd = (e: TouchEvent) => {
      for (const t of Array.from(e.changedTouches)) {
        if (t.identifier === joy.id) { joy.id = -1; joy.x = 0; joy.y = 0; }
        if (t.identifier === look.id) look.id = -1;
      }
    };
    el.addEventListener("touchstart", tStart, { passive: true });
    el.addEventListener("touchmove", tMove, { passive: false });
    el.addEventListener("touchend", tEnd);
    el.addEventListener("touchcancel", tEnd);
    const keys: Record<string, boolean> = {};
    const kd = (e: KeyboardEvent) => { keys[e.code] = true; };
    const ku = (e: KeyboardEvent) => { keys[e.code] = false; };
    window.addEventListener("keydown", kd);
    window.addEventListener("keyup", ku);

    const fire = () => {
      if (fireCd > 0 || hp <= 0) return;
      fireCd = WEAPONS[weapon].rate;
      seq++;
      const dir = new THREE.Vector3(0, 0, -1).applyEuler(new THREE.Euler(pitch, yaw, 0, "YXZ"));
      shotSound(weapon);
      muzzle.position.copy(pos).add(dir.clone().multiplyScalar(1.2)).add(new THREE.Vector3(0, -0.1, 0));
      muzzle.intensity = 6; muzzleT = 0.05;
      const from = pos.clone().add(new THREE.Vector3(0, -0.1, 0));
      const geo = new THREE.BufferGeometry().setFromPoints([from, from.clone().add(dir.clone().multiplyScalar(90))]);
      const line = new THREE.Line(geo, new THREE.LineBasicMaterial({ color: 0xffdd88, transparent: true, opacity: 0.9 }));
      scene.add(line);
      tracers.push({ line, life: 0.08 });
    };
    (window as unknown as Record<string, unknown>).__nova = {
      fireOn: (v: boolean) => { fireRef.current = v; },
      weap: () => { weapon = (weapon + 1) % WEAPONS.length; blip(660, 990, 0.12); setHud((h) => ({ ...h, weapon })); },
    };

    /* ---------- Loop ---------- */
    let last = performance.now();
    let raf = 0;
    let inputAcc = 0;
    const clock = new THREE.Clock();
    const loop = () => {
      raf = requestAnimationFrame(loop);
      const now = performance.now();
      const dt = Math.min(0.05, (now - last) / 1000);
      last = now;
      const t = clock.getElapsedTime();
      fireCd -= dt;
      muzzleT -= dt;
      muzzle.intensity = muzzleT > 0 ? 6 : 0;

      let wx = joy.x, wy = joy.y;
      if (keys["KeyW"]) wy += 1; if (keys["KeyS"]) wy -= 1;
      if (keys["KeyD"]) wx += 1; if (keys["KeyA"]) wx -= 1;
      if (keys["ArrowLeft"]) yaw += 2.2 * dt; if (keys["ArrowRight"]) yaw -= 2.2 * dt;
      const mag = Math.min(1, Math.hypot(wx, wy));
      if (mag > 0.05) {
        const nl2 = Math.hypot(wx, wy) || 1;
        const nx = wx / nl2, ny = wy / nl2;
        const sp = 6 * mag;
        const sy = Math.sin(yaw), cy = Math.cos(yaw);
        pos.x += (nx * cy - ny * sy) * sp * dt;
        pos.z += (-nx * sy - ny * cy) * sp * dt;
      }
      pos.x = Math.max(-47, Math.min(47, pos.x));
      pos.z = Math.max(-47, Math.min(47, pos.z));
      pos.y = 1.7 + Math.sin(t * 9) * 0.03 * mag; // Head-Bob

      inputAcc += dt;
      if (inputAcc > 0.05) {
        inputAcc = 0;
        seq++;
        const btn = (fireRef.current ? 2 : 0) | (mag > 0.9 ? 1 : 0);
        ws?.send(JSON.stringify({ Input: { id: myId, seq, wish: [wx, wy], yaw, pitch, sprint: mag > 0.9, buttons: btn, last_server_tick: lastTick } }));
      }
      if (fireRef.current) fire();

      camera.position.copy(pos);
      camera.rotation.set(pitch, yaw, 0, "YXZ");

      for (const r of remotes.values()) {
        r.g.position.x += (r.tx - r.g.position.x) * Math.min(1, 12 * dt);
        r.g.position.z += (r.tz - r.g.position.z) * Math.min(1, 12 * dt);
        r.g.rotation.y = r.tyaw;
      }
      for (let i = tracers.length - 1; i >= 0; i--) {
        tracers[i].life -= dt;
        if (tracers[i].life <= 0) { scene.remove(tracers[i].line); tracers[i].line.geometry.dispose(); tracers.splice(i, 1); }
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
    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener("resize", onResize);
      window.removeEventListener("keydown", kd);
      window.removeEventListener("keyup", ku);
      el.removeEventListener("touchstart", tStart);
      el.removeEventListener("touchmove", tMove);
      el.removeEventListener("touchend", tEnd);
      el.removeEventListener("touchcancel", tEnd);
      ws?.close();
      renderer.dispose();
      if (renderer.domElement.parentElement === mount) mount.removeChild(renderer.domElement);
    };
  }, []);

  return (
    <div className="fixed inset-0 bg-black overflow-hidden select-none">
      <div ref={mountRef} className="w-full h-full" style={{ touchAction: "none" }} />
      <div className="absolute top-2 left-2 pointer-events-none font-mono text-[11px] text-[#22ff55] bg-black/60 border border-[#22ff5555] px-3 py-2 whitespace-pre-line">
        {hud.score}{"\n"}HP {hud.hp} | {WEAPONS[hud.weapon].name} {hud.conn ? "| LIVE" : "| …"}
      </div>
      <div className="absolute top-2 right-2 pointer-events-none font-mono text-[10px] text-[#baffc9] text-right whitespace-pre-line" style={{ textShadow: "0 0 6px rgba(34,255,85,.6)" }}>
        {hud.feed.join("\n")}
      </div>
      <button
        type="button"
        className="absolute right-4 bottom-16 w-24 h-24 rounded-full border-2 border-[#22ff55aa] bg-[#22ff5533] text-[#22ff55] font-mono text-sm active:bg-[#22ff5577]"
        style={{ touchAction: "none" }}
        onTouchStart={(e) => { e.preventDefault(); ((window as unknown as Record<string, any>).__nova?.fireOn)(true); }}
        onTouchEnd={() => ((window as unknown as Record<string, any>).__nova?.fireOn)(false)}
        onMouseDown={() => ((window as unknown as Record<string, any>).__nova?.fireOn)(true)}
        onMouseUp={() => ((window as unknown as Record<string, any>).__nova?.fireOn)(false)}
      >FEUER</button>
      <button
        type="button"
        className="absolute right-32 bottom-20 w-16 h-16 rounded-full border-2 border-[#22ff5577] bg-[#22ff5522] text-[#22ff55] font-mono text-[10px]"
        style={{ touchAction: "none" }}
        onTouchStart={(e) => { e.preventDefault(); ((window as unknown as Record<string, any>).__nova?.weap)(); }}
        onClick={() => ((window as unknown as Record<string, any>).__nova?.weap)()}
      >WAFFE</button>
      <p className="absolute bottom-2 left-2 font-mono text-[9px] text-[#22ff5599] pointer-events-none">
        LINKS STICK · RECHTS LOOK · FEUER = {WEAPONS[hud.weapon].name}
      </p>
    </div>
  );
}
