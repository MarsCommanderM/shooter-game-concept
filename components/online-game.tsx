"use client";

import { useEffect, useRef, useState } from "react";
import * as THREE from "three";
import { updateDaily, loadCallsign, loadUpgOwned, loadDiff, seasonId, ReplayView, loadRange } from "./real-game";

/* ================================================================== */
/* WIRRWARR ONLINE v2 – FFA & TDM, Movement-Polish, Stance-Sync        */
/* ================================================================== */

type OnlineMode = "ffa" | "tdm" | "hq" | "dom" | "inv";
type MapSel = "sector" | "stahl" | "orbital" | "garten";

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
const TURN_KEY = "wirrwarr-turn";
function loadTurn(): { url: string; user: string; cred: string } | null {
  try {
    const t = JSON.parse(localStorage.getItem(TURN_KEY) ?? "null");
    if (t && t.url) return t;
  } catch { /* */ }
  return null;
}
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
function sRadioOnline() {
  const c = ac(); if (!c) return;
  const t = c.currentTime;
  [880, 660].forEach((f, i) => {
    const o = c.createOscillator(); const g = c.createGain();
    o.type = "square"; o.frequency.setValueAtTime(f, t + i * 0.09);
    g.gain.setValueAtTime(0.0001, t + i * 0.09); g.gain.exponentialRampToValueAtTime(0.04, t + i * 0.09 + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + i * 0.09 + 0.09);
    o.connect(g).connect(c.destination); o.start(t + i * 0.09); o.stop(t + i * 0.09 + 0.1);
  });
}
const BANTER_ONLINE: Record<string, string[]> = {
  kill: ["VEGA: „Sauber. Weiter so.“", "JUNO: „Boah, hast du das gesehen?!“", "VEGA: „Effizienz erkannt.“"],
  death: ["JUNO: „Nicht schon wieder!“", "VEGA: „Respawn. Fokus.“"],
  lowhp: ["JUNO: „Deine Vitals! Deckung!“", "VEGA: „Schild kritisch.“"],
  wave: ["VEGA: „Neue Welle. Position halten.“", "JUNO: „Sie kommen. ALLE.“"],
};
function banterOnline(ctx: string) {
  if (Math.random() > 0.5) return;
  const pool = BANTER_ONLINE[ctx];
  pushFeedStatic(`🎙 ${pool[Math.floor(Math.random() * pool.length)]}`);
  sRadioOnline();
}
let pushFeedStatic: (t: string) => void = () => {};
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
  const [mapSel, setMapSel] = useState<MapSel>("sector");
  const [micErr, setMicErr] = useState("");
  const vcRef = useRef<{ toggle: () => void; stop: () => void; rebuild?: (s: "all" | "team") => void } | null>(null);
  const [vcScope, setVcScope] = useState<"all" | "team">("all");
  const [replayOpen, setReplayOpen] = useState(false);
  const [onlineReplay, setOnlineReplay] = useState<null | { mode: string; map: string; frames: number[][]; botTrack: number[][]; botsMeta: [number, string, string][]; events: { t: number; e: string }[] }>(null);
  const [rangeTop, setRangeTop] = useState<{ name: string; acc: number }[]>([]);
  const [profileCard, setProfileCard] = useState<null | { name: string; kills: number; deaths: number; me: boolean }>(null);
  const [clanTop, setClanTop] = useState<{ tag: string; kills: number }[]>([]);
  const [pregameUi, setPregameUi] = useState(0);
  const [myVote, setMyVote] = useState<MapSel | null>(null);
  const vcCtx = useRef<{ team: () => number; has: (pid: number) => number | undefined; pids: () => number[] }>({ team: () => -1, has: () => undefined, pids: () => [] });
  const [name] = useState(() => {
    let cn = "";
    try { cn = localStorage.getItem("wirrwarr-clan") ?? ""; } catch { /* */ }
    const base = loadCallsign() || `KAEMPFER-${Math.floor(10 + Math.random() * 89)}`;
    return cn ? `[${cn}] ${base}` : base;
  });

  useEffect(() => {
    if (screen !== "game") return;
    let stream: MediaStream | null = null;
    let myId = -1;
    const pcs = new Map<number, RTCPeerConnection>();
    const audios = new Map<number, HTMLAudioElement>();
    const turn = loadTurn();
    const CFG: RTCConfiguration = {
      iceServers: [
        { urls: "stun:stun.l.google.com:19302" },
        ...(turn ? [{ urls: turn.url, username: turn.user, credential: turn.cred }] : []),
      ],
    };
    const sendVc = (m: Record<string, unknown>) => sendRef.current(m);
    let scope: "all" | "team" = "all";
    const teamOf = (pid: number) => (pid === myId ? vcCtx.current.team() : vcCtx.current.has(pid));
    const okScope = (pid: number) => scope === "all" || teamOf(pid) === vcCtx.current.team();
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
      if (!okScope(id)) return;
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
        if (m.t === "vc-teamchange") {
          // Gegner aus anderem Scope trennen
          for (const [pid, pc] of [...pcs]) {
            if (scope === "team" && teamOf(pid) !== vcCtx.current.team()) { pc.close(); pcs.delete(pid); const a = audios.get(pid); if (a) { a.srcObject = null; audios.delete(pid); } }
          }
          for (const pid of vcCtx.current.pids()) if (pid < myId) await makeOffer(pid);
        }
        if (m.t === "vc-offer" && stream) {
          if (!okScope(m.from as number)) return;
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
    const rebuild = (s: "all" | "team") => {
      scope = s;
      sendVc({ t: "vc-bye-broadcast" });
      pcs.forEach((pc) => pc.close());
      pcs.clear();
      audios.forEach((a) => { a.srcObject = null; });
      audios.clear();
      for (const pid of vcCtx.current.pids()) if (pid < myId) void makeOffer(pid);
    };
    rebuild("all");
    vcRef.current = {
      rebuild,
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
  useEffect(() => {
    if (screen !== "game") return;
    const iv = window.setInterval(() => {
      // pregame countdown liest engine via shared ref
      setPregameUi(pregameRef.current);
    }, 250);
    return () => window.clearInterval(iv);
  }, [screen]);
  const pregameRef = useRef(0);
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
    requestAnimationFrame(() => initEngine(m, mapSel));
  };

  const initEngine = (gameMode: OnlineMode, mapSel: MapSel, spec = false) => {
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
    const buildWalls = (sel: MapSel) => {
      for (const w of walls) scene.remove(w.mesh);
      walls.length = 0;
      addWall(0, -ARENA, ARENA * 2, 1, 4); addWall(0, ARENA, ARENA * 2, 1, 4);
      addWall(-ARENA, 0, 1, ARENA * 2, 4); addWall(ARENA, 0, 1, ARENA * 2, 4);
      const L: Record<MapSel, [number, number, number, number, number][]> = {
        sector: [
          [0, 0, 5, 5, 3], [-16, -12, 7, 1.2, 3], [16, 12, 7, 1.2, 3], [-16, 12, 1.2, 7, 3], [16, -12, 1.2, 7, 3],
          [-26, 0, 2, 8, 4], [26, 0, 2, 8, 4], [0, -26, 8, 2, 4], [0, 26, 8, 2, 4],
        ],
        stahl: [
          [-10, -12, 1.2, 10, 3], [10, -12, 1.2, 10, 3], [-10, 12, 1.2, 10, 3], [10, 12, 1.2, 10, 3],
          [-22, 0, 8, 1.2, 3], [22, 0, 8, 1.2, 3], [0, -6, 6, 1.2, 2], [0, 6, 6, 1.2, 2], [0, 0, 4, 4, 3],
        ],
        orbital: [
          [-8, -8, 6, 1.1, 2.5], [8, 8, 6, 1.1, 2.5], [-8, 8, 6, 1.1, 2.5], [8, -8, 6, 1.1, 2.5],
          [0, -16, 8, 1.2, 3], [0, 16, 8, 1.2, 3], [-16, 0, 1.2, 8, 3], [16, 0, 1.2, 8, 3],
        ],
        garten: [
          [-10, -10, 2, 2, 3], [10, -10, 2, 2, 3], [-10, 10, 2, 2, 3], [10, 10, 2, 2, 3],
          [-20, 0, 1.5, 6, 3], [20, 0, 1.5, 6, 3], [0, -20, 6, 1.5, 3], [0, 20, 6, 1.5, 3],
          [-5, -12, 1.5, 1.5, 2], [5, 12, 1.5, 1.5, 2], [12, -5, 1.5, 1.5, 2], [-12, 5, 1.5, 1.5, 2],
        ],
      };
      for (const [x, z, w, d, h] of L[sel]) addWall(x, z, w, d, h);
      addWall(-8, 8, 2.5, 2.5, 1.1); addWall(8, -8, 2.5, 2.5, 1.1);
      addWall(-8, -14, 3, 1.2, 1.6); addWall(8, 14, 3, 1.2, 1.6);
      addWall(14, 4, 1.2, 3, 2.2); addWall(-14, -4, 1.2, 3, 2.2);
    };
    buildWalls(mapSel);
    let activeMap: MapSel = mapSel;

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
    const upgOwned = loadUpgOwned();
    const me = {
      id: -1, team: -1, revived: false, spec,
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
    pushFeedStatic = pushFeed;
    const teamScore = [0, 0];
    let ended = false;
    const OBJ_PTS = gameMode === "dom" ? [[-14, 0], [14, 0], [0, -14]] : [[0, 0]];
    const objMeshes: THREE.Mesh[] = [];
    if (gameMode === "hq" || gameMode === "dom") {
      for (const [ox, oz] of OBJ_PTS) {
        const msh = new THREE.Mesh(new THREE.CylinderGeometry(2.2, 2.2, 0.15, 24), new THREE.MeshBasicMaterial({ color: 0x888888, transparent: true, opacity: 0.5 }));
        msh.position.set(ox, 0.08, oz);
        scene.add(msh);
        objMeshes.push(msh);
      }
    }
    const orec = { frames: [] as number[][], rem: [] as number[][], remMeta: [] as [number, string, string][], events: [] as { t: number; e: string }[], acc: 0, racc: 0, t: 0 };
    for (const [pid, r] of remotes.entries()) orec.remMeta.push([pid, r.name, r.color.toString(16).padStart(6, "0")]);
    const objState = { hq: -1, dom: [-1, -1, -1], scores: [0, 0] as number[], over: "" };
    let objAcc = 0;

    /* ---------- Invasion (Koop-Wellen) ---------- */
    const invBots = new Map<number, { mesh: THREE.Mesh; hp: number; x: number; z: number }>();
    const invState = { wave: 0, kills: 0 };
    const invBotsLocal: { id: number; x: number; z: number; hp: number; cd: number }[] = [];
    const pendingHits: { id: number; dmg: number; by: number }[] = [];
    let invBroadcastAcc = 0;
    const getInvMesh = (id: number) => {
      let e = invBots.get(id);
      if (!e) {
        const msh = new THREE.Mesh(new THREE.BoxGeometry(0.7, 1.5, 0.4), new THREE.MeshStandardMaterial({ color: 0x66ff66, emissive: 0x227722 }));
        msh.position.y = 0.75;
        scene.add(msh);
        e = { mesh: msh, hp: 100, x: 0, z: 0 };
        invBots.set(id, e);
      }
      return e;
    };
    let kc: { id: number; t: number } | null = null;
    const streakCamO = { t: 0, pos: new THREE.Vector3() };
    let pregame = gameMode === "inv" ? 0 : 12;
    pregameRef.current = pregame;
    const votes: Record<string, number> = {};
    const votedSet = new Set<number>();
    const vega = { on: false, x: me.x + 2, z: me.z + 2, cd: 0, hold: false };
    let bioAcc = 0;
    const buildBio = (x: number, z: number) => {
      const msh = new THREE.Mesh(new THREE.BoxGeometry(2, 2, 2), new THREE.MeshStandardMaterial({ color: 0x2f9a3a, emissive: 0x1f6a2a }));
      msh.position.set(x, 1, z);
      scene.add(msh);
      walls.push({ x, z, hw: 1, hd: 1, top: 2, mesh: msh });
    };
    const vegaMesh = new THREE.Mesh(new THREE.BoxGeometry(0.6, 1.5, 0.4), new THREE.MeshStandardMaterial({ color: 0xff99cc, emissive: 0x883366 }));
    vegaMesh.position.y = 0.75;
    vegaMesh.visible = false;
    scene.add(vegaMesh);

    /* ---------- WS ---------- */
    const proto = location.protocol === "https:" ? "wss:" : "ws:";
    let ws: WebSocket | null = null;
    try { ws = new WebSocket(`${proto}//${location.host}/ws?mode=${gameMode}-${mapSel}`); } catch { setStatus("error"); }
    const send = (m: unknown) => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(m)); };
    sendRef.current = send;
    vcCtx.current = { team: () => me.team, has: (pid: number) => remotes.get(pid)?.team, pids: () => [...remotes.keys()] };
    send({ t: "top", mode: gameMode });
    send({ t: "rangetop" });
    send({ t: "clantop" });

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
            const glowMul = upgOwned.includes("s1") ? 0.7 : 0.3;
      const body = new THREE.Mesh(new THREE.BoxGeometry(0.7, 1.5, 0.4), new THREE.MeshStandardMaterial({ color, emissive: new THREE.Color(color).multiplyScalar(glowMul) }));
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
            let dmgIn = m.dmg as number;
            if (upgOwned.includes("c1")) dmgIn = Math.round(dmgIn * 0.9);
            me.hp -= dmgIn;
            if (me.hp <= 0 && upgOwned.includes("c3") && !me.revived) {
              me.revived = true; me.hp = 50;
              pushFeed("🧬 CHITIN-OVERDRIVE – wieder im Kampf!");
            }
            if (me.hp <= 0) { kc = { id: m.id as number, t: 2.5 }; banterOnline("death"); }
            else if (me.hp < 30) banterOnline("lowhp");
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
        } else if (m.t === "bots") {
          const seen = new Set<number>();
          for (const row of m.bots as number[][]) {
            const [id, x, z, hp] = row;
            seen.add(id);
            const e = getInvMesh(id);
            e.x = x; e.z = z; e.hp = hp;
            e.mesh.visible = hp > 0;
            e.mesh.position.x += (x - e.mesh.position.x) * 0.5;
            e.mesh.position.z += (z - e.mesh.position.z) * 0.5;
          }
          for (const [id, e] of invBots) if (!seen.has(id)) e.mesh.visible = false;
        } else if (m.t === "wave") {
          invState.wave = m.n as number;
          pushFeed(`🌊 WELLE ${m.n} rollt an!`);
          banterOnline("wave");
        } else if (m.t === "bhit") {
          pendingHits.push({ id: m.id as number, dmg: m.dmg as number, by: m.id2 as number });
        } else if (m.t === "bkill") {
          invState.kills++;
          if (m.by === me.id) {
            me.kills++; banterOnline("kill");
            me.streak++;
            if ([3, 5, 8].includes(me.streak)) {
              const e2 = invBots.get(m.id as number);
              if (e2) { streakCamO.t = 0.9; streakCamO.pos.set(e2.x, 0, e2.z); }
            }
          }
          orec.events.push({ t: Math.round(orec.t * 10) / 10, e: `kill:SPORE-${m.id}` });
          pushFeed(`${m.by === me.id ? "DU" : remotes.get(m.by as number)?.name ?? "?"} ⚡ SPORE-${m.id}`);
        } else if (m.t === "mapvote") {
          if (!votedSet.has(m.from as number)) {
            votedSet.add(m.from as number);
            votes[String(m.map)] = (votes[String(m.map)] ?? 0) + 1;
          }
        } else if (m.t === "prestart") {
          pregame = 0;
        } else if (m.t === "setmap") {
          activeMap = m.map as MapSel;
          buildWalls(activeMap);
          pushFeed(`🗺️ ARENA: ${activeMap.toUpperCase()}`);
        } else if (m.t === "pdmg") {
          if (m.target === me.id && me.hp > 0) {
            me.hp -= m.dmg as number;
            if (me.hp <= 0) { me.deaths++; me.respawnAt = performance.now() / 1000 + 3; }
          }
        } else if (m.t === "obj") {
          objState.hq = m.hq; objState.dom = m.dom; objState.scores = m.scores;
          for (let i = 0; i < objMeshes.length; i++) {
            const own = gameMode === "dom" ? objState.dom[i] : objState.hq;
            (objMeshes[i].material as THREE.MeshBasicMaterial).color.setHex(own === 0 ? 0x22ff55 : own === 1 ? 0xff5544 : 0x888888);
          }
        } else if (m.t === "matchend") {
          if (!ended) { ended = true; setResult(String(m.winner)); }
        } else if (m.t === "top") {
          setTop10((m.top as { name: string; kills: number }[]) ?? []);
        } else if (m.t === "rangetop") {
          setRangeTop((m.top as { name: string; acc: number }[]) ?? []);
        } else if (m.t === "clantop") {
          setClanTop((m.top as { tag: string; kills: number }[]) ?? []);
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
      if (me.spec || pregame > 0) return;
      if (me.fireCd > 0 || me.reloading > 0 || me.ammo <= 0 || me.hp <= 0) return;
      me.fireCd = 0.18;
      me.ammo--;
      sShot();
      raycaster.setFromCamera(new THREE.Vector2(0, 0), camera);
      const targets: THREE.Object3D[] = [];
      for (const r of remotes.values()) {
        if (r.alive && (gameMode === "ffa" || r.team !== me.team)) targets.push(r.body);
      }
      const invMeshes = [...invBots.values()].filter((e) => e.mesh.visible).map((e) => e.mesh);
      const hits = raycaster.intersectObjects([...targets, ...invMeshes, ...walls.map((w) => w.mesh)], false);
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

      if (me.hp <= 0 && !me.spec) {
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
        if (me.spec) {
        const fsp = 14 * dt;
        let fx = 0, fz = 0;
        const a2 = yaw.rotation.y;
        if (keys["KeyW"]) { fx -= Math.sin(a2); fz -= Math.cos(a2); }
        if (keys["KeyS"]) { fx += Math.sin(a2); fz += Math.cos(a2); }
        if (keys["KeyA"]) { fx -= Math.cos(a2); fz += Math.sin(a2); }
        if (keys["KeyD"]) { fx += Math.cos(a2); fz -= Math.sin(a2); }
        const fl = Math.hypot(fx, fz);
        if (fl > 0) { me.x += (fx / fl) * fsp; me.z += (fz / fl) * fsp; }
        if (keys["Space"]) me.y += fsp;
        if (keys["KeyC"]) me.y = Math.max(0, me.y - fsp);
        me.x = Math.max(-39, Math.min(39, me.x)); me.z = Math.max(-39, Math.min(39, me.z)); me.y = Math.min(20, me.y);
      }
      const sprinting = !!keys["ShiftLeft"] && !me.crouch && sp0 > 4;
        const maxSp = (me.prone ? 1.5 : me.crouch ? 2.6 : sprinting ? 9 : 5.2) * (upgOwned.includes("l1") ? 1.08 : 1);
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
      if (streakCamO.t > 0) {
        camera.position.lerp(new THREE.Vector3(me.x, 3.0, me.z), Math.min(1, dt * 6));
        camera.lookAt(streakCamO.pos.clone().add(new THREE.Vector3(0, 1.2, 0)));
      } else if (kc && remotes.get(kc.id)) {
        kc.t -= dt;
        const rk = remotes.get(kc.id)!;
        yaw.position.set(rk.group.position.x, 1.7, rk.group.position.z);
        yaw.rotation.y = rk.group.rotation.y;
        if (kc.t <= 0) kc = null;
      } else {
        kc = null;
        yaw.position.set(me.x, me.camH + me.y + bob - me.landDip * 0.4, me.z);
      }
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
        send({ t: "state", x: me.x, z: me.z, yaw: yaw.rotation.y, hp: me.spec ? -1 : me.hp, stance: me.prone ? 3 : me.crouch ? 2 : 1 });
      }
      // Companion VEGA (Invasion, lokal, shared damage via bhit)
      if (gameMode === "inv" && keys["KeyJ"] !== vega.on) {
        vega.on = !!keys["KeyJ"];
        vegaMesh.visible = vega.on;
        if (vega.on) { vega.x = me.x + 1.5; vega.z = me.z + 1.5; pushFeed("🎙 VEGA: ‚Deckung? Nie gehört. Los.‘"); }
      }
      if (vega.on && keys["KeyF"] !== vega.hold) {
        vega.hold = !!keys["KeyF"];
        pushFeed(vega.hold ? "🎙 VEGA: „Halte Position.“" : "🎙 VEGA: „Folge dir.“");
      }
      if (vega.on && !vega.hold) {
        let bt2: { id: number; x: number; z: number } | null = null; let bd2 = 22;
        for (const [id, e] of invBots) {
          if (!e.mesh.visible) continue;
          const d = Math.hypot(e.x - vega.x, e.z - vega.z);
          if (d < bd2) { bd2 = d; bt2 = { id, x: e.x, z: e.z }; }
        }
        if (bt2) {
          const d = bd2 || 1;
          if (d > 1.8) { vega.x += ((bt2.x - vega.x) / d) * 5 * dt; vega.z += ((bt2.z - vega.z) / d) * 5 * dt; }
          vega.cd -= dt;
          if (d < 2.2 && vega.cd <= 0) { vega.cd = 0.8; send({ t: "bhit", id: bt2.id, dmg: 20, id2: me.id }); }
        } else {
          const d2 = Math.hypot(me.x - vega.x, me.z - vega.z);
          if (d2 > 3) { vega.x += ((me.x - vega.x) / d2) * 5.5 * dt; vega.z += ((me.z - vega.z) / d2) * 5.5 * dt; }
        }
        vegaMesh.position.x = vega.x; vegaMesh.position.z = vega.z;
      }
      if (pregame > 0) {
        pregame -= dt;
        const pids2 = [me.id, ...remotes.keys()];
        const isHost2 = me.id === Math.min(...pids2);
        pregameRef.current = Math.max(0, pregame);
        if (isHost2 && pregame <= 0) {
          let bestMap: MapSel = mapSel; let bestN = -1;
          for (const [mm, n] of Object.entries(votes)) if (n > bestN) { bestN = n; bestMap = mm as MapSel; }
          if (bestMap !== activeMap) { activeMap = bestMap; buildWalls(activeMap); }
          send({ t: "setmap", map: activeMap });
          send({ t: "prestart" });
          pushFeed(`🗺️ ARENA: ${activeMap.toUpperCase()} – LOS!`);
        }
      }
      orec.t += dt;
      orec.acc += dt;
      if (orec.acc >= 0.15) {
        orec.acc = 0;
        orec.frames.push([Math.round(orec.t * 10) / 10, Math.round(me.x * 100) / 100, Math.round(me.z * 100) / 100, Math.round(yaw.rotation.y * 1000) / 1000]);
      }
      orec.racc += dt;
      if (orec.racc >= 0.3) {
        orec.racc = 0;
        for (const [pid, r] of remotes.entries()) orec.rem.push([Math.round(orec.t * 10) / 10, pid, Math.round(r.tx * 100) / 100, Math.round(r.tz * 100) / 100]);
      }
      pingAcc += dt;
      if (pingAcc > 2) { pingAcc = 0; send({ t: "ping", ts: performance.now() }); }

      // Biomass-Wuchs (Host)
      const bioHost = me.id === Math.min(...[me.id, ...remotes.keys()]);
      bioAcc += dt;
      if (bioHost && bioAcc > 45) {
        bioAcc = 0;
        const bx = Math.round((Math.random() - 0.5) * 60);
        const bz = Math.round((Math.random() - 0.5) * 60);
        send({ t: "bio", x: bx, z: bz });
        buildBio(bx, bz);
        pushFeed("🌿 Die Biomass wächst. Neue Deckung. Neue Lanes.");
      }
      // Host: Invasion-Simulation
      if (gameMode === "inv") {
        const pidsInv = [me.id, ...remotes.keys()];
        const isHostInv = me.id === Math.min(...pidsInv);
        if (isHostInv) {
          const diffMulInv = loadDiff() === "apex" ? 1.35 : loadDiff() === "recruit" ? 0.7 : 1;
          const aliveCount = invBotsLocal.filter((b) => b.hp > 0).length;
          if (aliveCount === 0) {
            invState.wave++;
            if (invState.wave > 1 && invState.wave % 3 === 1) {
              const order: MapSel[] = ["sector", "stahl", "orbital", "garten"];
              activeMap = order[(order.indexOf(activeMap) + 1) % order.length];
              buildWalls(activeMap);
              send({ t: "setmap", map: activeMap });
              pushFeed(`🗺️ ARENA-ROTATION: ${activeMap.toUpperCase()}`);
            }
            const n = 3 + invState.wave * 2;
            for (let i = 0; i < n; i++) {
              const a2 = Math.random() * Math.PI * 2;
              invBotsLocal.push({ id: invState.wave * 100 + i, x: Math.cos(a2) * 34, z: Math.sin(a2) * 34, hp: 100, cd: 1 });
            }
            send({ t: "wave", n: invState.wave });
          }
          for (const b of invBotsLocal) {
            if (b.hp <= 0) continue;
            let tx = me.x, tz = me.z, td2 = me.hp > 0 ? Math.hypot(me.x - b.x, me.z - b.z) : 1e9; let tid = me.id;
            for (const r of remotes.values()) {
              if (!r.alive) continue;
              const d = Math.hypot(r.tx - b.x, r.tz - b.z);
              if (d < td2) { td2 = d; tx = r.tx; tz = r.tz; tid = r.id; }
            }
            if (td2 < 1e8) {
              const d = td2 || 1;
              if (d > 1.6) { const bs = 3.6 * (loadDiff() === "apex" ? 1.2 : 1); b.x += ((tx - b.x) / d) * bs * dt; b.z += ((tz - b.z) / d) * bs * dt; }
              b.cd -= dt;
              if (d < 2 && b.cd <= 0) { b.cd = 1; send({ t: "pdmg", target: tid, dmg: Math.round(12 * diffMulInv) }); }
            }
          }
          for (const h of pendingHits.splice(0)) {
            const b = invBotsLocal.find((x) => x.id === h.id);
            if (b && b.hp > 0) {
              b.hp -= h.dmg;
              if (b.hp <= 0) send({ t: "bkill", id: b.id, by: h.by });
            }
          }
          invBroadcastAcc += dt;
          if (invBroadcastAcc >= 0.1) {
            invBroadcastAcc = 0;
            send({ t: "bots", bots: invBotsLocal.filter((b) => b.hp > 0).map((b) => [b.id, Math.round(b.x * 10) / 10, Math.round(b.z * 10) / 10, b.hp]) });
          }
        }
      }

      // Host-autoritative Objectives (HQ / DOM)
      if ((gameMode === "hq" || gameMode === "dom") && !ended) {
        const pids = [me.id, ...remotes.keys()];
        const isHost = me.id === Math.min(...pids);
        if (isHost) {
          objAcc += dt;
          if (objAcc >= 0.5) {
            objAcc = 0;
            const inR = (x: number, z: number, r: number, team: number) => {
              let n = 0;
              if (me.team === team && me.hp > 0 && Math.hypot(me.x - x, me.z - z) < r) n++;
              for (const r2 of remotes.values()) if (r2.team === team && r2.alive && Math.hypot(r2.tx - x, r2.tz - z) < r) n++;
              return n;
            };
            if (gameMode === "hq") {
              const a = inR(0, 0, 4, 0), b2 = inR(0, 0, 4, 1);
              if (a > 0 && b2 === 0) { objState.hq = 0; objState.scores[0] += 0.5; }
              if (b2 > 0 && a === 0) { objState.hq = 1; objState.scores[1] += 0.5; }
            } else {
              OBJ_PTS.forEach((pt, i) => {
                const a = inR(pt[0], pt[1], 3.5, 0), b2 = inR(pt[0], pt[1], 3.5, 1);
                if (a > 0 && b2 === 0) { objState.dom[i] = 0; objState.scores[0] += 0.5; }
                if (b2 > 0 && a === 0) { objState.dom[i] = 1; objState.scores[1] += 0.5; }
              });
            }
            const limit = gameMode === "hq" ? 60 : 100;
            if (objState.scores[0] >= limit || objState.scores[1] >= limit) {
              objState.over = objState.scores[0] > objState.scores[1] ? "TEAM GRÜN gewinnt!" : "TEAM ROT gewinnt!";
              ended = true;
              setResult(objState.over);
            }
            send({ t: "obj", hq: objState.hq, dom: objState.dom, scores: objState.scores.map((s) => Math.floor(s)) });
            for (let i = 0; i < objMeshes.length; i++) {
              const own = gameMode === "dom" ? objState.dom[i] : objState.hq;
              (objMeshes[i].material as THREE.MeshBasicMaterial).color.setHex(own === 0 ? 0x22ff55 : own === 1 ? 0xff5544 : 0x888888);
            }
          }
        }
      }

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
          scores: gameMode === "tdm"
            ? `GRÜN ${teamScore[0]} : ${teamScore[1]} ROT`
            : gameMode === "hq" || gameMode === "dom"
              ? `GRÜN ${Math.floor(objState.scores[0])} : ${Math.floor(objState.scores[1])} ROT · ${gameMode === "hq" ? (objState.hq === 0 ? "HQ: GRÜN" : objState.hq === 1 ? "HQ: ROT" : "HQ: neutral") : `Zonen: ${objState.dom.map((o) => (o === 0 ? "G" : o === 1 ? "R" : "–")).join(" ")}`}`
              : "",
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
        if (!me.spec && me.kills > 0) send({ t: "score", name, kills: me.kills, mode: gameMode, season: seasonId() });
      try {
        if (orec.frames.length > 20 && orec.frames.length < 9000) {
          localStorage.setItem("wirrwarr-online-replay", JSON.stringify({ mode: gameMode, map: mapSel, frames: orec.frames, botTrack: orec.rem, botsMeta: orec.remMeta, events: orec.events, date: new Date().toISOString() }));
        }
      } catch { /* */ }
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
  if (replayOpen && onlineReplay) {
    const WALLS_FOR: Record<string, [number, number, number, number, number, number][]> = {
      sector: [[0, 1.5, 0, 5, 3, 5], [-16, 1.5, -12, 7, 3, 1.2], [16, 1.5, 12, 7, 3, 1.2], [-16, 1.5, 12, 1.2, 3, 7], [16, 1.5, -12, 1.2, 3, 7], [-26, 2, 0, 2, 4, 8], [26, 2, 0, 2, 4, 8], [0, 2, -26, 8, 4, 2], [0, 2, 26, 8, 4, 2]],
      stahl: [[-10, 1.5, -12, 1.2, 3, 10], [10, 1.5, -12, 1.2, 3, 10], [-10, 1.5, 12, 1.2, 3, 10], [10, 1.5, 12, 1.2, 3, 10], [-22, 1.5, 0, 8, 3, 1.2], [22, 1.5, 0, 8, 3, 1.2], [0, 1, -6, 6, 2, 1.2], [0, 1, 6, 6, 2, 1.2], [0, 1.5, 0, 4, 3, 4]],
      orbital: [[-8, 1.25, -8, 6, 2.5, 1.1], [8, 1.25, 8, 6, 2.5, 1.1], [-8, 1.25, 8, 6, 2.5, 1.1], [8, 1.25, -8, 6, 2.5, 1.1], [0, 1.5, -16, 8, 3, 1.2], [0, 1.5, 16, 8, 3, 1.2], [-16, 1.5, 0, 1.2, 3, 8], [16, 1.5, 0, 1.2, 3, 8]],
      garten: [[-10, 1.5, -10, 2, 3, 2], [10, 1.5, -10, 2, 3, 2], [-10, 1.5, 10, 2, 3, 2], [10, 1.5, 10, 2, 3, 2], [-20, 1.5, 0, 1.5, 3, 6], [20, 1.5, 0, 1.5, 3, 6], [0, 1.5, -20, 6, 3, 1.5], [0, 1.5, 20, 6, 3, 1.5]],
    };
    return <ReplayView data={onlineReplay} walls={WALLS_FOR[onlineReplay.map] ?? WALLS_FOR.sector} onExit={() => setReplayOpen(false)} />;
  }
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
              onClick={() => start("inv")}
              className="text-left border border-accent/60 bg-accent/10 rounded-sm p-4 hover:bg-accent/20 transition-all min-h-[44px]"
            >
              <p className="font-bold text-accent glow-neon-sm mb-1">🤖 INVASION (Koop)</p>
              <p className="font-mono text-[11px] text-muted-foreground">Gemeinsam Wellen überleben. Endlos.</p>
            </button>
            <button
              type="button"
              onClick={() => start("hq")}
              className="text-left border border-primary/50 bg-primary/10 rounded-sm p-4 hover:bg-primary/20 box-glow-neon transition-all min-h-[44px]"
            >
              <p className="font-bold text-primary glow-neon-sm mb-1">Hauptquartier</p>
              <p className="font-mono text-[11px] text-muted-foreground">HQ halten = Punkte. 60 gewinnen.</p>
            </button>
            <button
              type="button"
              onClick={() => start("dom")}
              className="text-left border border-primary/50 bg-primary/10 rounded-sm p-4 hover:bg-primary/20 box-glow-neon transition-all min-h-[44px]"
            >
              <p className="font-bold text-primary glow-neon-sm mb-1">Herrschaft</p>
              <p className="font-mono text-[11px] text-muted-foreground">3 Zonen halten. 100 Punkte.</p>
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
          <div className="flex items-center gap-2 mb-6">
            <span className="font-mono text-[10px] tracking-[0.25em] uppercase text-muted-foreground">Arena:</span>
            {(["sector", "stahl"] as MapSel[]).map((mm) => (
              <button
                key={mm}
                type="button"
                onClick={() => setMapSel(mm)}
                className={`font-mono text-[11px] uppercase tracking-wider rounded-sm border px-3 py-2 min-h-[36px] ${mapSel === mm ? "border-primary/70 bg-primary/10 text-primary" : "border-border text-muted-foreground"}`}
              >
                {mm === "sector" ? "Sektor 7" : mm === "stahl" ? "Stahlwiege" : mm === "orbital" ? "Orbitaldock" : "Biomass-Garten"}
              </button>
            ))}
          </div>
          <button
            type="button"
            onClick={() => { setMode("ffa"); setResult(null); setScreen("game"); requestAnimationFrame(() => initEngine("ffa", mapSel, true)); }}
            className="mb-6 text-left border border-border bg-card rounded-sm p-4 hover:border-primary/60 transition-all min-h-[44px]"
          >
            <p className="font-bold text-foreground mb-1">👁 Spectator</p>
            <p className="font-mono text-[11px] text-muted-foreground">Zuschauen + frei fliegen (Space/C) + chatten.</p>
          </button>
          {typeof window !== "undefined" && localStorage.getItem("wirrwarr-online-replay") && (
            <button
              type="button"
              onClick={() => {
                try {
                  const r = JSON.parse(localStorage.getItem("wirrwarr-online-replay")!);
                  setOnlineReplay(r);
                  setReplayOpen(true);
                } catch { /* */ }
              }}
              className="mb-6 text-left border border-border bg-card rounded-sm p-4 hover:border-primary/60 transition-all min-h-[44px]"
            >
              <p className="font-bold text-foreground mb-1">🎥 Letztes Online-Replay</p>
              <p className="font-mono text-[11px] text-muted-foreground">Mit allen Spielern. Auch als Code teilbar.</p>
            </button>
          )}
          <div className="flex flex-wrap items-center gap-2 mb-6">
            <button
              type="button"
              onClick={() => {
                try {
                  const r = loadRange();
                  sendRef.current({ t: "range", name, acc: r.bestAcc, hs: r.hs, season: seasonId() });
                } catch { /* */ }
              }}
              className="font-mono text-[10px] tracking-wider uppercase rounded-sm border border-primary/50 text-primary bg-primary/10 hover:bg-primary/20 px-3 py-2 min-h-[36px]"
            >
              🎯 Range-Bestwert hochladen
            </button>
            {rangeTop.length > 0 && (
              <p className="font-mono text-[10px] text-muted-foreground">
                🎯 Ladder: {rangeTop.slice(0, 4).map((r, i) => `${i + 1}. ${r.name} ${r.acc}%`).join(" · ")}
              </p>
            )}
            {clanTop.length > 0 && (
              <p className="font-mono text-[10px] text-accent">
                🏆 Clan-War: {clanTop.slice(0, 4).map((r, i) => `${i + 1}. [${r.tag}] ${r.kills}`).join(" · ")}
              </p>
            )}
          </div>
          <TurnSettings />
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
        <div className="absolute inset-0 flex items-center justify-center bg-black/70">
          <div className="border border-primary/50 bg-black/90 box-glow-neon rounded-sm p-6 max-w-md w-[92%] text-center">
            <p className={`font-mono text-2xl tracking-[0.2em] uppercase mb-4 ${result.startsWith("SIEG") || result.startsWith("EXFIL") ? "text-primary glow-neon" : "text-destructive"}`}>
              {result}
            </p>
            <p className="font-mono text-sm text-foreground mb-2">Kills: <span className="text-primary">{hud.players.find((x) => x.me)?.kills ?? 0}</span> · Tode: {hud.players.find((x) => x.me)?.deaths ?? 0}</p>
            {(() => {
              const mvp = [...hud.players].sort((a, b) => b.kills - a.kills)[0];
              return mvp ? (
                <p className="font-mono text-sm mb-3">
                  🏆 MVP: <span className={mvp.me ? "text-primary glow-neon-sm" : "text-accent"}>{mvp.name}</span> <span className="text-muted-foreground text-[10px]">({mvp.kills} Kills)</span>
                </p>
              ) : null;
            })()}
            <EndMedals kills={hud.players.find((x) => x.me)?.kills ?? 0} deaths={hud.players.find((x) => x.me)?.deaths ?? 0} win={result.startsWith("SIEG") || result.startsWith("EXFIL")} />
            <button type="button" onClick={() => { setResult(null); setScreen("menu"); }} className="mt-4 font-mono text-xs border border-primary/60 text-primary bg-primary/10 hover:bg-primary/20 rounded-sm px-4 py-2 min-h-[40px]">
              ← ZURÜCK ZUM MENÜ
            </button>
          </div>
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
        <button
          type="button"
          onClick={() => {
            const next = vcScope === "all" ? "team" : "all";
            setVcScope(next);
            vcRef.current?.rebuild?.(next);
          }}
          className={`font-mono text-[10px] px-3 py-2 rounded-sm border min-h-[36px] ${vcScope === "team" ? "border-accent/70 text-accent bg-accent/10" : "border-border text-muted-foreground"}`}
        >
          {vcScope === "team" ? "🔊 NUR TEAM" : "🔊 ALLE"}
        </button>
        {micErr && <p className="font-mono text-[9px] text-destructive">{micErr}</p>}
      </div>
      {pregameUi > 0 && (
        <div className="absolute inset-x-0 top-[18%] flex justify-center pointer-events-none">
          <div className="border border-primary/60 bg-black/85 box-glow-neon rounded-sm p-4 text-center pointer-events-auto">
            <p className="font-mono text-lg text-primary glow-neon mb-2">🗺️ MAP-VOTE · {Math.ceil(pregameUi)} s</p>
            <div className="flex gap-2 justify-center">
              {(["sector", "stahl", "orbital", "garten"] as MapSel[]).map((mm) => (
                <button
                  key={mm}
                  type="button"
                  onClick={() => {
                    setMyVote(mm);
                    sendRef.current({ t: "mapvote", map: mm });
                  }}
                  className={`font-mono text-[10px] uppercase rounded-sm border px-2.5 py-1.5 min-h-[32px] ${myVote === mm ? "border-primary bg-primary/20 text-primary" : "border-border text-muted-foreground"}`}
                >
                  {mm === "sector" ? "Sektor" : mm === "stahl" ? "Stahl" : mm === "orbital" ? "Orbital" : "Garten"}
                </button>
              ))}
            </div>
          </div>
        </div>
      )}
      <p className="absolute bottom-3 left-1/2 -translate-x-1/2 font-mono text-[9px] text-muted-foreground tracking-wider uppercase pointer-events-none">Enter = Chat · U = Team-Chat · Voice = WebRTC-P2P</p>
      <Scoreboard hud={hud} top10={top10} onCard={(p) => setProfileCard(p)} />
      {profileCard && (
        <div className="absolute inset-0 flex items-center justify-center bg-black/50" onClick={() => setProfileCard(null)}>
          <div className="border border-primary/50 bg-black/90 box-glow-neon rounded-sm p-5 w-72" onClick={(e) => e.stopPropagation()}>
            <p className="font-mono text-sm text-primary glow-neon-sm mb-2">{profileCard.name}</p>
            {(() => {
              const m = profileCard.name.match(/^\[([^\]]+)\]/);
              return m ? <p className="font-mono text-[10px] text-accent mb-2">CLAN {m[1]}</p> : null;
            })()}
            <p className="font-mono text-[11px] text-foreground mb-1">Kills: <span className="text-primary">{profileCard.kills}</span></p>
            <p className="font-mono text-[11px] text-foreground mb-1">Tode: <span className="text-destructive">{profileCard.deaths}</span></p>
            <p className="font-mono text-[11px] text-foreground mb-2">K/D: <span className="text-primary">{profileCard.deaths ? (profileCard.kills / profileCard.deaths).toFixed(2) : profileCard.kills.toFixed(1)}</span></p>
            {profileCard.me && <p className="font-mono text-[10px] text-muted-foreground">PING {hud.ping} ms</p>}
            <button type="button" onClick={() => setProfileCard(null)} className="mt-3 font-mono text-[10px] border border-border text-muted-foreground rounded-sm px-2 py-1 min-h-[28px] w-full">SCHLIESSEN</button>
          </div>
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

function EndMedals({ kills, deaths, win }: { kills: number; deaths: number; win: boolean }) {
  const medals: string[] = [];
  if (kills >= 10) medals.push("💀 Zehner-Pack");
  if (kills >= 5 && deaths === 0) medals.push("🛡 Unberührbar");
  if (kills >= 3 && deaths >= 5) medals.push("🩸 Durchgekämpft");
  if (win) medals.push("🏆 Sieg");
  // XP aufs Profil
  useEffect(() => {
    try {
      const key = "wirrwarr-profile";
      const pr = JSON.parse(localStorage.getItem(key) ?? "null") ?? { xp: 0 };
      const gain = kills * 20 + (win ? 150 : 50);
      pr.xp += gain;
      localStorage.setItem(key, JSON.stringify(pr));
    } catch { /* */ }
  }, []);
  return (
    <div className="flex flex-wrap justify-center gap-2">
      {medals.length ? medals.map((m) => (
        <span key={m} className="font-mono text-[10px] text-accent border border-accent/50 bg-accent/10 rounded-sm px-2 py-1">{m}</span>
      )) : <span className="font-mono text-[10px] text-muted-foreground">Keine Medaillen dieses Mal.</span>}
    </div>
  );
}

function TurnSettings() {
  const [open, setOpen] = useState(false);
  const [url, setUrl] = useState("");
  const [user, setUser] = useState("");
  const [cred, setCred] = useState("");
  const [saved, setSaved] = useState(false);
  return (
    <div className="border border-border bg-card rounded-sm p-3 mb-6">
      <button type="button" onClick={() => setOpen(!open)} className="font-mono text-[10px] tracking-wider uppercase text-muted-foreground hover:text-primary min-h-[32px]">
        🎙 Voice-Settings {open ? "▲" : "▼"} <span className="text-muted-foreground/60">(optional: TURN für strenge NATs/Firmennetz)</span>
      </button>
      {open && (
        <div className="grid sm:grid-cols-3 gap-2 mt-3">
          <input value={url} onChange={(e) => setUrl(e.target.value)} placeholder="turn:dein.turn:3478" className="bg-black/60 border border-border rounded-sm px-2 py-1.5 font-mono text-[10px] outline-none" />
          <input value={user} onChange={(e) => setUser(e.target.value)} placeholder="username" className="bg-black/60 border border-border rounded-sm px-2 py-1.5 font-mono text-[10px] outline-none" />
          <input value={cred} onChange={(e) => setCred(e.target.value)} placeholder="credential" type="password" className="bg-black/60 border border-border rounded-sm px-2 py-1.5 font-mono text-[10px] outline-none" />
          <button
            type="button"
            onClick={() => {
              try { localStorage.setItem(TURN_KEY, JSON.stringify({ url, user, cred })); setSaved(true); } catch { /* */ }
            }}
            className="sm:col-span-3 font-mono text-[10px] border border-primary/50 text-primary bg-primary/10 hover:bg-primary/20 rounded-sm px-2 py-1.5 min-h-[32px]"
          >
            {saved ? "✓ Gespeichert" : "TURN speichern (ohne = nur STUN, reicht für die meisten)"}
          </button>
        </div>
      )}
    </div>
  );
}

function Scoreboard({ hud, top10, onCard }: { hud: { players: { name: string; kills: number; deaths: number; me: boolean }[] }; top10: { name: string; kills: number }[]; onCard: (p: { name: string; kills: number; deaths: number; me: boolean }) => void }) {
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
          <button
            key={p.name}
            type="button"
            onClick={() => onCard(p)}
            className={`w-full flex justify-between gap-6 font-mono text-[11px] py-0.5 hover:text-accent ${p.me ? "text-primary" : "text-foreground"}`}
          >
            <span>{p.name}</span>
            <span className="text-muted-foreground">{p.kills} / {p.deaths}</span>
          </button>
        ))}
      </div>
    </div>
  );
}
