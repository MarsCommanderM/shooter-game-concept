use naga::back::glsl;
use naga::front::wgsl;
use naga::proc;
use naga::valid;

fn main() {
    let src = std::fs::read_to_string(std::env::args().nth(1).unwrap()).unwrap();
    let module = match wgsl::parse_str(&src) {
        Ok(m) => m,
        Err(e) => { println!("WGSL-PARSE-FAIL: {:?}", e); std::process::exit(1); }
    };
    let mut validator = valid::Validator::new(valid::ValidationFlags::all(), valid::Capabilities::all());
    let info = match validator.validate(&module) {
        Ok(i) => i,
        Err(e) => { println!("VALID-FAIL: {:?}", e); std::process::exit(1); }
    };
    let options = glsl::Options {
        version: glsl::Version::Embedded { version: 320, is_webgl: true },
        writer_flags: glsl::WriterFlags::empty(),
        binding_map: Default::default(),
        zero_initialize_workgroup_memory: false,
    };
    // pro Entry-Point testen
    for ep in &module.entry_points {
        let pipeline = if ep.stage == naga::ShaderStage::Vertex {
            glsl::PipelineOptions { shader_stage: naga::ShaderStage::Vertex, entry_point: ep.name.clone(), multiview: None }
        } else {
            glsl::PipelineOptions { shader_stage: naga::ShaderStage::Fragment, entry_point: ep.name.clone(), multiview: None }
        };
        let mut out = String::new();
        let mut writer = glsl::Writer::new(&mut out, &module, &info, &options, &pipeline, proc::BoundsCheckPolicies::default()).unwrap();
        match writer.write() {
            Ok(_) => println!("OK: {}", ep.name),
            Err(e) => println!("GLSL-FAIL [{}]: {:?}", ep.name, e),
        }
    }
}
