"use client";

import { useEffect, useRef, useState } from "react";
import * as THREE from "three";
import { updateDaily } from "./real-game";

/* ================================================================== */
/* WIRRWARR ONLINE v2 – FFA & TDM, Movement-Polish, Stance-Sync        */
/* ================================================================== */

type OnlineMode = "ffa" | "tdm";

interface Remote {
  id: number;
  name: string;
  color: number;
  team: number;
  group: THREE.Group;
  body: THREE.Mesh;
  label: THREE.Sprite;
  tx: number; tz: number; tyaw: number;
  hp: number;
  stance: number;
  alive: boolean;
  kills: number;
  deaths: number;
  buf: { x: number; z: number; yaw: number; t: number }[];
}

const ARENA = 40;
const TEAM_HEX = [0x22ff55, 0xff5544];

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

let actx: AudioContext | null = null;
function ac(): AudioContext | null {
  if (typeof window === "undefined") return null;
  const AC = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
  if (!AC) return null;
  if (!actx) actx = new AC();
  if (actx.state === "suspended") void actx.resume();
  return actx;
}
function sShot() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "triangle"; o.frequency.setValueAtTime(700, t); o.frequency.exponentialRampToValueAtTime(170, t + 0.07);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.12, t + 0.005); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.09);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.1);
}
function sAnnounceOnline() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  [523, 784, 1046].forEach((f, i) => {
    const o = c.createOscillator(); const g = c.createGain();
    o.type = "square"; o.frequency.setValueAtTime(f, t + i * 0.07);
    g.gain.setValueAtTime(0.0001, t + i * 0.07); g.gain.exponentialRampToValueAtTime(0.06, t + i * 0.07 + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + i * 0.07 + 0.18);
    o.connect(g).connect(c.destination); o.start(t + i * 0.07); o.stop(t + i * 0.07 + 0.2);
  });
}
function sStep() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  const o = c.createOscillator(); const g = c.createGain();
  o.type = "triangle"; o.frequency.setValueAtTime(95, t); o.frequency.exponentialRampToValueAtTime(55, t + 0.05);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.045, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.06);
  o.connect(g).connect(c.destination); o.start(t); o.stop(t + 0.07);
}

export function OnlineGame() {
  const mountRef = useRef<HTMLDivElement>(null);
  const [screen, setScreen] = useState<"menu" | "game">("menu");
  const [mode, setMode] = useState<OnlineMode>("ffa");
  const [status, setStatus] = useState<"connecting" | "online" | "error">("connecting");
  const [result, setResult] = useState<string | null>(null);
  const [hud, setHud] = useState({
    hp: 100, ammo: 24, reloading: false, feed: [] as string[],
    players: [] as { name: string; kills: number; deaths: number; me: boolean }[],
    scores: "", announce: null as string | null, ping: 0,
  });
  const apiRef = useRef<{ dispose: () => void } | null>(null);
  const sendRef = useRef<(m: unknown) => void>(() => {});
  const [chatOpen, setChatOpen] = useState<null | "all" | "team">(null);
  const [chatText, setChatText] = useState("");
  const [chatLog, setChatLog] = useState<{ name: string; text: string; teamChat: boolean; own: boolean }[]>([]);
  const [top10, setTop10] = useState<{ name: string; kills: number }[]>([]);
  const [micOn, setMicOn] = useState(false);
  const [micErr, setMicErr] = useState("");
  const vcRef = useRef<{ toggle: () => void; stop: () => void } | null>(null);
  const [name] = useState(() => `KAEMPFER-${Math.floor(10 + Math.random() * 89)}`);

  useEffect(() => {
    if (screen !== "game") return;
    let stream: MediaStream | null = null;
    let myId = -1;
    const pcs = new Map<number, RTCPeerConnection>();
    const audios = new Map<number, HTMLAudioElement>();
    const CFG: RTCConfiguration = { iceServers: [{ urls: "stun:stun.l.google.com:19302" }] };
    const sendVc = (m: Record<string, unknown>) => sendRef.current(m);
    const attach = (pc: RTCPeerConnection, id: number) => {
      pc.onicecandidate = (e) => { if (e.candidate) sendVc({ t: "vc-ice", to: id, cand: e.candidate }); };
      pc.ontrack = (e) => {
        let a = audios.get(id);
        if (!a) { a = new Audio(); audios.set(id, a); }
        a.srcObject = e.streams[0];
        a.autoplay = true;
        void a.play().catch(() => {});
      };
      pc.onconnectionstatechange = () => {
        if (["failed", "closed", "disconnected"].includes(pc.connectionState)) {
          pc.close(); pcs.delete(id);
          const a = audios.get(id); if (a) { a.srcObject = null; audios.delete(id); }
        }
      };
    };
    const addTracks = (pc: RTCPeerConnection) => { const s = stream; if (s) s.getTracks().forEach((t) => pc.addTrack(t, s)); };
    const makeOffer = async (id: number) => {
      const pc = new RTCPeerConnection(CFG);
      pcs.set(id, pc); attach(pc, id); addTracks(pc);
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);
      sendVc({ t: "vc-offer", to: id, sdp: offer });
    };
    const onMsg = (ev: Event) => {
      const m = (ev as MessageEvent).data as Record<string, any>;
      if (!m || typeof m !== "object") return;
      (async () => {
        if (m.t === "vc-hello") {
          myId = m.id;
          for (const pid of (m.others as number[]) ?? []) if (pid < myId) await makeOffer(pid);
        }
        if (m.t === "vc-offer" && stream) {
          let pc = pcs.get(m.from);
          if (!pc) { pc = new RTCPeerConnection(CFG); pcs.set(m.from, pc); attach(pc, m.from); addTracks(pc); }
          await pc.setRemoteDescription(m.sdp);
          const ans = await pc.createAnswer();
          await pc.setLocalDescription(ans);
          sendVc({ t: "vc-answer", to: m.from, sdp: ans });
        }
        if (m.t === "vc-answer") { const pc = pcs.get(m.from); if (pc) await pc.setRemoteDescription(m.sdp); }
        if (m.t === "vc-ice") { const pc = pcs.get(m.from); if (pc && m.cand) await pc.addIceCandidate(m.cand).catch(() => {}); }
        if (m.t === "vc-bye") {
          const pc = pcs.get(m.from); if (pc) pc.close();
          pcs.delete(m.from);
          const a = audios.get(m.from); if (a) { a.srcObject = null; audios.delete(m.from); }
        }
      })().catch(() => {});
    };
    window.addEventListener("wirrwarr-vc", onMsg);
    navigator.mediaDevices?.getUserMedia({ audio: true })
      .then((s) => { stream = s; setMicOn(true); sendVc({ t: "vc-join-req" }); })
      .catch(() => setMicErr("Mikrofon nicht verfügbar / verweigert."));
    vcRef.current = {
      toggle: () => {
        if (!stream) return;
        const enabled = !stream.getAudioTracks()[0].enabled;
        stream.getAudioTracks().forEach((t) => { t.enabled = enabled; });
        setMicOn(enabled);
      },
      stop: () => {
        sendVc({ t: "vc-bye-broadcast" });
        pcs.forEach((pc) => pc.close());
        stream?.getTracks().forEach((t) => t.stop());
      },
    };
    return () => {
      vcRef.current?.stop();
      window.removeEventListener("wirrwarr-vc", onMsg);
    };
  }, [screen]);
  useEffect(() => () => apiRef.current?.dispose(), []);
  useEffect(() => {
    if (screen !== "game") return;
    const h = (e: KeyboardEvent) => {
      const tag = (e.target as HTMLElement)?.tagName;
      if (tag === "INPUT") return;
      if (e.key === "Enter") { e.preventDefault(); setChatOpen("all"); }
      if (e.key === "u" || e.key === "U") { e.preventDefault(); setChatOpen("team"); }
      if (e.key === "Escape") setChatOpen(null);
    };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, [screen]);

  const start = (m: OnlineMode) => {
    setMode(m);
    setResult(null);
    setScreen("game");
    requestAnimationFrame(() => initEngine(m));
  };

  const initEngine = (gameMode: OnlineMode) => {
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

    /* ---------- Walls + niedrige Parkour-Cover ---------- */
    const walls: { x: number; z: number; hw: number; hd: number; top: number; mesh: THREE.Mesh }[] = [];
    const addWall = (x: number, z: number, w: number, d: number, h = 3) => {
      const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), new THREE.MeshStandardMaterial({ color: h < 3 ? 0x2f7a3a : 0x1c2a1c, roughness: 0.9 }));
      mesh.position.set(x, h / 2, z);
      scene.add(mesh);
      walls.push({ x, z, hw: w / 2, hd: d / 2, top: h, mesh });
    };
    addWall(0, -ARENA, ARENA * 2, 1, 4); addWall(0, ARENA, ARENA * 2, 1, 4);
    addWall(-ARENA, 0, 1, ARENA * 2, 4); addWall(ARENA, 0, 1, ARENA * 2, 4);
    addWall(0, 0, 5, 5);
    addWall(-16, -12, 7, 1.2); addWall(16, 12, 7, 1.2);
    addWall(-16, 12, 1.2, 7); addWall(16, -12, 1.2, 7);
    addWall(-26, 0, 2, 8); addWall(26, 0, 2, 8); addWall(0, -26, 8, 2); addWall(0, 26, 8, 2);
    // Parkour-Cover (klettern/springen/manteln)
    addWall(-8, 8, 2.5, 2.5, 1.1); addWall(8, -8, 2.5, 2.5, 1.1);
    addWall(-8, -14, 3, 1.2, 1.6); addWall(8, 14, 3, 1.2, 1.6);
    addWall(14, 4, 1.2, 3, 2.2); addWall(-14, -4, 1.2, 3, 2.2);

    const collides = (x: number, z: number, r: number, y: number) =>
      walls.some((w) => w.top > y + 0.5 && x > w.x - w.hw - r && x < w.x + w.hw + r && z > w.z - w.hd - r && z < w.z + w.hd + r);
    const move = (p: { x: number; z: number }, dx: number, dz: number, r: number, y: number) => {
      if (!collides(p.x + dx, p.z, r, y)) p.x += dx;
      if (!collides(p.x, p.z + dz, r, y)) p.z += dz;
    };
    const getSupport = (x: number, z: number, fromY: number) => {
      let g = 0;
      for (const w of walls) {
        if (x > w.x - w.hw - 0.4 && x < w.x + w.hw + 0.4 && z > w.z - w.hd - 0.4 && z < w.z + w.hd + 0.4) {
          if (w.top <= fromY + 0.001 && w.top > g) g = w.top;
        }
      }
      return g;
    };

    /* ---------- Lokaler Spieler (Movement v2) ---------- */
    const me = {
      id: -1, team: -1,
      x: 0, z: 30, y: 0, vy: 0, vx: 0, vz: 0,
      hp: 100, ammo: 24, reloading: 0, fireCd: 0,
      kills: 0, deaths: 0, respawnAt: 0,
      streak: 0, multiKills: [] as number[], announce: null as { text: string; t: number } | null,
      crouch: false, prone: false, proneHeld: false, prevCrouch: false,
      slideT: 0, coyote: 0, jbuf: 0, jumpHeld: false,
      camH: 1.7, bobPhase: 0, landDip: 0, stepAcc: 0, mantleTarget: null as number | null,
    };

    /* ---------- Remotes ---------- */
    const remotes = new Map<number, Remote>();
    const feed: string[] = [];
    const pushFeed = (t: string) => { feed.push(t); if (feed.length > 5) feed.shift(); };
    const teamScore = [0, 0];
    let ended = false;

    /* ---------- WS ---------- */
    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    let ws: WebSocket | null = null;
    try { ws = new WebSocket(`${proto}//${location.host}/ws?mode=${gameMode}`); } catch { setStatus("error"); }
    const send = (m: unknown) => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(m)); };
    sendRef.current = send;
    send({ t: "top", mode: gameMode });

    const announce = (text: string) => {
      me.announce = { text, t: performance.now() / 1000 };
      sAnnounceOnline();
    };
    const creditKill = (killerTeam: number, victimTeam: number, killerName: string, victimName: string) => {
      if (gameMode === "tdm" && killerTeam >= 0 && killerTeam < 2) teamScore[killerTeam]++;
      pushFeed(`${killerName} ⚡ ${victimName}`);
      const nowS = performance.now() / 1000;
      if (killerName === "DU") {
        updateDaily("kills");
        me.streak++;
        if (me.streak === 3) announce("RAMPAGE!");
        if (me.streak === 5) announce("UNSTOPPBAR!");
        if (me.streak === 8) announce("GOTTGLEICH!");
        me.multiKills = me.multiKills.filter((t) => nowS - t < 2.5);
        me.multiKills.push(nowS);
        if (me.multiKills.length === 2) announce("DOUBLE KILL!");
        if (me.multiKills.length === 3) announce("TRIPLE KILL!");
      }
      if (victimName === "DU") me.streak = 0;
      if (gameMode === "tdm" && !ended && teamScore[killerTeam] >= 10) {
        ended = true;
        setResult(me.team === killerTeam ? "SIEG! Team gewinnt 10" : `NIEDERLAGE – Team ${killerTeam === 0 ? "GRÜN" : "ROT"} gewinnt`);
      }
    };

    if (ws) {
      ws.onopen = () => { setStatus("online"); send({ t: "hello", name, color: 0x22ff55 }); };
      ws.onerror = () => setStatus("error");
      ws.onmessage = (ev) => {
        const m = JSON.parse(ev.data as string) as Record<string, any>;
        const id = m.id as number;
        if (m.t === "you") { me.id = id; me.team = m.team as number; }
        else if (m.t === "hello") {
          if (id === me.id) return;
          if (!remotes.has(id)) {
            const team = m.team as number;
            const color = gameMode === "tdm" ? TEAM_HEX[team] : (m.color as number) || 0xff5544;
            const group = new THREE.Group();
            const body = new THREE.Mesh(new THREE.BoxGeometry(0.7, 1.5, 0.4), new THREE.MeshStandardMaterial({ color, emissive: new THREE.Color(color).multiplyScalar(0.3) }));
            body.position.y = 0.75;
            const head = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.4, 0.4), new THREE.MeshStandardMaterial({ color: 0x111111, emissive: new THREE.Color(color).multiplyScalar(0.5) }));
            head.position.y = 1.75;
            const label = makeLabel(String(m.name ?? "???"), `#${color.toString(16).padStart(6, "0")}`);
            group.add(body, head, label);
            group.position.set(0, 0, -30);
            scene.add(group);
            remotes.set(id, { id, name: String(m.name), color, team, group, body, label, tx: 0, tz: -30, tyaw: 0, hp: 100, stance: 1, alive: true, kills: 0, deaths: 0, buf: [] });
            pushFeed(`${m.name} beitritt`);
          }
        } else if (m.t === "state") {
          const r = remotes.get(id);
          if (r) {
            r.tx = m.x; r.tz = m.z; r.tyaw = m.yaw; r.hp = m.hp; r.stance = m.stance ?? 1;
            r.alive = m.hp > 0;
            r.buf.push({ x: m.x, z: m.z, yaw: m.yaw, t: performance.now() });
            if (r.buf.length > 40) r.buf.shift();
          }
        } else if (m.t === "pong") {
          pingVal = Math.round(performance.now() - (m.ts as number));
        } else if (m.t === "hit") {
          if (m.target === me.id && me.hp > 0) {
            me.hp -= m.dmg;
            const killer = remotes.get(id);
            if (me.hp <= 0) {
              me.deaths++;
              me.respawnAt = performance.now() / 1000 + 3;
              if (killer) killer.kills++;
              creditKill(killer?.team ?? -1, me.team, killer?.name ?? "?", "DU");
              send({ t: "death", killer: id });
            }
          } else {
            const target = remotes.get(m.target);
            const killer = remotes.get(id);
            if (target) {
              target.hp -= m.dmg;
              if (target.hp <= 0 && target.alive) {
                target.alive = false;
                target.deaths++;
                if (killer) killer.kills++;
                creditKill(killer?.team ?? -1, target.team, killer?.name ?? "?", target.name);
              }
            }
          }
        } else if (typeof m.t === "string" && m.t.startsWith("vc-")) {
          window.dispatchEvent(new MessageEvent("wirrwarr-vc", { data: m }));
          return;
        } else if (m.t === "top") {
          setTop10((m.top as { name: string; kills: number }[]) ?? []);
        } else if (m.t === "chat") {
          setChatLog((l) => [...l.slice(-7), { name: String(m.name), text: String(m.text), teamChat: !!m.teamChat, own: m.id === me.id }]);
        } else if (m.t === "warn") {
          pushFeed(`⚠ Anti-Cheat: Treffer verworfen (${m.reason})`);
        } else if (m.t === "leave") {
          const r = remotes.get(id);
          if (r) { scene.remove(r.group); remotes.delete(id); pushFeed(`${r.name} verlassen`); }
        }
      };
    }

    /* ---------- Input (gepolisht) ---------- */
    const keys: Record<string, boolean> = {};
    let mouseDown = false;
    let lastMX = 0;
    let targetYaw = 0;
    let targetPitch = 0;
    const el = renderer.domElement;
    const kd = (e: KeyboardEvent) => { keys[e.code] = true; };
    const ku = (e: KeyboardEvent) => { keys[e.code] = false; };
    const md = () => { mouseDown = true; try { el.requestPointerLock(); } catch { /* */ } };
    const mu = () => { mouseDown = false; };
    const mm = (e: MouseEvent) => {
      if (document.pointerLockElement === el) {
        targetYaw -= e.movementX * 0.0022;
        targetPitch = Math.max(-1.4, Math.min(1.4, targetPitch - e.movementY * 0.0022));
      } else if (mouseDown) targetYaw -= (e.clientX - lastMX) * 0.004;
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
    const shoot = () => {
      if (me.fireCd > 0 || me.reloading > 0 || me.ammo <= 0 || me.hp <= 0) return;
      me.fireCd = 0.18;
      me.ammo--;
      sShot();
      raycaster.setFromCamera(new THREE.Vector2(0, 0), camera);
      const targets: THREE.Object3D[] = [];
      for (const r of remotes.values()) {
        if (r.alive && (gameMode === "ffa" || r.team !== me.team)) targets.push(r.body);
      }
      const hits = raycaster.intersectObjects([...targets, ...walls.map((w) => w.mesh)], false);
      const muzzle = new THREE.Vector3(); tip.getWorldPosition(muzzle);
      let end = raycaster.ray.at(60, new THREE.Vector3());
      if (hits.length > 0) {
        end = hits[0].point;
        const rHit = [...remotes.values()].find((r) => r.alive && hits[0].object === r.body);
        if (rHit) {
          send({ t: "hit", target: rHit.id, dmg: 26 });
          rHit.hp -= 26;
          if (rHit.hp <= 0 && rHit.alive) {
            rHit.alive = false; rHit.deaths++; me.kills++;
            creditKill(me.team, rHit.team, "DU", rHit.name);
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
    let pingAcc = 0;
    let pingVal = 0;
    const loop = () => {
      raf = requestAnimationFrame(loop);
      const dt = Math.min(0.05, clock.getDelta());
      me.fireCd -= dt;

      if (me.hp <= 0) {
        if (performance.now() / 1000 >= me.respawnAt) {
          me.hp = 100; me.x = (Math.random() - 0.5) * 60; me.z = 30; me.ammo = 24; me.vx = 0; me.vz = 0;
        }
      } else {
        // Stance
        const support = getSupport(me.x, me.z, me.y);
        const onGround = me.y <= support + 0.02;
        if (onGround) me.coyote = 0.12; else me.coyote = Math.max(0, me.coyote - dt);
        const wantCrouch = !!(keys["KeyC"] || keys["ControlLeft"]);
        if (keys["KeyX"] && !me.proneHeld) me.prone = !me.prone;
        me.proneHeld = !!keys["KeyX"];
        me.crouch = me.prone || wantCrouch;
        const sp0 = Math.hypot(me.vx, me.vz);
        if (wantCrouch && !me.prevCrouch && sp0 > 5.5 && onGround) me.slideT = 0.75;
        me.prevCrouch = wantCrouch;
        me.slideT = Math.max(0, me.slideT - dt);
        const sliding = me.slideT > 0 && onGround;
        const sprinting = !!keys["ShiftLeft"] && !me.crouch && sp0 > 4;
        const maxSp = me.prone ? 1.5 : me.crouch ? 2.6 : sprinting ? 9 : 5.2;
        // Wish + Accel + Friction
        let wx = 0, wz = 0;
        const a = yaw.rotation.y;
        if (keys["KeyW"]) { wx -= Math.sin(a); wz -= Math.cos(a); }
        if (keys["KeyS"]) { wx += Math.sin(a); wz += Math.cos(a); }
        if (keys["KeyA"]) { wx -= Math.cos(a); wz += Math.sin(a); }
        if (keys["KeyD"]) { wx += Math.cos(a); wz -= Math.sin(a); }
        const wl = Math.hypot(wx, wz);
        if (wl > 0.01) { wx /= wl; wz /= wl; }
        me.vx += wx * 46 * dt; me.vz += wz * 46 * dt;
        const damp = onGround ? (wl > 0.01 ? 1 - 1.4 * dt : 1 - 12 * dt) : 1 - 0.15 * dt;
        me.vx *= damp; me.vz *= damp;
        const spNow = Math.hypot(me.vx, me.vz);
        const cap = sliding ? Math.max(maxSp, spNow * (1 - 1.1 * dt)) : maxSp;
        if (spNow > cap && spNow > 0) { me.vx *= cap / spNow; me.vz *= cap / spNow; }
        // Jump-Buffer + Coyote
        if (keys["Space"] && !me.jumpHeld) me.jbuf = 0.14; else me.jbuf = Math.max(0, me.jbuf - dt);
        me.jumpHeld = !!keys["Space"];
        if (me.prone && me.jbuf > 0) { me.prone = false; me.jbuf = 0; }
        else if (me.jbuf > 0 && me.coyote > 0 && !me.crouch) { me.vy = 7.6; me.coyote = 0; me.jbuf = 0; }
        // Gravitation + Landung
        me.vy -= 20 * dt;
        const newY = me.y + me.vy * dt;
        if (newY <= support && me.vy <= 0) {
          if (me.vy < -7) me.landDip = 0.16;
          me.y = support; me.vy = 0;
        } else me.y = Math.max(0, newY);
        // Horizontal + Step-Up + Mantling
        const dxm = me.vx * dt, dzm = me.vz * dt;
        const bx = me.x, bz = me.z;
        if (me.mantleTarget == null) move(me, dxm, dzm, 0.5, me.y);
        const blockedX = Math.abs(me.x - (bx + dxm)) > 0.001;
        const blockedZ = Math.abs(me.z - (bz + dzm)) > 0.001;
        if ((blockedX || blockedZ) && me.mantleTarget == null) {
          const probeTop = getSupport(bx + (blockedX ? Math.sign(dxm) * 0.7 : 0), bz + (blockedZ ? Math.sign(dzm) * 0.7 : 0), me.y + 2);
          const dh = probeTop - me.y;
          if (dh > 0.02 && dh <= 0.55 && onGround) me.y = probeTop;
          else if (dh > 0.55 && dh <= 1.5 && !me.crouch && onGround && wl > 0.01) { me.mantleTarget = probeTop + 0.02; me.vy = 0; }
          else { if (blockedX) me.vx = 0; if (blockedZ) me.vz = 0; }
        }
        if (me.mantleTarget != null) {
          me.y += (me.mantleTarget - me.y) * Math.min(1, 10 * dt);
          move(me, dxm * 1.5, dzm * 1.5, 0.5, me.y);
          if (Math.abs(me.mantleTarget - me.y) < 0.04) { me.y = me.mantleTarget; me.mantleTarget = null; }
        }
        // Footsteps
        const spd = Math.hypot(me.vx, me.vz);
        if (onGround && spd > 1.5 && !sliding) {
          me.stepAcc += spd * dt;
          if (me.stepAcc > 2.4) { me.stepAcc = 0; sStep(); }
        }
        if (me.reloading > 0) { me.reloading -= dt; if (me.reloading <= 0) me.ammo = 24; }
        if (keys["KeyR"] && me.ammo < 24 && me.reloading <= 0) me.reloading = 1.2;
        if (mouseDown) shoot();
        const gspd = Math.hypot(me.vx, me.vz);
        gun.position.y = -0.28 + Math.sin(me.bobPhase) * Math.min(0.014, gspd * 0.0018);
        gun.position.x = 0.3 + Math.cos(me.bobPhase * 0.5) * Math.min(0.01, gspd * 0.0012);
      }

      // Kamera-Flow
      yaw.rotation.y += (targetYaw - yaw.rotation.y) * Math.min(1, 34 * dt);
      pitch.rotation.x += (targetPitch - pitch.rotation.x) * Math.min(1, 34 * dt);
      const heightT = me.prone ? 0.55 : me.crouch ? 1.15 : 1.7;
      me.camH += (heightT - me.camH) * Math.min(1, 12 * dt);
      const camSpd = Math.hypot(me.vx, me.vz);
      const camGrounded = me.y <= getSupport(me.x, me.z, me.y) + 0.05;
      if (camGrounded && camSpd > 1) me.bobPhase += camSpd * dt * 1.7;
      const bob = camGrounded ? Math.sin(me.bobPhase) * Math.min(0.045, camSpd * 0.005) : 0;
      me.landDip = Math.max(0, me.landDip - dt * 0.5);
      yaw.position.set(me.x, me.camH + me.y + bob - me.landDip * 0.4, me.z);
      const fovT = me.slideT > 0 ? 84 : keys["ShiftLeft"] && camSpd > 6 && !me.crouch ? 81 : me.prone ? 70 : 75;
      if (Math.abs(camera.fov - fovT) > 0.1) {
        camera.fov += (fovT - camera.fov) * Math.min(1, 9 * dt);
        camera.updateProjectionMatrix();
      }

      // Remotes: echter Interpolation-Buffer (~100 ms in der Vergangenheit)
      const rt = performance.now() - 100;
      for (const r of remotes.values()) {
        while (r.buf.length > 2 && r.buf[1].t < rt) r.buf.shift();
        if (r.buf.length >= 2) {
          const a = r.buf[0], b = r.buf[1];
          const f = Math.max(0, Math.min(1, (rt - a.t) / ((b.t - a.t) || 1)));
          r.group.position.x = a.x + (b.x - a.x) * f;
          r.group.position.z = a.z + (b.z - a.z) * f;
          let ay = a.yaw, by = b.yaw;
          if (by - ay > Math.PI) ay += Math.PI * 2;
          if (ay - by > Math.PI) by += Math.PI * 2;
          r.group.rotation.y = ay + (by - ay) * f;
        } else {
          r.group.position.x += (r.tx - r.group.position.x) * Math.min(1, dt * 12);
          r.group.position.z += (r.tz - r.group.position.z) * Math.min(1, dt * 12);
          r.group.rotation.y += (r.tyaw - r.group.rotation.y) * Math.min(1, dt * 12);
        }
        r.group.visible = r.alive;
        const sc = r.stance === 3 ? 0.45 : r.stance === 2 ? 0.72 : 1;
        r.group.scale.y += (sc - r.group.scale.y) * Math.min(1, dt * 10);
      }

      // Tracer
      for (let i = tracers.length - 1; i >= 0; i--) {
        const t = tracers[i];
        t.life -= dt;
        (t.line.material as THREE.LineBasicMaterial).opacity = Math.max(0, t.life / 0.08);
        if (t.life <= 0) { scene.remove(t.line); t.line.geometry.dispose(); tracers.splice(i, 1); }
      }

      // State 12 Hz (inkl. Stance) + Ping 0.5 Hz
      stateAcc += dt;
      if (stateAcc > 0.08) {
        stateAcc = 0;
        send({ t: "state", x: me.x, z: me.z, yaw: yaw.rotation.y, hp: me.hp, stance: me.prone ? 3 : me.crouch ? 2 : 1 });
      }
      pingAcc += dt;
      if (pingAcc > 2) { pingAcc = 0; send({ t: "ping", ts: performance.now() }); }

      hudAcc += dt;
      if (hudAcc > 0.25) {
        hudAcc = 0;
        const players = [
          { name: `${name} (DU)`, kills: me.kills, deaths: me.deaths, me: true },
          ...[...remotes.values()].map((r) => ({ name: r.name, kills: r.kills, deaths: r.deaths, me: false })),
        ].sort((x, y) => y.kills - x.kills);
        setHud({
          hp: Math.max(0, Math.round(me.hp)), ammo: me.ammo, reloading: me.reloading > 0,
          feed: [...feed], players,
          scores: gameMode === "tdm" ? `GRÜN ${teamScore[0]} : ${teamScore[1]} ROT` : "",
          announce: me.announce && performance.now() / 1000 - me.announce.t < 1.8 ? me.announce.text : null,
          ping: pingVal,
        } as typeof hud & { announce: string | null; ping: number });
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
        if (me.kills > 0) send({ t: "score", name, kills: me.kills, mode: gameMode });
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
  };

  /* ================= UI ================= */
  if (screen === "menu") {
    return (
      <div className="min-h-screen bg-background text-foreground flex items-center justify-center px-6 py-16">
        <div className="max-w-2xl w-full">
          <p className="font-mono text-xs tracking-[0.3em] uppercase text-primary glow-neon-sm mb-3">
            WIRRWARR // ONLINE
          </p>
          <h1 className="text-4xl md:text-5xl font-bold mb-3">
            Echtes <span className="text-primary glow-neon">Multiplayer</span>
          </h1>
          <p className="text-muted-foreground mb-8 leading-relaxed">
            WebSocket-Server, Live-Sync, Namens-Tags, Scoreboard. Öffne das Spiel in zwei Tabs
            oder schick den Link an Freunde. Gleicher Movement-Flow wie im 3D-Game: Sprint, Slide,{" "}
            Crouch, Prone, Mantling, Coyote-Jumps.
          </p>
          <div className="grid sm:grid-cols-2 gap-3 mb-8">
            <button
              type="button"
              onClick={() => start("ffa")}
              className="text-left border border-border bg-card rounded-sm p-4 hover:border-primary/60 hover:box-glow-neon transition-all min-h-[44px]"
            >
              <p className="font-bold text-foreground mb-1">Frei für alle</p>
              <p className="font-mono text-[11px] text-muted-foreground">Jeder gegen jeden – offene Lobby.</p>
            </button>
            <button
              type="button"
              onClick={() => start("tdm")}
              className="text-left border border-primary/50 bg-primary/10 rounded-sm p-4 hover:bg-primary/20 box-glow-neon transition-all min-h-[44px]"
            >
              <p className="font-bold text-primary glow-neon-sm mb-1">Team-Deathmatch</p>
              <p className="font-mono text-[11px] text-muted-foreground">Server teilt Teams zu – erstes Team mit 10 Kills.</p>
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
              Online braucht den Custom-Server: lokal <span className="text-foreground font-mono">npm run online</span> starten.
            </p>
          </div>
        </div>
      )}
      {hud.announce && (
        <div className="absolute inset-x-0 top-[28%] flex justify-center pointer-events-none">
          <p className="font-mono text-4xl md:text-6xl font-bold text-primary glow-neon tracking-[0.25em] uppercase animate-pulse-neon">
            {hud.announce}
          </p>
        </div>
      )}
      {result && (
        <div className="absolute inset-x-0 top-24 flex justify-center pointer-events-none">
          <p className={`font-mono text-2xl tracking-[0.2em] uppercase px-6 py-3 border rounded-sm bg-black/70 ${result.startsWith("SIEG") ? "text-primary border-primary/60 glow-neon" : "text-destructive border-destructive/60"}`}>
            {result}
          </p>
        </div>
      )}
      <div className="absolute top-3 left-1/2 -translate-x-1/2 text-center pointer-events-none">
        <p className="font-mono text-sm text-primary glow-neon-sm tracking-wider">
          {mode === "tdm" ? hud.scores || "TDM" : "ONLINE // FFA"}
        </p>
        <p className="font-mono text-[10px] text-muted-foreground tracking-wider mt-1">{name} · PING {hud.ping} ms · Tab = Scoreboard</p>
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
      {/* Chat-Log */}
      <div className="absolute bottom-24 left-3 pointer-events-none space-y-0.5 max-w-[45%]">
        {chatLog.slice(-6).map((c, i) => (
          <p key={i} className="font-mono text-[10px] leading-relaxed bg-black/40 rounded-sm px-1.5 py-0.5 w-max max-w-full">
            <span className={c.teamChat ? "text-accent" : "text-primary"}>{c.teamChat ? "[TEAM] " : ""}{c.own ? "DU" : c.name}</span>
            <span className="text-foreground">: {c.text}</span>
          </p>
        ))}
      </div>
      {/* Chat-Input */}
      {chatOpen && (
        <form
          className="absolute bottom-10 left-1/2 -translate-x-1/2 w-[70%] max-w-md"
          onSubmit={(e) => {
            e.preventDefault();
            if (chatText.trim()) {
              sendRef.current({ t: "chat", text: chatText.trim(), teamChat: chatOpen === "team" });
              setChatLog((l) => [...l.slice(-7), { name: "DU", text: chatText.trim(), teamChat: chatOpen === "team", own: true }]);
            }
            setChatText("");
            setChatOpen(null);
          }}
        >
          <input
            autoFocus
            value={chatText}
            onChange={(e) => setChatText(e.target.value)}
            placeholder={chatOpen === "team" ? "[TEAM-CHAT] Nachricht … (Enter senden, Esc schließen)" : "[ALL-CHAT] Nachricht … (Enter senden, Esc schließen)"}
            className={`w-full bg-black/80 border rounded-sm px-3 py-2 font-mono text-sm outline-none ${chatOpen === "team" ? "border-accent/70 text-accent" : "border-primary/70 text-foreground"}`}
          />
        </form>
      )}
      <div className="absolute bottom-10 right-3 flex flex-col items-end gap-1">
        <button
          type="button"
          onClick={() => vcRef.current?.toggle()}
          className={`font-mono text-[10px] px-3 py-2 rounded-sm border min-h-[36px] ${micOn ? "border-primary/70 text-primary bg-primary/10" : "border-border text-muted-foreground"}`}
        >
          {micOn ? "🎙 MIC AN" : "🔇 MIC AUS"}
        </button>
        {micErr && <p className="font-mono text-[9px] text-destructive">{micErr}</p>}
      </div>
      <p className="absolute bottom-3 left-1/2 -translate-x-1/2 font-mono text-[9px] text-muted-foreground tracking-wider uppercase pointer-events-none">Enter = Chat · U = Team-Chat · Voice = WebRTC-P2P</p>
      <Scoreboard hud={hud} top10={top10} />
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

function Scoreboard({ hud, top10 }: { hud: { players: { name: string; kills: number; deaths: number; me: boolean }[] }; top10: { name: string; kills: number }[] }) {
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
        {top10.length > 0 && (
          <div className="mb-2 pb-2 border-b border-border/50">
            <p className="font-mono text-[9px] text-muted-foreground uppercase tracking-wider mb-1">🏆 Top-10 ({top10.length ? "Server" : ""})</p>
            {top10.slice(0, 5).map((t, i) => (
              <p key={i} className="font-mono text-[10px] text-muted-foreground">{i + 1}. {t.name} – {t.kills}</p>
            ))}
          </div>
        )}
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
