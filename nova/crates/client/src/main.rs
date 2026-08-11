//! NOVA-Client (Milestone 01): winit-Fenster + wgpu-Renderer (WGSL, PBR, Shadows)
//! + FPS-Controller + glTF-Loading. Renderer-Backend = wgpu → später WASM/WebGPU-fähig.
use std::cell::RefCell;
use std::collections::HashSet;
use std::rc::Rc;
use std::time::Duration;
use timec::Instant;

mod net;
use net::{Net, PendingInput};

use bytemuck::{Pod, Zeroable};
use glam::{Mat4, Vec3, Vec4};
use nova_core::maps;
#[cfg(target_arch = "wasm32")]
use std::sync::atomic::{AtomicBool, Ordering};

#[cfg(target_arch = "wasm32")]
static FIRE_HELD: AtomicBool = AtomicBool::new(false);
#[cfg(target_arch = "wasm32")]
static WEAP_SWITCH: AtomicBool = AtomicBool::new(false);

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen::prelude::wasm_bindgen]
pub fn nova_weap() {
    WEAP_SWITCH.store(true, Ordering::SeqCst);
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen::prelude::wasm_bindgen]
pub fn nova_fire(v: bool) {
    FIRE_HELD.store(v, Ordering::SeqCst);
}

pub mod timec {
    #[cfg(not(target_arch = "wasm32"))]
    pub use std::time::Instant;

    #[cfg(target_arch = "wasm32")]
    #[derive(Clone, Copy, PartialEq, PartialOrd)]
    pub struct Instant(pub f64);
    #[cfg(target_arch = "wasm32")]
    impl Instant {
        pub fn now() -> Self {
            Instant(
                web_sys::window()
                    .and_then(|w| w.performance())
                    .map(|p| p.now())
                    .unwrap_or(0.0),
            )
        }
        pub fn elapsed(&self) -> std::time::Duration {
            Instant::now().duration_since(*self)
        }
        pub fn duration_since(&self, earlier: Instant) -> std::time::Duration {
            std::time::Duration::from_secs_f64(((self.0 - earlier.0) / 1000.0).max(0.0))
        }
        pub fn checked_sub(&self, d: std::time::Duration) -> Option<Instant> {
            let ms = self.0 - d.as_secs_f64() * 1000.0;
            if ms < 0.0 { None } else { Some(Instant(ms)) }
        }
    }
    #[cfg(target_arch = "wasm32")]
    impl std::ops::Sub for Instant {
        type Output = std::time::Duration;
        fn sub(self, rhs: Instant) -> std::time::Duration {
            std::time::Duration::from_secs_f64(((self.0 - rhs.0) / 1000.0).max(0.0))
        }
    }
}

#[cfg(target_arch = "wasm32")]
fn log(t: &str) {
    web_sys::console::log_1(&t.into());
}
#[cfg(not(target_arch = "wasm32"))]
fn log(t: &str) { println!("{}", t); }

#[cfg(target_arch = "wasm32")]
mod audio {
    use std::sync::OnceLock;
    use web_sys::{AudioContext, OscillatorType};

    fn ctx() -> &'static AudioContext {
        static C: OnceLock<AudioContext> = OnceLock::new();
        C.get_or_init(|| AudioContext::new().unwrap())
    }

    fn beep(f0: f32, f1: f32, dur: f32, vol: f32, kind: OscillatorType) {
        let c = ctx();
        let _ = c.resume();
        let t = c.current_time();
        let Ok(o) = c.create_oscillator() else { return };
        let Ok(g) = c.create_gain() else { return };
        o.set_type(kind);
        let _ = o.frequency().set_value_at_time(f0, t);
        let _ = o.frequency().exponential_ramp_to_value_at_time(f1.max(1.0), t + dur as f64);
        let _ = g.gain().set_value_at_time(0.0001, t);
        let _ = g.gain().exponential_ramp_to_value_at_time(vol, t + 0.01);
        let _ = g.gain().exponential_ramp_to_value_at_time(0.0001, t + dur as f64);
        let _ = o.connect_with_audio_node(&g);
        let _ = g.connect_with_audio_node(&c.destination());
        let _ = o.start_with_when(t);
        let _ = o.stop_with_when(t + dur as f64 + 0.05);
    }

    use web_sys::AudioBuffer;

    fn noise_buf() -> &'static AudioBuffer {
        static N: OnceLock<AudioBuffer> = OnceLock::new();
        N.get_or_init(|| {
            let c = ctx();
            let sr = c.sample_rate();
            let buf = c.create_buffer(1, sr as u32, sr).unwrap();
            let mut data = vec![0.0f32; sr as usize];
            let mut seed = 0x1234567u32;
            for v in data.iter_mut() {
                seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
                *v = (seed as f32 / u32::MAX as f32) * 2.0 - 1.0;
            }
            buf.copy_to_channel(&mut data, 0).unwrap();
            buf
        })
    }

    /// Echter Gunshot: Noise-Burst + Lowpass + Sub-Thunk (kein Piu-Piu)
    pub fn shot(w: u8) {
        let (rate, filt, vol, dur): (f32, f32, f32, f32) = match w {
            0 => (1.0f32, 2500.0, 0.5, 0.12),  // M16
            1 => (0.8, 1800.0, 0.55, 0.14),    // AK
            2 => (1.3, 3200.0, 0.4, 0.08),     // MP5
            3 => (0.9, 2200.0, 0.5, 0.1),      // MG4
            _ => (0.5, 900.0, 0.85, 0.3),      // PUMP
        };
        let c = ctx();
        let _ = c.resume();
        let t = c.current_time();
        if let Ok(src) = c.create_buffer_source() {
            src.set_buffer(Some(noise_buf()));
            let _ = src.playback_rate().set_value(rate);
            if let Ok(f) = c.create_biquad_filter() {
                f.set_type(web_sys::BiquadFilterType::Lowpass);
                let _ = f.frequency().set_value(filt);
                if let Ok(g) = c.create_gain() {
                    let _ = g.gain().set_value_at_time(vol, t);
                    let _ = g.gain().exponential_ramp_to_value_at_time(0.0001, t + dur as f64);
                    let _ = src.connect_with_audio_node(&f);
                    let _ = f.connect_with_audio_node(&g);
                    let _ = g.connect_with_audio_node(&c.destination());
                    let _ = src.start_with_when(t);
                    let _ = src.stop_with_when(t + dur as f64 + 0.05);
                }
            }
        }
        // Sub-Thunk (Brustkorb-Bass)
        beep(110.0, 40.0, dur * 0.8, vol * 0.5, OscillatorType::Sine);
    }
    pub fn hurt() { beep(140.0, 60.0, 0.15, 0.24, OscillatorType::Sine); }
    pub fn hitm() { beep(1200.0, 600.0, 0.06, 0.09, OscillatorType::Sine); }
    pub fn kill() { beep(1600.0, 2400.0, 0.09, 0.11, OscillatorType::Square); }
    pub fn win() { beep(660.0, 990.0, 0.4, 0.15, OscillatorType::Sine); }
}
use wgpu::util::DeviceExt;
use winit::dpi::PhysicalSize;
use winit::event::{DeviceEvent, ElementState, Event, MouseButton, WindowEvent};
use winit::event_loop::{ControlFlow, EventLoop, EventLoopWindowTarget};
use winit::keyboard::{KeyCode, PhysicalKey};
use std::sync::Arc;

use winit::window::{CursorGrabMode, WindowBuilder};

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct FrameU {
    vp: Mat4,
    lvp: Mat4,
    cam: Vec4,
    light_dir: Vec4,
    light_col: Vec4, // xyz Farbe, w = Strength
    amb: Vec4,
}

#[repr(C)]
#[derive(Clone, Copy, Pod, Zeroable)]
struct DrawU {
    model: Mat4,
    base: Vec4,
    params: Vec4, // x = metallic, y = roughness
}

const REMOTE_COLORS: [[f32; 4]; 6] = [
    [1.0, 0.33, 0.27, 1.0], [1.0, 0.8, 0.2, 1.0], [0.2, 0.8, 1.0, 1.0],
    [1.0, 0.42, 0.8, 1.0], [0.6, 1.0, 0.2, 1.0], [0.7, 0.5, 1.0, 1.0],
];

#[derive(Default)]
struct TouchState {
    joy_id: Option<i32>,
    joy_ox: f32, joy_oy: f32,
    joy_x: f32, joy_y: f32,   // normiert -1..1 (y hoch = positiv)
    look_id: Option<i32>,
    look_lx: f32, look_ly: f32,
    look_acc_x: f32, look_acc_y: f32, // pro Frame akkumuliert
}

#[derive(Clone, Copy)]
struct Material {
    base: [f32; 4],
    metallic: f32,
    roughness: f32,
    emissive: f32,
    win: f32,
}

const MAT_CONCRETE: Material = Material { base: [0.45, 0.45, 0.42, 1.0], metallic: 0.05, roughness: 0.9, emissive: 0.0, win: 0.0 };
const MAT_BUILDING: Material = Material { base: [0.35, 0.37, 0.42, 1.0], metallic: 0.1, roughness: 0.8, emissive: 0.0, win: 1.0 };
const MAT_TRUNK: Material = Material { base: [0.25, 0.16, 0.09, 1.0], metallic: 0.0, roughness: 0.95, emissive: 0.0, win: 0.0 };
const MAT_LEAF: Material = Material { base: [0.08, 0.2, 0.07, 1.0], metallic: 0.0, roughness: 0.95, emissive: 0.0, win: 0.0 };
const MAT_CAR: Material = Material { base: [0.4, 0.12, 0.08, 1.0], metallic: 0.7, roughness: 0.5, emissive: 0.0, win: 0.0 };
const MAT_LAMP: Material = Material { base: [1.0, 0.75, 0.4, 1.0], metallic: 0.0, roughness: 0.4, emissive: 3.0, win: 0.0 };
const MAT_METAL: Material = Material { base: [0.34, 0.42, 0.5, 1.0], metallic: 0.9, roughness: 0.3, emissive: 0.0, win: 0.0 };
const MAT_CRATE: Material = Material { base: [0.75, 0.45, 0.15, 1.0], metallic: 0.3, roughness: 0.6, emissive: 0.15, win: 0.0 };
const MAT_NEON: Material = Material { base: [0.13, 1.0, 0.33, 1.0], metallic: 0.0, roughness: 0.4, emissive: 2.4, win: 0.0 };
const MAT_NEON_CYAN: Material = Material { base: [0.2, 0.8, 1.0, 1.0], metallic: 0.0, roughness: 0.4, emissive: 1.8, win: 0.0 };
const MAT_TOWER: Material = Material { base: [0.1, 0.12, 0.14, 1.0], metallic: 0.7, roughness: 0.4, emissive: 0.0, win: 0.0 };

fn boxm(x: f32, y: f32, z: f32, sx: f32, sy: f32, sz: f32, mat: Material) -> (Mat4, Material) {
    (Mat4::from_translation(Vec3::new(x, y, z)) * Mat4::from_scale(Vec3::new(sx, sy, sz)), mat)
}

/// Shard City Lite: gemeinsame Map aus nova-core (Client rendert, Server simuliert)
fn build_arena() -> Vec<(Mat4, Material)> {
    let mut a = Vec::new();
    for bd in maps::shard_city_lite() {
        let mat = match bd.kind {
            1 => MAT_NEON,
            2 => MAT_CRATE,
            3 => MAT_METAL,
            4 => MAT_NEON_CYAN,
            5 => MAT_TOWER,
            _ => MAT_CONCRETE,
        };
        a.push((
            Mat4::from_translation(Vec3::from(bd.pos)) * Mat4::from_scale(Vec3::from(bd.half) * 2.0),
            mat,
        ));
    }
    a
}

struct Tracer {
    from: Vec3,
    to: Vec3,
    life: f32,
    col: [f32; 3],
    thick: f32,
}

struct ModelMesh {
    vb: wgpu::Buffer,
    ib: wgpu::Buffer,
    count: u32,
    mat: Material,
}

const SHADER_SRC: &str = r#"
struct FrameU {
  vp: mat4x4<f32>,
  lvp: mat4x4<f32>,
  cam: vec4<f32>,
  light_dir: vec4<f32>,
  light_col: vec4<f32>,
  amb: vec4<f32>,
};
struct DrawU {
  model: mat4x4<f32>,
  base: vec4<f32>,
  params: vec4<f32>,
};
@group(0) @binding(0) var<uniform> frame: FrameU;
@group(0) @binding(1) var shadow_map: texture_depth_2d;
@group(0) @binding(2) var shadow_smp: sampler;
@group(1) @binding(0) var<uniform> draw: DrawU;

struct VSOut {
  @builtin(position) clip: vec4<f32>,
  @location(0) wpos: vec3<f32>,
  @location(1) nrm: vec3<f32>,
  @location(2) uv: vec2<f32>,
};

@vertex
fn vs_main(@location(0) pos: vec3<f32>, @location(1) nrm: vec3<f32>, @location(2) uv: vec2<f32>) -> VSOut {
  var o: VSOut;
  let w = draw.model * vec4<f32>(pos, 1.0);
  o.wpos = w.xyz;
  o.nrm = normalize((draw.model * vec4<f32>(nrm, 0.0)).xyz);
  o.uv = uv;
  o.clip = frame.vp * w;
  return o;
}

@vertex
fn vs_shadow(@location(0) pos: vec3<f32>) -> @builtin(position) vec4<f32> {
  return frame.lvp * draw.model * vec4<f32>(pos, 1.0);
}

@fragment
fn fs_shadow() -> @location(0) vec4<f32> {
  return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}

fn d_ggx(no_h: f32, r: f32) -> f32 {
  let a = r * r;
  let a2 = a * a;
  let d = no_h * no_h * (a2 - 1.0) + 1.0;
  return a2 / (3.14159265 * d * d + 0.00001);
}
fn g_schlick(no_v: f32, no_l: f32, r: f32) -> f32 {
  var k = (r + 1.0);
  k = k * k / 8.0;
  return (no_v / (no_v * (1.0 - k) + k)) * (no_l / (no_l * (1.0 - k) + k));
}
fn f_schlick(vo_h: f32, f0: vec3<f32>) -> vec3<f32> {
  return f0 + (vec3<f32>(1.0) - f0) * pow(1.0 - vo_h, 5.0);
}
fn aces(x: vec3<f32>) -> vec3<f32> {
  return clamp((x * (2.51 * x + vec3<f32>(0.03))) / (x * (2.43 * x + vec3<f32>(0.59)) + vec3<f32>(0.14)), vec3<f32>(0.0), vec3<f32>(1.0));
}

fn shadow_factor(w: vec3<f32>, no_l: f32) -> f32 {
  let sp = frame.lvp * vec4<f32>(w, 1.0);
  let p = sp.xyz / sp.w * 0.5 + vec3<f32>(0.5);
  if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) { return 1.0; }
  let bias = max(0.004 * (1.0 - no_l), 0.0015);
  let sz = vec2<f32>(1.0 / 2048.0, 1.0 / 2048.0);
  var sh = 0.0;
  for (var dx = -1; dx <= 1; dx++) {
    for (var dy = -1; dy <= 1; dy++) {
      let d = textureSample(shadow_map, shadow_smp, p.xy + vec2<f32>(f32(dx), f32(dy)) * sz);
      if ((p.z - bias) <= d) { sh += 1.0; } else { sh += 0.0; }
    }
  }
  return mix(0.3, 1.0, sh / 9.0);
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
  let n = normalize(in.nrm);
  let v = normalize(frame.cam.xyz - in.wpos);
  let l = normalize(-frame.light_dir.xyz);
  let h = normalize(v + l);
  let no_l = max(dot(n, l), 0.0);
  let no_v = max(dot(n, v), 0.001);
  let no_h = max(dot(n, h), 0.0);
  let vo_h = max(dot(v, h), 0.0);
  let metal = draw.params.x;
  let rough = max(draw.params.y, 0.04);
  let f0 = mix(vec3<f32>(0.04), draw.base.xyz, metal);
  let f = f_schlick(vo_h, f0);
  let d = d_ggx(no_h, rough);
  let g = g_schlick(no_v, no_l, rough);
  let spec = d * g * f / (4.0 * no_v * no_l + 0.0001);
  let kd = (vec3<f32>(1.0) - f) * (1.0 - metal);
  let sh = shadow_factor(in.wpos, no_l);
  var col = (kd * draw.base.xyz / 3.14159265 + spec) * frame.light_col.xyz * frame.light_col.w * no_l * sh
            + frame.amb.xyz * draw.base.xyz;
  if (draw.params.w > 1.5) { col += vec3<f32>(0.0); }
  col += draw.base.xyz * draw.params.z; // Emissive (Neon)
  col += draw.base.xyz * draw.params.z; // Emissive (Neon)
  if (draw.params.w > 0.5) {
    // Belebte Fassade: Fenster-Grid aus World-Pos, manche warm beleuchtet
    let fp = vec2<f32>((in.wpos.x + in.wpos.z) * 0.35, in.wpos.y * 0.45);
    let cell = floor(fp);
    let fr = fract(fp);
    let hsh = fract(sin(dot(cell, vec2<f32>(12.9898, 78.233))) * 43758.5453);
    let inwin = step(0.18, fr.x) * step(fr.x, 0.82) * step(0.25, fr.y) * step(fr.y, 0.8);
    let lit = step(0.55, hsh);
    col += vec3<f32>(1.0, 0.75, 0.4) * inwin * lit * 1.6;
    col = mix(col, col * 0.55, inwin * (1.0 - lit)); // dunkle Fenster
  }
  col = aces(col);
  col = pow(col, vec3<f32>(1.0 / 2.2));
  return vec4<f32>(col, 1.0);
}

@fragment
fn fs_simple(in: VSOut) -> @location(0) vec4<f32> {
  return vec4<f32>(in.nrm * 0.5 + 0.5, 1.0);
}

/* GLES-sicherer Lite-Shader: Lambert + Emissive + Distanz-Fade */
@fragment
fn fs_lite(in: VSOut) -> @location(0) vec4<f32> {
  let n = normalize(in.nrm);
  let l = normalize(-frame.light_dir.xyz);
  let nl = max(dot(n, l), 0.0);
  var col = draw.base.xyz * (0.35 + 1.1 * nl) * frame.light_col.xyz * 1.6;
  col += draw.base.xyz * draw.params.z;
  let d = length(frame.cam.xyz - in.wpos);
  col = mix(col, vec3<f32>(0.05, 0.03, 0.02), smoothstep(40.0, 110.0, d));
  return vec4<f32>(col, 1.0);
}

/* ---------- Sky-Dome ---------- */
@fragment
fn fs_sky(in: VSOut) -> @location(0) vec4<f32> {
  let d = normalize(in.wpos - frame.cam.xyz);
  let h = clamp(d.y, -1.0, 1.0);
  var col = mix(vec3<f32>(0.38, 0.17, 0.08), vec3<f32>(0.015, 0.04, 0.11), pow(clamp(h * 2.2 + 0.25, 0.0, 1.0), 0.55));
  let sund = normalize(vec3<f32>(-0.55, 0.3, -0.35));
  let sd = max(dot(d, sund), 0.0);
  col += vec3<f32>(1.0, 0.45, 0.18) * (pow(sd, 700.0) * 5.0 + pow(sd, 6.0) * 0.3);
  let st = floor(d * 240.0);
  let hsh = fract(sin(dot(st.xz + st.y, vec2<f32>(12.9898, 78.233))) * 43758.5453);
  col += vec3<f32>(0.7, 0.8, 1.0) * step(0.9985, hsh) * clamp(h * 3.0, 0.0, 1.0) * 0.5;
  return vec4<f32>(col, 1.0);
}

/* ---------- Post: Bloom-Bright + Composite ---------- */
@group(0) @binding(0) var t_scene: texture_2d<f32>;
@group(0) @binding(1) var t_bloom: texture_2d<f32>;
@group(0) @binding(2) var post_smp: sampler;

@vertex
fn vs_post(@builtin(vertex_index) vi: u32) -> @builtin(position) vec4<f32> {
  var p = array<vec2<f32>, 3>(vec2<f32>(-1.0, -3.0), vec2<f32>(3.0, 1.0), vec2<f32>(-1.0, 1.0));
  return vec4<f32>(p[vi], 0.0, 1.0);
}
@fragment
fn fs_bright(@builtin(position) fc: vec4<f32>) -> @location(0) vec4<f32> {
  let dims = textureDimensions(t_scene);
  let uv = vec2<f32>(fc.x / f32(dims.x), 1.0 - fc.y / f32(dims.y));
  let c = textureSample(t_scene, post_smp, uv).rgb;
  let l = dot(c, vec3<f32>(0.299, 0.587, 0.114));
  return vec4<f32>(c * smoothstep(0.4, 0.95, l), 1.0);
}
@fragment
fn fs_comp(@builtin(position) fc: vec4<f32>) -> @location(0) vec4<f32> {
  let dims = textureDimensions(t_scene);
  let uv = vec2<f32>(fc.x / f32(dims.x), 1.0 - fc.y / f32(dims.y));
  let bd = textureDimensions(t_bloom);
  let tx = 1.0 / vec2<f32>(f32(bd.x), f32(bd.y));
  var bloom = vec3<f32>(0.0);
  for (var x = -2; x <= 2; x++) {
    for (var y = -2; y <= 2; y++) {
      bloom += textureSample(t_bloom, post_smp, uv + vec2<f32>(f32(x), f32(y)) * tx * 1.7).rgb;
    }
  }
  bloom /= 25.0;
  var col = textureSample(t_scene, post_smp, uv).rgb + bloom * 1.15;
  let q = uv - 0.5;
  col *= 1.0 - dot(q, q) * 0.6;
  let luma = dot(col, vec3<f32>(0.299, 0.587, 0.114));
  col = mix(vec3<f32>(luma), col, 1.12);
  col = col * vec3<f32>(1.04, 1.0, 0.96);
  return vec4<f32>(col, 1.0);
}
"#;

struct State {
    window: Arc<winit::window::Window>,
    device: wgpu::Device,
    queue: wgpu::Queue,
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
    depth_view: wgpu::TextureView,
    pipeline: wgpu::RenderPipeline,
    shadow_pipeline: wgpu::RenderPipeline,
    bg0: wgpu::BindGroup,
    bgl1: wgpu::BindGroupLayout,
    shadow_view: wgpu::TextureView,
    frame_buf: wgpu::Buffer,
    draw_buf: wgpu::Buffer,
    meshes: Vec<ModelMesh>,
    sky: Option<ModelMesh>,
    arena: Vec<(Mat4, Material)>,
    // Kamera/Controller
    pos: Vec3,
    yaw: f32,
    pitch: f32,
    keys: HashSet<KeyCode>,
    grabbed: bool,
    last: Instant,
    net: Option<Net>,
    seq: u32,
    unit_cube: usize,
    sky_pipeline: Option<wgpu::RenderPipeline>,
    bright_pipeline: Option<wgpu::RenderPipeline>,
    comp_pipeline: Option<wgpu::RenderPipeline>,
    post_bg: Option<wgpu::BindGroup>,
    color_view: Option<wgpu::TextureView>,
    color_tex: Option<wgpu::Texture>,
    bloom_view: Option<wgpu::TextureView>,
    bloom_tex: Option<wgpu::Texture>,
    touch: Rc<RefCell<TouchState>>,
    hp: u32,
    fire_cd: f32,
    fire_held_native: bool,
    tracers: Vec<Tracer>,
    feed: Vec<(String, f32)>,
    feed_clock: f32,
    my_team: u8,
    banner_t: f32,
    banner_text: String,
    weapon: u8,
    hidden: Vec<bool>,
    sim: nova_core::sim::PlayerSim,
    arena_boxes: Vec<nova_core::maps::BoxDef>,
    fx_full: bool,
    fps_acc: f32,
    fps_n: u32,
    fps_val: u32,
}

fn ground_mesh_data() -> (Vec<f32>, Vec<u32>, Material) {
    let h = 24.0f32;
    let pos = vec![
        -h, 0.0, -h, 0.0, 1.0, 0.0, 0.0, 0.0, h, 0.0, -h, 0.0, 1.0, 0.0, 1.0, 0.0, h, 0.0, h, 0.0,
        1.0, 0.0, 1.0, 1.0, -h, 0.0, h, 0.0, 1.0, 0.0, 0.0, 1.0,
    ];
    let idx = vec![0, 1, 2, 0, 2, 3];
    (pos, idx, Material { base: [0.05, 0.09, 0.05, 1.0], metallic: 0.0, roughness: 0.95, emissive: 0.0, win: 0.0 })
}

fn cube_mesh_data() -> (Vec<f32>, Vec<u32>, Material) {
    let c: [[f32; 3]; 8] = [
        [-0.5, -0.5, -0.5], [0.5, -0.5, -0.5], [0.5, 0.5, -0.5], [-0.5, 0.5, -0.5],
        [-0.5, -0.5, 0.5], [0.5, -0.5, 0.5], [0.5, 0.5, 0.5], [-0.5, 0.5, 0.5],
    ];
    let f: [([u32; 4], [f32; 3]); 6] = [
        ([0, 1, 2, 3], [0.0, 0.0, -1.0]), ([5, 4, 7, 6], [0.0, 0.0, 1.0]),
        ([4, 0, 3, 7], [-1.0, 0.0, 0.0]), ([1, 5, 6, 2], [1.0, 0.0, 0.0]),
        ([3, 2, 6, 7], [0.0, 1.0, 0.0]), ([4, 5, 1, 0], [0.0, -1.0, 0.0]),
    ];
    let uvs: [[f32; 2]; 4] = [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]];
    let mut pos = Vec::new();
    let mut idx = Vec::new();
    for (q, (face, n)) in f.iter().enumerate() {
        let b = (q * 4) as u32;
        for (k, vi) in face.iter().enumerate() {
            pos.extend_from_slice(&c[*vi as usize]);
            pos.extend_from_slice(&*n);
            pos.extend_from_slice(&uvs[k]);
        }
        idx.extend_from_slice(&[b, b + 1, b + 2, b, b + 2, b + 3]);
    }
    (pos, idx, Material { base: [0.1, 0.55, 0.16, 1.0], metallic: 0.6, roughness: 0.4, emissive: 0.0, win: 0.0 })
}

fn load_gltf_meshes(path: &str) -> Option<Vec<(Vec<f32>, Vec<u32>, Material)>> {
    let (doc, buffers, _images) = gltf::import(path).ok()?;
    let mut out = Vec::new();
    for mesh in doc.meshes() {
        for prim in mesh.primitives() {
            let reader = prim.reader(|b| Some(&buffers[b.index()]));
            let positions: Vec<[f32; 3]> = reader.read_positions()?.collect();
            let normals: Vec<[f32; 3]> = reader.read_normals()?.collect();
            let uvs: Vec<[f32; 2]> = reader.read_tex_coords(0)?.into_f32().collect();
            let indices: Vec<u32> = reader.read_indices()?.into_u32().collect();
            let pbr = prim.material().pbr_metallic_roughness();
            let bf = pbr.base_color_factor();
            let mat = Material { base: bf, metallic: pbr.metallic_factor(), roughness: pbr.roughness_factor(), emissive: 0.0, win: 0.0 };
            let mut pos = Vec::with_capacity(positions.len() * 8);
            for i in 0..positions.len() {
                pos.extend_from_slice(&positions[i]);
                pos.extend_from_slice(&normals.get(i).copied().unwrap_or([0.0, 1.0, 0.0]));
                pos.extend_from_slice(&uvs.get(i).copied().unwrap_or([0.0, 0.0]));
            }
            out.push((pos, indices, mat));
        }
    }
    if out.is_empty() { None } else { Some(out) }
}

impl State {
    async fn new(window: Arc<winit::window::Window>, fx_full: bool) -> Self {
        log("N1: instance");
        let size = window.inner_size();
        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::all(),
            ..Default::default()
        });
        let surface = instance.create_surface(window.clone()).unwrap();
        log("N3: adapter req");
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                compatible_surface: Some(&surface),
                power_preference: wgpu::PowerPreference::HighPerformance,
                force_fallback_adapter: false,
            })
            .await
            .expect("kein GPU-Adapter gefunden");
        log("N4: device req");
        let (device, queue) = adapter
            .request_device(
                &wgpu::DeviceDescriptor {
                    label: Some("nova"),
                    required_features: wgpu::Features::empty(),
                    required_limits: if cfg!(target_arch = "wasm32") {
                        wgpu::Limits::downlevel_webgl2_defaults()
                    } else {
                        wgpu::Limits::default()
                    },
                },
                None,
            )
            .await
            .expect("Device fehlgeschlagen");

        let config = surface
            .get_default_config(&adapter, size.width.max(1), size.height.max(1))
            .expect("Surface-Config");
        surface.configure(&device, &config);

        log("N5: shader");
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("nova.wgsl"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        log("N6: layouts");
        let bgl0 = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("bgl0"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Depth,
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::NonFiltering),
                    count: None,
                },
            ],
        });
        let bgl1 = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("bgl1"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });
        let layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("layout"),
            bind_group_layouts: &[&bgl0, &bgl1],
            push_constant_ranges: &[],
        });

        let vbuf_layout = wgpu::VertexBufferLayout {
            array_stride: 32,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &[
                wgpu::VertexAttribute { offset: 0, shader_location: 0, format: wgpu::VertexFormat::Float32x3 },
                wgpu::VertexAttribute { offset: 12, shader_location: 1, format: wgpu::VertexFormat::Float32x3 },
                wgpu::VertexAttribute { offset: 24, shader_location: 2, format: wgpu::VertexFormat::Float32x2 },
            ],
        };

        log("N7: main pipeline");
        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("main"),
            layout: Some(&layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: "vs_main",
                buffers: &[vbuf_layout.clone()],
            },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: if cfg!(target_arch = "wasm32") { "fs_lite" } else { "fs_main" },
                targets: &[Some(wgpu::ColorTargetState {
                    format: config.format,
                    blend: Some(wgpu::BlendState::REPLACE),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            primitive: wgpu::PrimitiveState::default(),
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth32Float,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::LessEqual,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
        });

        log("N8: shadow pipeline");
        let shadow_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("shadow"),
            layout: Some(&layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: "vs_shadow",
                buffers: &[vbuf_layout.clone()],
            },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: "fs_shadow",
                targets: &[Some(wgpu::ColorTargetState {
                    format: config.format,
                    blend: Some(wgpu::BlendState::REPLACE),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            primitive: wgpu::PrimitiveState {
                cull_mode: Some(wgpu::Face::Front),
                ..Default::default()
            },
            depth_stencil: Some(wgpu::DepthStencilState {
                format: wgpu::TextureFormat::Depth32Float,
                depth_write_enabled: true,
                depth_compare: wgpu::CompareFunction::LessEqual,
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
        });

        let mut sky_pipeline = None;
        let mut bright_pipeline = None;
        let mut comp_pipeline = None;
        let mut post_bg = None;
        let mut color_view = None;
        let mut color_tex = None;
        let mut bloom_view = None;
        let mut bloom_tex = None;
        let mut sky = None;
        if fx_full {
        // Sky-Pipeline (Front-Culling, invertierte Sphäre)
        log("N9: sky pipeline");
        let sky_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("sky"),
            layout: Some(&layout),
            vertex: wgpu::VertexState { module: &shader, entry_point: "vs_main", buffers: &[vbuf_layout.clone()] },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: "fs_sky",
                targets: &[Some(wgpu::ColorTargetState { format: wgpu::TextureFormat::Rgba8Unorm, blend: Some(wgpu::BlendState::REPLACE), write_mask: wgpu::ColorWrites::ALL })],
            }),
            primitive: wgpu::PrimitiveState { cull_mode: Some(wgpu::Face::Front), ..Default::default() },
            depth_stencil: Some(wgpu::DepthStencilState { format: wgpu::TextureFormat::Depth32Float, depth_write_enabled: false, depth_compare: wgpu::CompareFunction::LessEqual, bias: wgpu::DepthBiasState::default(), stencil: wgpu::StencilState::default() }),
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
        });

        // Post-Processing
        log("N10: post");
        let post_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("post"),
            entries: &[
                wgpu::BindGroupLayoutEntry { binding: 0, visibility: wgpu::ShaderStages::FRAGMENT, ty: wgpu::BindingType::Texture { sample_type: wgpu::TextureSampleType::Float { filterable: true }, view_dimension: wgpu::TextureViewDimension::D2, multisampled: false }, count: None },
                wgpu::BindGroupLayoutEntry { binding: 1, visibility: wgpu::ShaderStages::FRAGMENT, ty: wgpu::BindingType::Texture { sample_type: wgpu::TextureSampleType::Float { filterable: true }, view_dimension: wgpu::TextureViewDimension::D2, multisampled: false }, count: None },
                wgpu::BindGroupLayoutEntry { binding: 2, visibility: wgpu::ShaderStages::FRAGMENT, ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering), count: None },
            ],
        });
        let post_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor { label: Some("postl"), bind_group_layouts: &[&post_bgl], push_constant_ranges: &[] });
        let mk_post = |entry: &str| device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some(entry),
            layout: Some(&post_layout),
            vertex: wgpu::VertexState { module: &shader, entry_point: "vs_post", buffers: &[] },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: entry,
                targets: &[Some(wgpu::ColorTargetState { format: wgpu::TextureFormat::Rgba8Unorm, blend: Some(wgpu::BlendState::REPLACE), write_mask: wgpu::ColorWrites::ALL })],
            }),
            primitive: wgpu::PrimitiveState::default(),
            depth_stencil: None,
            multisample: wgpu::MultisampleState::default(),
            multiview: None,
        });
        let bright_pipeline = mk_post("fs_bright");
        let comp_pipeline = mk_post("fs_comp");

        let (ww, hh) = (size.width.max(1), size.height.max(1));
        log("N11: offscreen tex");
        let color_tex = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("scene"), size: wgpu::Extent3d { width: ww, height: hh, depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING, view_formats: &[],
        });
        let color_view = color_tex.create_view(&wgpu::TextureViewDescriptor::default());
        let bloom_tex = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("bloom"), size: wgpu::Extent3d { width: (ww / 8).max(8), height: (hh / 8).max(8), depth_or_array_layers: 1 },
            mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Rgba8Unorm,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING, view_formats: &[],
        });
        let bloom_view = bloom_tex.create_view(&wgpu::TextureViewDescriptor::default());
        let post_smp = device.create_sampler(&wgpu::SamplerDescriptor { label: Some("postsmp"), mag_filter: wgpu::FilterMode::Linear, min_filter: wgpu::FilterMode::Linear, ..Default::default() });
        let post_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("postbg"), layout: &post_bgl,
            entries: &[
                wgpu::BindGroupEntry { binding: 0, resource: wgpu::BindingResource::TextureView(&color_view) },
                wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(&bloom_view) },
                wgpu::BindGroupEntry { binding: 2, resource: wgpu::BindingResource::Sampler(&post_smp) },
            ],
        });

        // Sky-Sphäre
        sky = Some({
            let (rings, sectors, rad) = (14usize, 24usize, 240.0f32);
            let mut v: Vec<f32> = Vec::new();
            let mut idx: Vec<u32> = Vec::new();
            for r in 0..=rings {
                let phi = std::f32::consts::PI * r as f32 / rings as f32;
                for sc in 0..=sectors {
                    let th = 2.0 * std::f32::consts::PI * sc as f32 / sectors as f32;
                    let p = [rad * phi.sin() * th.cos(), rad * phi.cos(), rad * phi.sin() * th.sin()];
                    v.extend_from_slice(&p);
                    v.extend_from_slice(&[0.0, 1.0, 0.0]);
                    v.extend_from_slice(&[0.0, 0.0]);
                }
            }
            for r in 0..rings {
                for sc in 0..sectors {
                    let aa = (r * (sectors + 1) + sc) as u32;
                    let bb = aa + sectors as u32 + 1;
                    idx.extend_from_slice(&[aa, bb, aa + 1, aa + 1, bb, bb + 1]);
                }
            }
            let vb = device.create_buffer_init(&wgpu::util::BufferInitDescriptor { label: Some("skyvb"), contents: bytemuck::cast_slice(&v), usage: wgpu::BufferUsages::VERTEX });
            let ib = device.create_buffer_init(&wgpu::util::BufferInitDescriptor { label: Some("skyib"), contents: bytemuck::cast_slice(&idx), usage: wgpu::BufferUsages::INDEX });
            ModelMesh { vb, ib, count: idx.len() as u32, mat: MAT_CONCRETE }
        });

        }
        log("N12: shadowmap");
        // Shadow-Map 2048²
        let shadow_tex = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("shadowmap"),
            size: wgpu::Extent3d { width: 2048, height: 2048, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Depth32Float,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });
        let shadow_view = shadow_tex.create_view(&wgpu::TextureViewDescriptor::default());
        let shadow_smp = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("shadow_smp"),
            mag_filter: wgpu::FilterMode::Nearest,
            min_filter: wgpu::FilterMode::Nearest,
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            ..Default::default()
        });

        let frame_buf = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("frame"),
            contents: bytemuck::cast_slice(&[FrameU {
                vp: Mat4::IDENTITY,
                lvp: Mat4::IDENTITY,
                cam: Vec4::new(0.0, 1.7, 4.0, 0.0),
                light_dir: Vec4::new(0.4, -0.8, 0.3, 0.0),
                light_col: Vec4::new(1.0, 0.72, 0.45, 4.5),
                amb: Vec4::new(0.10, 0.12, 0.18, 0.0),
            }]),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        });
        let draw_buf = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("draw"),
            contents: bytemuck::cast_slice(&[DrawU {
                model: Mat4::IDENTITY,
                base: Vec4::ONE,
                params: Vec4::ZERO,
            }]),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        });

        let bg0 = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("bg0"),
            layout: &bgl0,
            entries: &[
                wgpu::BindGroupEntry { binding: 0, resource: frame_buf.as_entire_binding() },
                wgpu::BindGroupEntry { binding: 1, resource: wgpu::BindingResource::TextureView(&shadow_view) },
                wgpu::BindGroupEntry { binding: 2, resource: wgpu::BindingResource::Sampler(&shadow_smp) },
            ],
        });

        // Depth für Main-Pass
        log("N13: depth");
        let depth_tex = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("depth"),
            size: wgpu::Extent3d {
                width: size.width.max(1),
                height: size.height.max(1),
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Depth32Float,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            view_formats: &[],
        });
        let depth_view = depth_tex.create_view(&wgpu::TextureViewDescriptor::default());

        // Einheitswürfel = einzige Geometrie; die Arena ist prozedural instanziert
        let raw: Vec<(Vec<f32>, Vec<u32>, Material)> = vec![cube_mesh_data()];
        let unit_cube = 0usize;

        let meshes = raw
            .into_iter()
            .map(|(pos, idx, mat)| {
                let vb = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
                    label: Some("vb"),
                    contents: bytemuck::cast_slice(&pos),
                    usage: wgpu::BufferUsages::VERTEX,
                });
                let ib = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
                    label: Some("ib"),
                    contents: bytemuck::cast_slice(&idx),
                    usage: wgpu::BufferUsages::INDEX,
                });
                ModelMesh { vb, ib, count: idx.len() as u32, mat }
            })
            .collect();

        log("N16: struct init");
        let arena_val = { log("N15b: arena build"); build_arena() };
        log("N17: arena fertig");
        let touch_val = Rc::new(RefCell::new(TouchState::default()));
        log("N18: touch fertig");
        State {
            window,
            device,
            queue,
            surface,
            config,
            depth_view,
            pipeline,
            shadow_pipeline,
            bg0,
            bgl1,
            shadow_view,
            frame_buf,
            draw_buf,
            meshes,
            sky,
            arena: arena_val,
            pos: Vec3::new(40.0, 1.7, 4.0),
            yaw: std::f32::consts::FRAC_PI_2, // Blick Richtung City-Mitte
            pitch: -0.12,                    // leicht nach unten, damit die City sichtbar ist
            keys: HashSet::new(),
            grabbed: false,
            last: Instant::now(),
            net: None,
            seq: 0,
            unit_cube,
            sky_pipeline,
            bright_pipeline,
            comp_pipeline,
            post_bg,
            color_view,
            color_tex,
            bloom_view,
            bloom_tex,
            touch: touch_val,
            hp: 100,
            fire_cd: 0.0,
            fire_held_native: false,
            tracers: Vec::new(),
            feed: Vec::new(),
            feed_clock: 0.0,
            my_team: 0,
            banner_t: 0.0,
            banner_text: String::new(),
            weapon: 0,
            hidden: Vec::new(),
            sim: nova_core::sim::PlayerSim { pos: [40.0, 0.0, 4.0], vel: [0.0, 0.0, 0.0], yaw: std::f32::consts::FRAC_PI_2, pitch: -0.12 },
            arena_boxes: nova_core::maps::shard_city_lite(),
            fx_full,
            fps_acc: 0.0,
            fps_n: 0,
            fps_val: 0,
        }
    }

    fn resize(&mut self, size: PhysicalSize<u32>) {
        log("RS1: resize start");
        if size.width == 0 || size.height == 0 {
            return;
        }
        self.config.width = size.width;
        self.config.height = size.height;
        self.surface.configure(&self.device, &self.config);
        if self.color_tex.is_some() {
            let (ww, hh) = (size.width.max(1), size.height.max(1));
            let ct = self.device.create_texture(&wgpu::TextureDescriptor {
                label: Some("scene"), size: wgpu::Extent3d { width: ww, height: hh, depth_or_array_layers: 1 },
                mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Rgba8Unorm,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING, view_formats: &[],
            });
            self.color_view = Some(ct.create_view(&wgpu::TextureViewDescriptor::default()));
            self.color_tex = Some(ct);
            let bt = self.device.create_texture(&wgpu::TextureDescriptor {
                label: Some("bloom"), size: wgpu::Extent3d { width: (ww / 8).max(8), height: (hh / 8).max(8), depth_or_array_layers: 1 },
                mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Rgba8Unorm,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING, view_formats: &[],
            });
            self.bloom_view = Some(bt.create_view(&wgpu::TextureViewDescriptor::default()));
            self.bloom_tex = Some(bt);
        }
        let depth_tex = self.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("depth"),
            size: wgpu::Extent3d { width: size.width, height: size.height, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: wgpu::TextureFormat::Depth32Float,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            view_formats: &[],
        });
        self.depth_view = depth_tex.create_view(&wgpu::TextureViewDescriptor::default());
    }

    fn update(&mut self) {
        log("U1: update");
        let now = Instant::now();
        let dt = now.duration_since(self.last).as_secs_f32().min(0.05);
        self.last = now;

        self.sim.yaw = self.yaw;
        self.sim.pitch = self.pitch;
        // Touch: Look-Akku anwenden + Joystick-Wish
        let mut wf = (self.keys.contains(&KeyCode::KeyW) as i32 as f32) - (self.keys.contains(&KeyCode::KeyS) as i32 as f32);
        let mut wr = (self.keys.contains(&KeyCode::KeyD) as i32 as f32) - (self.keys.contains(&KeyCode::KeyA) as i32 as f32);
        let mut sprint = self.keys.contains(&KeyCode::ShiftLeft);
        if self.hp == 0 { wf = 0.0; wr = 0.0; sprint = false; }
        {
            let mut t = self.touch.borrow_mut();
            if t.look_acc_x != 0.0 || t.look_acc_y != 0.0 {
                self.yaw -= t.look_acc_x * 0.0046;
                self.pitch = (self.pitch - t.look_acc_y * 0.0042).clamp(-1.4, 1.4);
                t.look_acc_x = 0.0;
                t.look_acc_y = 0.0;
            }
            if t.joy_id.is_some() {
                let mag = (t.joy_x * t.joy_x + t.joy_y * t.joy_y).sqrt();
                if mag > 0.05 {
                    wr = t.joy_x;
                    wf = t.joy_y;
                    if mag > 0.9 { sprint = true; } // Voll durchdrücken = Sprint
                }
            }
        }
        let wl2 = (wr * wr + wf * wf).sqrt();
        let wish_n = if wl2 > 1.0 { [wr / wl2, wf / wl2] } else { [wr, wf] };
        let sample = nova_core::sim::InputSample { wish: wish_n, sprint };
        nova_core::sim::step(&mut self.sim, &sample, dt, &self.arena_boxes);
        self.pos = Vec3::new(self.sim.pos[0], self.sim.pos[1] + 1.6, self.sim.pos[2]);

        self.feed_clock += dt;
        self.banner_t = (self.banner_t - dt).max(0.0);
        self.fps_acc += dt;
        self.fps_n += 1;
        if self.fps_acc >= 0.5 {
            self.fps_val = (self.fps_n as f32 / self.fps_acc) as u32;
            self.fps_acc = 0.0;
            self.fps_n = 0;
        }
        #[cfg(target_arch = "wasm32")]
        if WEAP_SWITCH.swap(false, Ordering::SeqCst) {
            self.weapon = (self.weapon + 1) % 5;
        }
        for (k, w) in [(KeyCode::Digit1, 0u8), (KeyCode::Digit2, 1), (KeyCode::Digit3, 2), (KeyCode::Digit4, 3), (KeyCode::Digit5, 4)] {
            if self.keys.contains(&k) { self.weapon = w; }
        }
        if self.keys.contains(&KeyCode::KeyQ) { self.weapon = (self.weapon + 1) % 5; }
        self.feed.retain(|(_, t)| self.feed_clock - *t < 5.0);
        self.fire_cd -= dt;
        for t in self.tracers.iter_mut() { t.life -= dt; }
        self.tracers.retain(|t| t.life > 0.0);

        // ---- Combat: Fire (Maus-Lock / Touch-Button), Server entscheidet ----
        #[cfg(target_arch = "wasm32")]
        let touch_fire = FIRE_HELD.load(Ordering::SeqCst);
        #[cfg(not(target_arch = "wasm32"))]
        let touch_fire = false;
        let alive = self.hp > 0;
        if alive && (self.fire_held_native || touch_fire) && self.fire_cd <= 0.0 {
            let w = nova_core::protocol::WEAPONS[self.weapon as usize];
            self.fire_cd = w.0;
            let dir = self.forward();
            let eye = self.pos + Vec3::Y * 1.5;
            if let Some(net) = self.net.as_mut() {
                self.seq += 1;
                net.send_fire(self.seq, dir.to_array(), self.weapon);
            }
            let thick = if self.weapon == 4 { 0.06 } else { 0.03 };
            self.tracers.push(Tracer { from: eye, to: eye + dir * 70.0, life: 0.07, col: [1.0, 0.85, 0.4], thick });
            #[cfg(target_arch = "wasm32")]
            audio::shot(self.weapon);
        }

        // ---- Events: Killfeed + Gegner-Tracer + HP ----
        if let Some(net) = self.net.as_mut() {
            self.hp = net.my_hp();
            self.my_team = net.my_team();
            for ev in net.drain_events() {
                match ev {
                    nova_core::protocol::GameEvent::Kill { killer, victim } => {
                        let a = net.name_of(killer);
                        let b = net.name_of(victim);
                        self.feed.push((format!("{} ⚡ {}", a, b), self.feed_clock));
                        #[cfg(target_arch = "wasm32")]
                        if killer == net.id { audio::kill(); }
                    }
                    nova_core::protocol::GameEvent::Break { bi } => {
                        if (bi as usize) < self.hidden.len() {
                            self.hidden[bi as usize] = true;
                        }
                    }
                    nova_core::protocol::GameEvent::MatchEnd { winner, score } => {
                        self.banner_t = 5.0;
                        self.banner_text = format!("TEAM {} GEWINNT {} : {} — NEUE RUNDE", if winner == 0 { "GRÜN" } else { "ROT" }, score[0], score[1]);
                        self.feed.push((self.banner_text.clone(), self.feed_clock));
                        #[cfg(target_arch = "wasm32")]
                        audio::win();
                    }
                    nova_core::protocol::GameEvent::Damage { from, to, hit, .. } => {
                        if from != net.id {
                            // Gegner-Schuss: Tracer von dessen Snapshot-Pos zum Hit
                            if let Some(fp) = net.remotes(Duration::from_millis(100)).iter().find(|(id, _, _, _)| *id == from).map(|(_, p, _, _)| *p) {
                                self.tracers.push(Tracer { from: Vec3::from(fp) + Vec3::Y * 1.4, to: Vec3::from(hit), life: 0.12, col: [1.0, 0.35, 0.25], thick: 0.03 });
                            }
                        }
                        if to == net.id {
                            self.feed.push((format!("DU getroffen von {}", net.name_of(from)), self.feed_clock));
                            #[cfg(target_arch = "wasm32")]
                            audio::hurt();
                        } else if from == net.id {
                            #[cfg(target_arch = "wasm32")]
                            audio::hitm();
                        }
                    }
                }
            }
        }

        // ---- Reconciliation: Ack-Position vs. Predicted -> Snap ----
        if let Some(net) = self.net.as_mut() {
            if let Some((srv_pos, _ack)) = net.take_server_pos() {
                let dx = srv_pos[0] - self.sim.pos[0];
                let dz = srv_pos[2] - self.sim.pos[2];
                if (dx * dx + dz * dz).sqrt() > 0.35 {
                    self.sim.pos = srv_pos;
                    self.sim.vel = [0.0, 0.0, 0.0];
                    self.pos = Vec3::new(self.sim.pos[0], self.sim.pos[1] + 1.6, self.sim.pos[2]);
                }
            }
            self.seq += 1;
            let mut w2 = [wr, wf];
            let wl = (w2[0] * w2[0] + w2[1] * w2[1]).sqrt();
            if wl > 1.0 { w2 = [w2[0] / wl, w2[1] / wl]; }
            net.send_input(PendingInput { seq: self.seq, wish: w2, yaw: self.yaw, pitch: self.pitch, sprint, buttons: 0, predicted: self.pos.to_array() });
            if let Some((spos, pend)) = net.poll() {
                self.pos = Vec3::from(spos);
                self.pos.y = 1.7;
                for p in pend {
                    let speed = if p.sprint { 8.5 } else { 5.2 };
                    let (psy, pcy) = (p.yaw.sin(), p.yaw.cos());
                    let mv = Vec3::new(p.wish[0] * pcy - p.wish[1] * psy, 0.0, -p.wish[0] * psy - p.wish[1] * pcy);
                    self.pos += mv * speed * nova_core::FIXED_DT;
                }
            }
        }

        // ---- WASM: DOM-HUD (HP, Feed, Respawn) ----
        #[cfg(target_arch = "wasm32")]
        {
            if let Some(doc) = web_sys::window().and_then(|w| w.document()) {
                if let Some(el) = doc.get_element_by_id("hpnum") {
                    el.set_text_content(Some(&format!("{}  |  {}", self.hp, nova_core::protocol::WEAPON_NAMES[self.weapon as usize])));
                }
                if let Some(el) = doc.get_element_by_id("hpbar") {
                    let _ = el.set_attribute("style", &format!("width:{}%;background:{};box-shadow:0 0 8px {}", self.hp, if self.hp > 35 { "#22ff55" } else { "#ff3b30" }, if self.hp > 35 { "rgba(34,255,85,.7)" } else { "rgba(255,59,48,.7)" }));
                }
                if let Some(el) = doc.get_element_by_id("feed") {
                    let txt: Vec<String> = self.feed.iter().rev().take(4).map(|(t, _)| t.clone()).collect();
                    el.set_text_content(Some(&txt.join("\n")));
                }
                if let Some(el) = doc.get_element_by_id("hud") {
                    let n = self.net.as_ref().map(|n| (n.connected, n.id)).unwrap_or((false, 0));
                    el.set_text_content(Some(&format!("STW // fps:{} draws:{} fx:{} conn:{}\nMAP: SHARD CITY\nSERVER: 169.58.152.88", self.fps_val, self.arena.len() + 2, if self.fx_full { "FULL" } else { "SAFE" }, if n.0 { format!("#{}", n.1) } else { "NEIN".into() })));
                }
                if let Some(el) = doc.get_element_by_id("score") {
                    if let Some(n) = self.net.as_ref() {
                        let sc = n.scores();
                        el.set_text_content(Some(&format!("GRÜN {} : {} ROT", sc[0], sc[1])));
                    }
                }
                if let Some(el) = doc.get_element_by_id("banner") {
                    let show = self.banner_t > 0.0;
                    let _ = el.set_attribute("style", &format!("display:{};position:fixed;inset:0;align-items:center;justify-content:center;z-index:13;pointer-events:none;background:rgba(0,0,0,.45);color:#22ff55;font:bold 22px monospace;letter-spacing:.25em;text-align:center", if show { "flex" } else { "none" }));
                    el.set_text_content(Some(&self.banner_text));
                }
                if let Some(el) = doc.get_element_by_id("respawn") {
                    let _ = el.set_attribute("style", &format!("display:{}", if self.hp == 0 { "flex" } else { "none" }));
                }
            }
        }
    }

    fn render(&mut self, remotes: &[(u32, [f32; 3], f32, u8)]) {
        log("R3: render start");
        if self.hidden.len() != self.arena.len() {
            self.hidden.resize(self.arena.len(), false);
        }
        let vp = Mat4::perspective_rh(75.0f32.to_radians(), self.config.width as f32 / self.config.height.max(1) as f32, 0.1, 500.0)
            * Mat4::look_to_rh(self.pos + Vec3::Y * 1.6, self.forward(), Vec3::Y);
        let ldir = Vec3::new(0.55, -0.30, 0.35).normalize(); // Abendsonne
        let eye = -ldir * 60.0;
        let lvp = Mat4::orthographic_rh(-40.0, 40.0, -40.0, 40.0, 1.0, 140.0)
            * Mat4::look_at_rh(eye, Vec3::ZERO, Vec3::Y);

        let frame = FrameU {
            vp,
            lvp,
            cam: Vec4::new(self.pos.x, self.pos.y, self.pos.z, 0.0),
            light_dir: Vec4::new(ldir.x, ldir.y, ldir.z, 0.0),
            light_col: Vec4::new(1.0, 0.72, 0.45, 4.5),
            amb: Vec4::new(0.10, 0.12, 0.18, 0.0),
        };
        self.queue.write_buffer(&self.frame_buf, 0, bytemuck::cast_slice(&[frame]));

        // Draw-Liste: prozedurale Arena + interpolierte Remote-Player
        let mut draws: Vec<(usize, Mat4, Material)> = Vec::new();
        for (k, (model, mat)) in self.arena.iter().enumerate() {
            if self.hidden.get(k) == Some(&true) { continue; }
            draws.push((self.unit_cube, *model, *mat));
        }
        for (pid, p, yaw, team) in remotes {
            let enemy = *team != self.my_team;
            let col: [f32; 4] = if enemy { [1.0, 0.16, 0.12, 1.0] } else { [0.2, 0.85, 1.0, 1.0] };
            let model = Mat4::from_translation(Vec3::new(p[0], p[1] + 0.85, p[2]))
                * Mat4::from_rotation_y(*yaw)
                * Mat4::from_scale(Vec3::new(0.7, 1.7, 0.7));
            draws.push((self.unit_cube, model, Material { base: col, metallic: 0.25, roughness: 0.45, emissive: if enemy { 0.35 } else { 0.5 }, win: 0.0 }));
        }
        for t in &self.tracers {
            let dir = t.to - t.from;
            let len = dir.length().max(0.01);
            let dn = dir / len;
            let rot = Mat4::look_to_rh(Vec3::ZERO, -dn, Vec3::Y);
            let model = Mat4::from_translation((t.from + t.to) * 0.5) * rot * Mat4::from_scale(Vec3::new(t.thick, t.thick, len));
            draws.push((self.unit_cube, model, Material { base: [t.col[0], t.col[1], t.col[2], 1.0], metallic: 0.0, roughness: 0.5, emissive: 3.0, win: 0.0 }));
        }

        let bg1s: Vec<wgpu::BindGroup> = draws.iter().map(|_| self.bg1()).collect();

        // ---- Mobile-Safe-Mode: direkt auf Surface, ohne Offscreen/Bloom/Sky ----
        if !self.fx_full {
            log("R4: mobile pass");
            // DIAG: Testboxen direkt vor der Kamera
            let cam_p = self.pos + Vec3::Y * 1.6;
            let fw = self.forward();
            let rt = Vec3::new(fw.z, 0.0, -fw.x);
            let tests: [(Vec3, Vec3, [f32; 3]); 4] = [
                (cam_p + fw * 5.0, Vec3::new(1.0, 1.0, 1.0), [1.0, 0.0, 0.0]),
                (cam_p + fw * 7.0 + rt * 2.5, Vec3::new(1.0, 1.0, 1.0), [0.0, 1.0, 0.0]),
                (cam_p + fw * 7.0 - rt * 2.5, Vec3::new(1.0, 1.0, 1.0), [0.0, 0.0, 1.0]),
                (Vec3::new(0.0, -0.25, 0.0), Vec3::new(200.0, 0.5, 200.0), [0.4, 0.4, 0.4]),
            ];
            for (tp, ts, tc) in tests.iter() {
                draws.push((self.unit_cube, Mat4::from_translation(*tp) * Mat4::from_scale(*ts), Material { base: [tc[0], tc[1], tc[2], 1.0], metallic: 0.0, roughness: 0.5, emissive: 0.0, win: 0.0 }));
            }
            let out = match self.surface.get_current_texture() {
                Ok(t) => t,
                Err(_) => return,
            };
            let view = out.texture.create_view(&wgpu::TextureViewDescriptor::default());
            let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: Some("m") });
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("main"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view,
                    resolve_target: None,
                    ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color { r: 0.02, g: 0.03, b: 0.07, a: 1.0 }), store: wgpu::StoreOp::Store },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &self.depth_view,
                    depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(1.0), store: wgpu::StoreOp::Store }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
            });
            pass.set_pipeline(&self.pipeline);
            pass.set_bind_group(0, &self.bg0, &[]);
            for (k, (mi, model, mat)) in draws.iter().enumerate() {
                let m = &self.meshes[*mi];
                let du = DrawU { model: *model, base: Vec4::from_array(mat.base), params: Vec4::new(mat.metallic, mat.roughness, mat.emissive, mat.win) };
                self.queue.write_buffer(&self.draw_buf, 0, bytemuck::cast_slice(&[du]));
                pass.set_bind_group(1, &bg1s[k], &[]);
                pass.set_vertex_buffer(0, m.vb.slice(..));
                pass.set_index_buffer(m.ib.slice(..), wgpu::IndexFormat::Uint32);
                pass.draw_indexed(0..m.count, 0, 0..1);
            }
            drop(pass);
            log("R5: submit");
            self.queue.submit(std::iter::once(encoder.finish()));
            log("R6: present");
            out.present();
            log("R7: frame done");
            return;
        }

        // ---- Pass 1: Shadow ----
        let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: Some("frame") });
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("shadow"),
                color_attachments: &[],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &self.shadow_view,
                    depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(1.0), store: wgpu::StoreOp::Store }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
            });
            pass.set_pipeline(&self.shadow_pipeline);
            pass.set_bind_group(0, &self.bg0, &[]);
            for (k, (mi, model, mat)) in draws.iter().enumerate() {
                let m = &self.meshes[*mi];
                let du = DrawU { model: *model, base: Vec4::from_array(mat.base), params: Vec4::new(mat.metallic, mat.roughness, mat.emissive, mat.win) };
                self.queue.write_buffer(&self.draw_buf, 0, bytemuck::cast_slice(&[du]));
                pass.set_bind_group(1, &bg1s[k], &[]);
                pass.set_vertex_buffer(0, m.vb.slice(..));
                pass.set_index_buffer(m.ib.slice(..), wgpu::IndexFormat::Uint32);
                pass.draw_indexed(0..m.count, 0, 0..1);
            }
        }
        self.queue.submit(std::iter::once(encoder.finish()));

        // ---- Pass 2: Main (offscreen) ----
        {
            let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: Some("frame2") });
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("main"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: self.color_view.as_ref().unwrap(),
                    resolve_target: None,
                    ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color { r: 0.012, g: 0.02, b: 0.012, a: 1.0 }), store: wgpu::StoreOp::Store },
                })],
                depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                    view: &self.depth_view,
                    depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(1.0), store: wgpu::StoreOp::Store }),
                    stencil_ops: None,
                }),
                timestamp_writes: None,
                occlusion_query_set: None,
            });
            // Sky zuerst
            pass.set_pipeline(self.sky_pipeline.as_ref().unwrap());
            pass.set_bind_group(0, &self.bg0, &[]);
            pass.set_bind_group(1, &bg1s[0], &[]);
            let du_sky = DrawU { model: Mat4::from_translation(self.pos), base: Vec4::ZERO, params: Vec4::ZERO };
            self.queue.write_buffer(&self.draw_buf, 0, bytemuck::cast_slice(&[du_sky]));
            pass.set_vertex_buffer(0, self.sky.as_ref().unwrap().vb.slice(..));
            pass.set_index_buffer(self.sky.as_ref().unwrap().ib.slice(..), wgpu::IndexFormat::Uint32);
            pass.draw_indexed(0..self.sky.as_ref().unwrap().count, 0, 0..1);

            pass.set_pipeline(&self.pipeline);
            pass.set_bind_group(0, &self.bg0, &[]);
            for (k, (mi, model, mat)) in draws.iter().enumerate() {
                let m = &self.meshes[*mi];
                let du = DrawU { model: *model, base: Vec4::from_array(mat.base), params: Vec4::new(mat.metallic, mat.roughness, mat.emissive, mat.win) };
                self.queue.write_buffer(&self.draw_buf, 0, bytemuck::cast_slice(&[du]));
                pass.set_bind_group(1, &bg1s[k], &[]);
                pass.set_vertex_buffer(0, m.vb.slice(..));
                pass.set_index_buffer(m.ib.slice(..), wgpu::IndexFormat::Uint32);
                pass.draw_indexed(0..m.count, 0, 0..1);
            }
            drop(pass);
            self.queue.submit(std::iter::once(encoder.finish()));
        }

        // ---- Pass 3: Bloom-Bright ----
        {
            let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: Some("bright") });
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("bright"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: self.bloom_view.as_ref().unwrap(),
                    resolve_target: None,
                    ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::BLACK), store: wgpu::StoreOp::Store },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
            });
            pass.set_pipeline(self.bright_pipeline.as_ref().unwrap());
            pass.set_bind_group(0, self.post_bg.as_ref().unwrap(), &[]);
            pass.draw(0..3, 0..1);
            drop(pass);
            self.queue.submit(std::iter::once(encoder.finish()));
        }

        // ---- Pass 4: Composite -> Surface ----
        {
            let out = match self.surface.get_current_texture() {
                Ok(t) => t,
                Err(_) => return,
            };
            let view = out.texture.create_view(&wgpu::TextureViewDescriptor::default());
            let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: Some("comp") });
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("comp"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view,
                    resolve_target: None,
                    ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::BLACK), store: wgpu::StoreOp::Store },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
            });
            pass.set_pipeline(self.comp_pipeline.as_ref().unwrap());
            pass.set_bind_group(0, self.post_bg.as_ref().unwrap(), &[]);
            pass.draw(0..3, 0..1);
            drop(pass);
            self.queue.submit(std::iter::once(encoder.finish()));
            out.present();
        }
    }

    fn forward(&self) -> Vec3 {
        Vec3::new(-self.yaw.sin() * self.pitch.cos(), self.pitch.sin(), -self.yaw.cos() * self.pitch.cos()).normalize()
    }

    fn bg1(&self) -> wgpu::BindGroup {
        self.device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("bg1"),
            layout: &self.bgl1,
            entries: &[wgpu::BindGroupEntry { binding: 0, resource: self.draw_buf.as_entire_binding() }],
        })
    }
}

fn handle(state: &mut State, event: Event<()>, elwt: &EventLoopWindowTarget<()>) {
    log("H1: event");
    elwt.set_control_flow(ControlFlow::Poll);
    match event {
        Event::WindowEvent { event, .. } => match event {
            WindowEvent::CloseRequested => elwt.exit(),
            WindowEvent::Resized(sz) => state.resize(sz),
            WindowEvent::KeyboardInput { event: k, .. } => {
                if let PhysicalKey::Code(code) = k.physical_key {
                    if k.state == ElementState::Pressed {
                        if code == KeyCode::Escape { elwt.exit(); }
                        state.keys.insert(code);
                    } else {
                        state.keys.remove(&code);
                    }
                }
            }
            WindowEvent::MouseInput { state: ElementState::Pressed, button: MouseButton::Left, .. } => {
                if !state.grabbed {
                    state.grabbed = true;
                    let _ = state.window.set_cursor_grab(CursorGrabMode::Locked);
                } else {
                    state.fire_held_native = true;
                }
            }
            WindowEvent::MouseInput { state: ElementState::Released, button: MouseButton::Left, .. } => {
                state.fire_held_native = false;
            }
            _ => {}
        },
        Event::DeviceEvent { event: DeviceEvent::MouseMotion { delta }, .. } => {
            if state.grabbed {
                state.yaw -= delta.0 as f32 * 0.0022;
                state.pitch = (state.pitch - delta.1 as f32 * 0.0022).clamp(-1.4, 1.4);
            }
        }
        Event::AboutToWait => {
            log("R1: frame");
            log("R2: vor update");
            let remotes = state
                .net
                .as_ref()
                .map(|n| n.remotes(Duration::from_millis(100)))
                .unwrap_or_default();
            state.update();
            state.render(&remotes);
        }
        _ => {}
    }
}

#[cfg(not(target_arch = "wasm32"))]
fn main() {
    let event_loop = EventLoop::new().expect("EventLoop");
    let window = Arc::new(
        WindowBuilder::new()
            .with_title("SAVE THE WORLD // NOVA-Engine")
            .with_inner_size(PhysicalSize::new(1280u32, 720u32))
            .build(&event_loop)
            .expect("Fenster"),
    );
    let mut state = pollster::block_on(State::new(window, true));

    let server_addr = std::env::args()
        .collect::<Vec<String>>()
        .windows(2)
        .find(|w| w[0] == "--server")
        .map(|w| w[1].clone())
        .unwrap_or_else(|| "127.0.0.1:27015".into());
    state.net = match Net::connect(&server_addr) {
        Ok(n) => {
            println!("NOVA-Net: verbinde mit {}", server_addr);
            Some(n)
        }
        Err(e) => {
            println!("NOVA-Net: kein Server erreichbar ({}) -> Solo-Modus", e);
            None
        }
    };
    event_loop.run(move |event, elwt| handle(&mut state, event, elwt)).expect("run");
}

#[cfg(target_arch = "wasm32")]
fn main() {
    wasm_bindgen_futures::spawn_local(async_main());
}

#[cfg(target_arch = "wasm32")]
#[wasm_bindgen::prelude::wasm_bindgen(start)]
pub fn wasm_start() {
    main();
}

#[cfg(target_arch = "wasm32")]
async fn async_main() {
    use winit::platform::web::WindowExtWebSys;
    let event_loop = EventLoop::new().expect("EventLoop");
    let window = Arc::new(
        WindowBuilder::new()
            .with_title("SAVE THE WORLD // NOVA")
            .build(&event_loop)
            .expect("window"),
    );
    if let Some(canvas) = window.canvas() {
        let st = canvas.style();
        let _ = st.set_property("position", "fixed");
        let _ = st.set_property("inset", "0");
        let _ = st.set_property("width", "100%");
        let _ = st.set_property("height", "100%");
        if let Some(body) = web_sys::window().and_then(|w| w.document()).and_then(|d| d.body()) {
            let _ = body.append_child(&canvas);
        }
    }
    let ua = web_sys::window()
        .and_then(|w| w.navigator().user_agent().ok())
        .unwrap_or_default()
        .to_lowercase();
    log(&format!("A: ua={}", ua));
    let has_webgpu = web_sys::window()
        .map(|w| js_sys::Reflect::get(&w, &"gpu".into()).ok())
        .flatten()
        .map(|v| !v.is_undefined() && !v.is_null())
        .unwrap_or(false);
    log(&format!("A1: webgpu={}", has_webgpu));
    let fx_full = has_webgpu; // Full-FX (Sky/Bloom) nur mit echter WebGPU, sonst Safe-Pfad
    log("B: State::new start");
    let mut state = State::new(window, fx_full).await;
    log("C: State::new done");

    // ---- Touch-Controls (WASM): links Joystick, rechts Look ----
    {
        use wasm_bindgen::prelude::*;
        use wasm_bindgen::JsCast;
        let touch = state.touch.clone();
        let win = web_sys::window().unwrap();
        let doc = win.document().unwrap();
        let target: &web_sys::EventTarget = doc.as_ref();

        let t2 = touch.clone();
        let on_start = Closure::wrap(Box::new(move |e: web_sys::TouchEvent| {
            e.prevent_default();
            let vw = web_sys::window().map(|w| w.inner_width().map(|v| v.as_f64().unwrap_or(800.0)).unwrap_or(800.0)).unwrap_or(800.0) as f32;
            let mut t = t2.borrow_mut();
            let cl = e.changed_touches();
            for i in 0..cl.length() {
                if let Some(tc) = cl.get(i) {
                    let x = tc.client_x() as f32;
                    let y = tc.client_y() as f32;
                    if x < vw / 2.0 && t.joy_id.is_none() {
                        t.joy_id = Some(tc.identifier());
                        t.joy_ox = x; t.joy_oy = y; t.joy_x = 0.0; t.joy_y = 0.0;
                    } else if t.look_id.is_none() {
                        t.look_id = Some(tc.identifier());
                        t.look_lx = x; t.look_ly = y;
                    }
                }
            }
        }) as Box<dyn FnMut(web_sys::TouchEvent)>);
        target.add_event_listener_with_callback("touchstart", on_start.as_ref().unchecked_ref()).unwrap();
        on_start.forget();

        let t3 = touch.clone();
        let on_move = Closure::wrap(Box::new(move |e: web_sys::TouchEvent| {
            e.prevent_default();
            let mut t = t3.borrow_mut();
            let cl = e.changed_touches();
            for i in 0..cl.length() {
                if let Some(tc) = cl.get(i) {
                    let x = tc.client_x() as f32;
                    let y = tc.client_y() as f32;
                    if t.joy_id == Some(tc.identifier()) {
                        let r = 60.0f32;
                        let mut dx = x - t.joy_ox;
                        let mut dy = y - t.joy_oy;
                        let len = (dx * dx + dy * dy).sqrt().max(1e-3);
                        let clamped = len.min(r);
                        dx = dx / len * clamped;
                        dy = dy / len * clamped;
                        t.joy_x = dx / r;
                        t.joy_y = -dy / r; // hoch = vor
                    } else if t.look_id == Some(tc.identifier()) {
                        t.look_acc_x += x - t.look_lx;
                        t.look_acc_y += y - t.look_ly;
                        t.look_lx = x; t.look_ly = y;
                    }
                }
            }
        }) as Box<dyn FnMut(web_sys::TouchEvent)>);
        target.add_event_listener_with_callback("touchmove", on_move.as_ref().unchecked_ref()).unwrap();
        on_move.forget();

        let t4 = touch;
        let on_end = Closure::wrap(Box::new(move |e: web_sys::TouchEvent| {
            let mut t = t4.borrow_mut();
            let cl = e.changed_touches();
            for i in 0..cl.length() {
                if let Some(tc) = cl.get(i) {
                    if t.joy_id == Some(tc.identifier()) { t.joy_id = None; t.joy_x = 0.0; t.joy_y = 0.0; }
                    if t.look_id == Some(tc.identifier()) { t.look_id = None; }
                }
            }
        }) as Box<dyn FnMut(web_sys::TouchEvent)>);
        target.add_event_listener_with_callback("touchend", on_end.as_ref().unchecked_ref()).unwrap();
        target.add_event_listener_with_callback("touchcancel", on_end.as_ref().unchecked_ref()).unwrap();
        on_end.forget();
    }

    log("A2: nach State::new");
    let host = web_sys::window()
        .and_then(|w| w.location().hostname().ok())
        .unwrap_or_else(|| "169.58.152.88".into());
    state.net = Net::connect(&format!("{}:27015", host)).ok();
    log("A3: start run");
    event_loop
        .run(move |event, elwt| handle(&mut state, event, elwt))
        .expect("run");
}
