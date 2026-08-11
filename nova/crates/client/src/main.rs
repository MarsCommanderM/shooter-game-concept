//! NOVA-Client (Milestone 01): winit-Fenster + wgpu-Renderer (WGSL, PBR, Shadows)
//! + FPS-Controller + glTF-Loading. Renderer-Backend = wgpu → später WASM/WebGPU-fähig.
use std::collections::HashSet;
use std::time::Duration;

mod net;
use net::{Net, PendingInput};
use std::time::Instant;

use bytemuck::{Pod, Zeroable};
use glam::{Mat4, Vec3, Vec4};
use wgpu::util::DeviceExt;
use winit::dpi::PhysicalSize;
use winit::event::{DeviceEvent, ElementState, Event, MouseButton, WindowEvent};
use winit::event_loop::{ControlFlow, EventLoop};
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

#[derive(Clone, Copy)]
struct Material {
    base: [f32; 4],
    metallic: f32,
    roughness: f32,
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
  col = aces(col);
  col = pow(col, vec3<f32>(1.0 / 2.2));
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
}

fn ground_mesh_data() -> (Vec<f32>, Vec<u32>, Material) {
    let h = 24.0f32;
    let pos = vec![
        -h, 0.0, -h, 0.0, 1.0, 0.0, 0.0, 0.0, h, 0.0, -h, 0.0, 1.0, 0.0, 1.0, 0.0, h, 0.0, h, 0.0,
        1.0, 0.0, 1.0, 1.0, -h, 0.0, h, 0.0, 1.0, 0.0, 0.0, 1.0,
    ];
    let idx = vec![0, 1, 2, 0, 2, 3];
    (pos, idx, Material { base: [0.05, 0.09, 0.05, 1.0], metallic: 0.0, roughness: 0.95 })
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
    (pos, idx, Material { base: [0.1, 0.55, 0.16, 1.0], metallic: 0.6, roughness: 0.4 })
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
            let mat = Material { base: bf, metallic: pbr.metallic_factor(), roughness: pbr.roughness_factor() };
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
    async fn new(window: Arc<winit::window::Window>) -> Self {
        let size = window.inner_size();
        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::all(),
            ..Default::default()
        });
        let surface = instance.create_surface(window.clone()).unwrap();
        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                compatible_surface: Some(&surface),
                power_preference: wgpu::PowerPreference::HighPerformance,
                force_fallback_adapter: false,
            })
            .await
            .expect("kein GPU-Adapter gefunden");
        let (device, queue) = adapter
            .request_device(
                &wgpu::DeviceDescriptor {
                    label: Some("nova"),
                    required_features: wgpu::Features::empty(),
                    required_limits: wgpu::Limits::default(),
                },
                None,
            )
            .await
            .expect("Device fehlgeschlagen");

        let config = surface
            .get_default_config(&adapter, size.width.max(1), size.height.max(1))
            .expect("Surface-Config");
        surface.configure(&device, &config);

        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("nova.wgsl"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

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
                entry_point: "fs_main",
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

        let shadow_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("shadow"),
            layout: Some(&layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: "vs_shadow",
                buffers: &[vbuf_layout],
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
                light_col: Vec4::new(1.0, 0.98, 0.92, 3.0),
                amb: Vec4::new(0.06, 0.09, 0.06, 0.0),
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

        // ---- Meshes: glTF laden, Fallback prozedural ----
        let mut raw: Vec<(Vec<f32>, Vec<u32>, Material)> = Vec::new();
        if let Some(m) = load_gltf_meshes("assets/test.gltf") {
            println!("glTF geladen: {} Mesh(es)", m.len());
            raw.extend(m);
        } else {
            println!("glTF nicht gefunden -> Fallback-Cubes");
            raw.push(cube_mesh_data());
        }
        let unit_cube = raw.len();
        raw.push(cube_mesh_data()); // Einheitswürfel für Remote-Player
        raw.push(ground_mesh_data());

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
            pos: Vec3::new(0.0, 1.7, 4.0),
            yaw: 0.0,
            pitch: 0.0,
            keys: HashSet::new(),
            grabbed: false,
            last: Instant::now(),
            net: None,
            seq: 0,
            unit_cube,
        }
    }

    fn resize(&mut self, size: PhysicalSize<u32>) {
        if size.width == 0 || size.height == 0 {
            return;
        }
        self.config.width = size.width;
        self.config.height = size.height;
        self.surface.configure(&self.device, &self.config);
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
        let now = Instant::now();
        let dt = now.duration_since(self.last).as_secs_f32().min(0.05);
        self.last = now;

        let (sy, cy) = (self.yaw.sin(), self.yaw.cos());
        let fwd = Vec3::new(-sy, 0.0, -cy);
        let right = Vec3::new(cy, 0.0, -sy);
        let sprint = self.keys.contains(&KeyCode::ShiftLeft);
        let wf = (self.keys.contains(&KeyCode::KeyW) as i32 as f32) - (self.keys.contains(&KeyCode::KeyS) as i32 as f32);
        let wr = (self.keys.contains(&KeyCode::KeyD) as i32 as f32) - (self.keys.contains(&KeyCode::KeyA) as i32 as f32);
        let mut wish = Vec3::ZERO;
        if wf != 0.0 { wish += fwd * wf; }
        if wr != 0.0 { wish += right * wr; }
        if wish.length() > 0.001 {
            let speed = if sprint { 8.5 } else { 5.2 };
            self.pos += wish.normalize() * speed * dt;
        }
        self.pos.y = 1.7;

        // ---- Netcode: Client-Prediction + Server-Reconciliation (Bauplan §7/§8) ----
        if let Some(net) = self.net.as_mut() {
            self.seq += 1;
            let mut w2 = [wr, wf];
            let wl = (w2[0] * w2[0] + w2[1] * w2[1]).sqrt();
            if wl > 1.0 { w2 = [w2[0] / wl, w2[1] / wl]; }
            net.send_input(PendingInput { seq: self.seq, wish: w2, yaw: self.yaw, sprint, predicted: self.pos.to_array() });
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
    }

    fn render(&mut self, remotes: &[(u32, [f32; 3], f32)]) {
        let vp = Mat4::perspective_rh(75.0f32.to_radians(), self.config.width as f32 / self.config.height.max(1) as f32, 0.1, 500.0)
            * Mat4::look_to_rh(self.pos, self.forward(), Vec3::Y);
        let ldir = Vec3::new(0.4, -0.8, 0.3).normalize();
        let eye = -ldir * 60.0;
        let lvp = Mat4::orthographic_rh(-40.0, 40.0, -40.0, 40.0, 1.0, 140.0)
            * Mat4::look_at_rh(eye, Vec3::ZERO, Vec3::Y);

        let frame = FrameU {
            vp,
            lvp,
            cam: Vec4::new(self.pos.x, self.pos.y, self.pos.z, 0.0),
            light_dir: Vec4::new(ldir.x, ldir.y, ldir.z, 0.0),
            light_col: Vec4::new(1.0, 0.98, 0.92, 3.0),
            amb: Vec4::new(0.06, 0.09, 0.06, 0.0),
        };
        self.queue.write_buffer(&self.frame_buf, 0, bytemuck::cast_slice(&[frame]));

        // Draw-Liste: statische Meshes + interpolierte Remote-Player
        let mut draws: Vec<(usize, Mat4, Material)> = Vec::new();
        for (i, m) in self.meshes.iter().enumerate() {
            if i == self.unit_cube { continue; }
            let model = if i + 1 == self.meshes.len() {
                Mat4::IDENTITY // Ground
            } else if i == 0 && self.meshes.len() > 1 {
                Mat4::from_translation(Vec3::new(0.0, 0.5, -2.0))
            } else {
                Mat4::from_translation(Vec3::new((i as f32 - 1.0) * 2.0, 0.5, -2.0))
            };
            draws.push((i, model, m.mat));
        }
        for (pid, p, yaw) in remotes {
            let col = REMOTE_COLORS[(pid % 6) as usize];
            let model = Mat4::from_translation(Vec3::new(p[0], p[1] + 0.85, p[2]))
                * Mat4::from_rotation_y(*yaw)
                * Mat4::from_scale(Vec3::new(0.7, 1.7, 0.7));
            draws.push((self.unit_cube, model, Material { base: col, metallic: 0.2, roughness: 0.5 }));
        }

        let bg1s: Vec<wgpu::BindGroup> = draws.iter().map(|_| self.bg1()).collect();

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
                let du = DrawU { model: *model, base: Vec4::from_array(mat.base), params: Vec4::new(mat.metallic, mat.roughness, 0.0, 0.0) };
                self.queue.write_buffer(&self.draw_buf, 0, bytemuck::cast_slice(&[du]));
                pass.set_bind_group(1, &bg1s[k], &[]);
                pass.set_vertex_buffer(0, m.vb.slice(..));
                pass.set_index_buffer(m.ib.slice(..), wgpu::IndexFormat::Uint32);
                pass.draw_indexed(0..m.count, 0, 0..1);
            }
        }
        self.queue.submit(std::iter::once(encoder.finish()));

        // ---- Pass 2: Main ----
        {
            let out = match self.surface.get_current_texture() {
                Ok(t) => t,
                Err(_) => return,
            };
            let view = out.texture.create_view(&wgpu::TextureViewDescriptor::default());
            let mut encoder = self.device.create_command_encoder(&wgpu::CommandEncoderDescriptor { label: Some("frame2") });
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("main"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view,
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
            pass.set_pipeline(&self.pipeline);
            pass.set_bind_group(0, &self.bg0, &[]);
            for (k, (mi, model, mat)) in draws.iter().enumerate() {
                let m = &self.meshes[*mi];
                let du = DrawU { model: *model, base: Vec4::from_array(mat.base), params: Vec4::new(mat.metallic, mat.roughness, 0.0, 0.0) };
                self.queue.write_buffer(&self.draw_buf, 0, bytemuck::cast_slice(&[du]));
                pass.set_bind_group(1, &bg1s[k], &[]);
                pass.set_vertex_buffer(0, m.vb.slice(..));
                pass.set_index_buffer(m.ib.slice(..), wgpu::IndexFormat::Uint32);
                pass.draw_indexed(0..m.count, 0, 0..1);
            }
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

fn main() {
    let event_loop = EventLoop::new().expect("EventLoop");
    let window = Arc::new(
        WindowBuilder::new()
            .with_title("SAVE THE WORLD // NOVA-Engine")
            .with_inner_size(PhysicalSize::new(1280u32, 720u32))
            .build(&event_loop)
            .expect("Fenster"),
    );

    let mut state = pollster::block_on(State::new(window));

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

    event_loop
        .run(move |event, elwt| {
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
                        state.grabbed = !state.grabbed;
                        let mode = if state.grabbed { CursorGrabMode::Locked } else { CursorGrabMode::None };
                        let _ = state.window.set_cursor_grab(mode);
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
        })
        .expect("run");
}
