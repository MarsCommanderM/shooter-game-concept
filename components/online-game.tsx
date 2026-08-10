"use client";

import { useEffect, useRef, useState } from "react";
import * as THREE from "three";

/* ================================================================== */
/* WIRRWARR ONLINE – echtes Multiplayer über WebSocket (/ws)           */
/* FFA-Deathmatch, serverless-freundlich: Custom-Server server.mjs     */
/* ================================================================== */

interface Remote {
  id: number;
  name: string;
  color: number;
  group: THREE.Group;
  body: THREE.Mesh;
  label: THREE.Sprite;
  tx: number; tz: number; tyaw: number;
  hp: number;
  alive: boolean;
  kills: number;
  deaths: number;
  lastSeen: number;
}

const ARENA = 40;

function makeLabel(text: string, color: string): THREE.Sprite {
  const cv = document.createElement("canvas");
  cv.width = 128; cv.height = 32;
  const c = cv.getContext("2d")!;
  c.font = "bold 20px monospace";
  c.textAlign = "center";
  c.fillStyle = color;
  c.fillText(text.slice(0, 10), 64, 22);
  const tex = new THREE.CanvasTexture(cv);
  const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, transparent: true }));
  sp.scale.set(2.4, 0.6, 1);
  sp.position.y = 2.3;
  return sp;
}

export function OnlineGame() {
  const mountRef = useRef<HTMLDivElement>(null);
  const [status, setStatus] = useState<"connecting" | "online" | "error">("connecting");
  const [hud, setHud] = useState({ hp: 100, ammo: 24, reloading: false, feed: [] as string[], players: [] as { name: string; kills: number; deaths: number; me: boolean }[] });
  const apiRef = useRef<{ dispose: () => void } | null>(null);
  const [name] = useState(() => `KAEMPFER-${Math.floor(10 + Math.random() * 89)}`);

  useEffect(() => {
    const mount = mountRef.current;
    if (!mount) return;

    /* ---------- Scene ---------- */
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.setSize(mount.clientWidth, mount.clientHeight);
    mount.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x050705);
    scene.fog = new THREE.FogExp2(0x050705, 0.03);
    const camera = new THREE.PerspectiveCamera(75, mount.clientWidth / mount.clientHeight, 0.1, 200);
    const yaw = new THREE.Object3D();
    const pitch = new THREE.Object3D();
    yaw.add(pitch); pitch.add(camera);
    scene.add(yaw);
    scene.add(new THREE.HemisphereLight(0x22ff55, 0x000000, 0.5));
    const dl = new THREE.DirectionalLight(0x88ffaa, 0.7);
    dl.position.set(20, 40, 10);
    scene.add(dl);

    const ground = new THREE.Mesh(new THREE.PlaneGeometry(ARENA * 2, ARENA * 2), new THREE.MeshStandardMaterial({ color: 0x0a0f0a }));
    ground.rotation.x = -Math.PI / 2;
    scene.add(ground);
    scene.add(new THREE.GridHelper(ARENA * 2, 40, 0x1a4d24, 0x10240f));

    /* ---------- Walls ---------- */
    const walls: { x: number; z: number; hw: number; hd: number; mesh: THREE.Mesh }[] = [];
    const addWall = (x: number, z: number, w: number, d: number, h = 3) => {
      const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), new THREE.MeshStandardMaterial({ color: 0x1c2a1c, roughness: 0.9 }));
      mesh.position.set(x, h / 2, z);
      scene.add(mesh);
      walls.push({ x, z, hw: w / 2, hd: d / 2, mesh });
    };
    addWall(0, -ARENA, ARENA * 2, 1, 4); addWall(0, ARENA, ARENA * 2, 1, 4);
    addWall(-ARENA, 0, 1, ARENA * 2, 4); addWall(ARENA, 0, 1, ARENA * 2, 4);
    addWall(0, 0, 5, 5); addWall(-16, -12, 7, 1.2); addWall(16, 12, 7, 1.2);
    addWall(-16, 12, 1.2, 7); addWall(16, -12, 1.2, 7);
    addWall(-26, 0, 2, 8); addWall(26, 0, 2, 8); addWall(0, -26, 8, 2); addWall(0, 26, 8, 2);

    const collides = (x: number, z: number, r: number) =>
      walls.some((w) => x > w.x - w.hw - r && x < w.x + w.hw + r && z > w.z - w.hd - r && z < w.z + w.hd + r);
    const move = (p: { x: number; z: number }, dx: number, dz: number, r: number) => {
      if (!collides(p.x + dx, p.z, r)) p.x += dx;
      if (!collides(p.x, p.z + dz, r)) p.z += dz;
    };

    /* ---------- Lokaler Spieler ---------- */
    const me = { id: -1, x: 0, z: 30, y: 0, vy: 0, hp: 100, ammo: 24, reloading: 0, fireCd: 0, kills: 0, deaths: 0, respawnAt: 0 };
    yaw.position.set(me.x, 1.7, me.z);

    /* ---------- Remotes ---------- */
    const remotes = new Map<number, Remote>();
    const feed: string[] = [];
    const pushFeed = (t: string) => { feed.push(t); if (feed.length > 5) feed.shift(); };

    /* ---------- WS ---------- */
    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    let ws: WebSocket | null = null;
    let disposed = false;
    try {
      ws = new WebSocket(`${proto}//${location.host}/ws`);
    } catch {
      setStatus("error");
    }
    const send = (m: unknown) => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(m)); };

    if (ws) {
      ws.onopen = () => {
        setStatus("online");
        send({ t: "hello", name, color: 0x22ff55 });
      };
      ws.onerror = () => setStatus("error");
      ws.onclose = () => setStatus((s) => (s === "online" ? "error" : s));
      ws.onmessage = (ev) => {
        const m = JSON.parse(ev.data as string) as Record<string, unknown>;
        const id = m.id as number;
        if (m.t === "you") me.id = id;
        else if (m.t === "hello") {
          if (id === me.id) return;
          if (!remotes.has(id)) {
            const color = (m.color as number) || 0xff5544;
            const group = new THREE.Group();
            const body = new THREE.Mesh(new THREE.BoxGeometry(0.7, 1.5, 0.4), new THREE.MeshStandardMaterial({ color, emissive: new THREE.Color(color).multiplyScalar(0.3) }));
            body.position.y = 0.75;
            const head = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.4, 0.4), new THREE.MeshStandardMaterial({ color: 0x111111, emissive: new THREE.Color(color).multiplyScalar(0.5) }));
            head.position.y = 1.75;
            const label = makeLabel(String(m.name ?? "???"), `#${color.toString(16).padStart(6, "0")}`);
            group.add(body, head, label);
            group.position.set(0, 0, -30);
            scene.add(group);
            remotes.set(id, { id, name: String(m.name), color, group, body, label, tx: 0, tz: -30, tyaw: 0, hp: 100, alive: true, kills: 0, deaths: 0, lastSeen: performance.now() });
            pushFeed(`${m.name} beitritt`);
          }
        } else if (m.t === "state") {
          const r = remotes.get(id);
          if (r) { r.tx = m.x as number; r.tz = m.z as number; r.tyaw = m.yaw as number; r.hp = m.hp as number; r.alive = (m.hp as number) > 0; r.lastSeen = performance.now(); }
        } else if (m.t === "hit") {
          // jemand behauptet, mich getroffen zu haben
          if ((m.target as number) === me.id && me.hp > 0) {
            me.hp -= m.dmg as number;
            if (me.hp <= 0) {
              me.deaths++;
              me.respawnAt = performance.now() / 1000 + 3;
              const killer = remotes.get(id);
              if (killer) killer.kills++;
              pushFeed(`${killer?.name ?? "?"} ⚡ DU`);
              send({ t: "death", killer: id });
            }
          } else {
            const target = remotes.get(m.target as number);
            const killer = remotes.get(id);
            if (target) {
              target.hp -= m.dmg as number;
              if (target.hp <= 0 && target.alive) {
                target.alive = false;
                target.deaths++;
                if (killer) killer.kills++;
                pushFeed(`${killer?.name ?? "?"} ⚡ ${target.name}`);
              }
            }
          }
        } else if (m.t === "leave" || m.t === "join") {
          if (m.t === "leave") {
            const r = remotes.get(id);
            if (r) { scene.remove(r.group); remotes.delete(id); pushFeed(`${r.name} verlassen`); }
          }
        }
      };
    }

    /* ---------- Input ---------- */
    const keys: Record<string, boolean> = {};
    let mouseDown = false;
    let lastMX = 0;
    const el = renderer.domElement;
    const kd = (e: KeyboardEvent) => { keys[e.code] = true; };
    const ku = (e: KeyboardEvent) => { keys[e.code] = false; };
    const md = () => { mouseDown = true; try { el.requestPointerLock(); } catch { /* */ } };
    const mu = () => { mouseDown = false; };
    const mm = (e: MouseEvent) => {
      if (document.pointerLockElement === el) {
        yaw.rotation.y -= e.movementX * 0.0022;
        pitch.rotation.x = Math.max(-1.4, Math.min(1.4, pitch.rotation.x - e.movementY * 0.0022));
      } else if (mouseDown) yaw.rotation.y -= (e.clientX - lastMX) * 0.004;
      lastMX = e.clientX;
    };
    window.addEventListener("keydown", kd);
    window.addEventListener("keyup", ku);
    el.addEventListener("mousedown", md);
    window.addEventListener("mouseup", mu);
    window.addEventListener("mousemove", mm);

    /* ---------- Viewmodel + Tracer ---------- */
    const gun = new THREE.Group();
    gun.add(new THREE.Mesh(new THREE.BoxGeometry(0.12, 0.14, 0.7), new THREE.MeshStandardMaterial({ color: 0x1a1f1a, emissive: 0x0a2a10 })));
    const tip = new THREE.Mesh(new THREE.BoxGeometry(0.05, 0.05, 0.2), new THREE.MeshStandardMaterial({ color: 0x22ff55, emissive: 0x22ff55 }));
    tip.position.set(0, 0.02, -0.45);
    gun.add(tip);
    gun.position.set(0.3, -0.28, -0.6);
    camera.add(gun);

    const tracers: { line: THREE.Line; life: number }[] = [];
    const tracer = (a: THREE.Vector3, b: THREE.Vector3, color: number) => {
      const line = new THREE.Line(new THREE.BufferGeometry().setFromPoints([a, b]), new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.9 }));
      scene.add(line);
      tracers.push({ line, life: 0.08 });
    };

    /* ---------- Schießen ---------- */
    const raycaster = new THREE.Raycaster();
    let actx: AudioContext | null = null;
    const sShot = () => {
      const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
      if (!AC) return;
      if (!actx) actx = new AC();
      if (actx.state === "suspended") void actx.resume();
      const t = actx.currentTime;
      const o = actx.createOscillator(); const g = actx.createGain();
      o.type = "triangle"; o.frequency.setValueAtTime(700, t); o.frequency.exponentialRampToValueAtTime(170, t + 0.07);
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.12, t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.09);
      o.connect(g).connect(actx.destination); o.start(t); o.stop(t + 0.1);
    };
    const shoot = () => {
      if (me.fireCd > 0 || me.reloading > 0 || me.ammo <= 0 || me.hp <= 0) return;
      me.fireCd = 0.18;
      me.ammo--;
      sShot();
      raycaster.setFromCamera(new THREE.Vector2(0, 0), camera);
      const targets: THREE.Object3D[] = [];
      for (const r of remotes.values()) if (r.alive) targets.push(r.body);
      const hits = raycaster.intersectObjects([...targets, ...walls.map((w) => w.mesh)], false);
      const muzzle = new THREE.Vector3(); tip.getWorldPosition(muzzle);
      let end = raycaster.ray.at(60, new THREE.Vector3());
      if (hits.length > 0) {
        end = hits[0].point;
        const rHit = [...remotes.values()].find((r) => r.alive && hits[0].object === r.body);
        if (rHit) {
          send({ t: "hit", target: rHit.id, dmg: 26 });
          // lokal sofort Feedback
          rHit.hp -= 26;
          if (rHit.hp <= 0 && rHit.alive) {
            rHit.alive = false; rHit.deaths++; me.kills++;
            pushFeed(`DU ⚡ ${rHit.name}`);
          }
        }
      }
      tracer(muzzle, end, 0x22ff55);
      if (me.ammo === 0) me.reloading = 1.2;
    };

    /* ---------- Loop ---------- */
    const clock = new THREE.Clock();
    let raf = 0;
    let stateAcc = 0;
    let hudAcc = 0;
    const loop = () => {
      raf = requestAnimationFrame(loop);
      const dt = Math.min(0.05, clock.getDelta());
      me.fireCd -= dt;

      if (me.hp <= 0) {
        if (performance.now() / 1000 >= me.respawnAt) {
          me.hp = 100; me.x = (Math.random() - 0.5) * 60; me.z = 30; me.ammo = 24;
        }
      } else {
        const sp = 7 * dt * (keys["ShiftLeft"] ? 1.4 : 1);
        let fx = 0, fz = 0;
        const a = yaw.rotation.y;
        if (keys["KeyW"]) { fx -= Math.sin(a); fz -= Math.cos(a); }
        if (keys["KeyS"]) { fx += Math.sin(a); fz += Math.cos(a); }
        if (keys["KeyA"]) { fx -= Math.cos(a); fz += Math.sin(a); }
        if (keys["KeyD"]) { fx += Math.cos(a); fz -= Math.sin(a); }
        const l = Math.hypot(fx, fz);
        if (l > 0.01) move(me, (fx / l) * sp, (fz / l) * sp, 0.5);
        if (keys["Space"] && me.y === 0) me.vy = 7.5;
        me.vy -= 20 * dt;
        me.y = Math.max(0, me.y + me.vy * dt);
        if (me.y === 0) me.vy = 0;
        if (me.reloading > 0) { me.reloading -= dt; if (me.reloading <= 0) me.ammo = 24; }
        if (keys["KeyR"] && me.ammo < 24 && me.reloading <= 0) me.reloading = 1.2;
        if (mouseDown) shoot();
      }
      yaw.position.set(me.x, 1.7 + me.y, me.z);

      // Remotes interpolieren
      for (const r of remotes.values()) {
        r.group.position.x += (r.tx - r.group.position.x) * Math.min(1, dt * 12);
        r.group.position.z += (r.tz - r.group.position.z) * Math.min(1, dt * 12);
        r.group.rotation.y += (r.tyaw - r.group.rotation.y) * Math.min(1, dt * 12);
        r.group.visible = r.alive;
      }

      // Tracer
      for (let i = tracers.length - 1; i >= 0; i--) {
        const t = tracers[i];
        t.life -= dt;
        (t.line.material as THREE.LineBasicMaterial).opacity = Math.max(0, t.life / 0.08);
        if (t.life <= 0) { scene.remove(t.line); t.line.geometry.dispose(); tracers.splice(i, 1); }
      }

      // State senden 12 Hz
      stateAcc += dt;
      if (stateAcc > 0.08) {
        stateAcc = 0;
        send({ t: "state", x: me.x, z: me.z, yaw: yaw.rotation.y, hp: me.hp });
      }

      hudAcc += dt;
      if (hudAcc > 0.25) {
        hudAcc = 0;
        const players = [
          { name: `${name} (DU)`, kills: me.kills, deaths: me.deaths, me: true },
          ...[...remotes.values()].map((r) => ({ name: r.name, kills: r.kills, deaths: r.deaths, me: false })),
        ].sort((x, y) => y.kills - x.kills);
        setHud({ hp: Math.max(0, Math.round(me.hp)), ammo: me.ammo, reloading: me.reloading > 0, feed: [...feed], players });
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
        disposed = true;
        cancelAnimationFrame(raf);
        ws?.close();
        window.removeEventListener("keydown", kd);
        window.removeEventListener("keyup", ku);
        window.removeEventListener("mouseup", mu);
        window.removeEventListener("mousemove", mm);
        window.removeEventListener("resize", onResize);
        el.removeEventListener("mousedown", md);
        renderer.dispose();
        if (el.parentElement === mount) mount.removeChild(el);
      },
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div className="relative h-screen w-full bg-black overflow-hidden select-none">
      <div ref={mountRef} className="w-full h-full" />
      {status === "connecting" && (
        <div className="absolute inset-0 flex items-center justify-center bg-black/70">
          <p className="font-mono text-sm text-primary tracking-[0.3em] uppercase animate-pulse-neon">Verbinde mit Server …</p>
        </div>
      )}
      {status === "error" && (
        <div className="absolute inset-0 flex items-center justify-center bg-black/80">
          <div className="text-center max-w-md px-6">
            <p className="font-mono text-sm text-destructive tracking-wider uppercase mb-3">Server nicht erreichbar</p>
            <p className="text-sm text-muted-foreground leading-relaxed">
              Online läuft über den Custom-Server: <span className="text-foreground font-mono">npm run online</span> (node server.mjs) statt <span className="font-mono">next start</span>. In der Arena-Vorschau hier sollte es funktionieren.
            </p>
          </div>
        </div>
      )}
      <div className="absolute top-3 left-1/2 -translate-x-1/2 text-center pointer-events-none">
        <p className="font-mono text-sm text-primary glow-neon-sm tracking-wider">ONLINE // FFA-Deathmatch</p>
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider mt-1">{name} · Tab = Scoreboard</p>
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
        <p className="font-mono text-2xl text-foreground leading-none">
          {hud.reloading ? <span className="text-primary">LÄDT…</span> : <>{hud.ammo}<span className="text-muted-foreground text-sm">/∞</span></>}
        </p>
      </div>
      <Scoreboard hud={hud} />
    </div>
  );
}

function Scoreboard({ hud }: { hud: { players: { name: string; kills: number; deaths: number; me: boolean }[] } }) {
  const [tab, setTab] = useState(false);
  useEffect(() => {
    const d = (e: KeyboardEvent) => { if (e.key === "Tab") { e.preventDefault(); setTab(true); } };
    const u = (e: KeyboardEvent) => { if (e.key === "Tab") setTab(false); };
    window.addEventListener("keydown", d);
    window.addEventListener("keyup", u);
    return () => { window.removeEventListener("keydown", d); window.removeEventListener("keyup", u); };
  }, []);
  if (!tab) return null;
  return (
    <div className="absolute inset-x-0 top-1/2 -translate-y-1/2 flex justify-center pointer-events-none">
      <div className="bg-black/85 border border-primary/40 rounded-sm p-4 min-w-[300px] box-glow-neon">
        <p className="font-mono text-[10px] tracking-[0.25em] uppercase text-primary mb-2">Live-Scoreboard</p>
        {hud.players.map((p) => (
          <div key={p.name} className={`flex justify-between gap-6 font-mono text-[11px] py-0.5 ${p.me ? "text-primary" : "text-foreground"}`}>
            <span>{p.name}</span>
            <span className="text-muted-foreground">{p.kills} / {p.deaths}</span>
          </div>
        ))}
      </div>
    </div>
  );
}
