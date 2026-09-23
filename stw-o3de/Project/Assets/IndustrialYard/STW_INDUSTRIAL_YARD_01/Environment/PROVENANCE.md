# STW_INDUSTRIAL_YARD_01 environment provenance

- Origin: original procedural geometry generated for STW by
  `tools/blender/generate_industrial_yard.py`.
- External mesh or texture sources: none.
- Authoring tool: Blender 4.5.14 LTS Linux x64.
- Tool archive SHA-256:
  `9ba871ff2ecd36526b77432745980b7e6664ecd0c7ca11c48849073dcfe06da3`.
- Coordinate convention: source Z-up; FBX export forward `-Y`, up `Z`.
- License status: original project work; no third-party asset license attached.
- Validation: `STW_INDUSTRIAL_YARD_01.report.json` records all 24 post-import
  target AABBs and nine group identities.
- Production foundation: repeated facade ribs, service bands, fasteners,
  truss/gallery details, beacon hardware, flush hazard treatment, and six
  semantic StandardPBR sources with base-color, metallic, roughness, normal,
  and ambient-occlusion maps.
- West Annex (added 2026-09-23): the yard's west wall was split around a 3 m
  doorway; beyond it is the first enterable, multi-storey structure - a real
  walkable ramp (not a lift/teleport) climbs from the ground floor to a
  genuine upper floor, open above the 3.5 m wall height on every side as an
  unglazed window/balcony over the arena (no roof yet - open-top, not a
  gap in coverage). All new pieces are bucketed into the existing deck/
  wall/struct groups, so the gate's 9-group contract is unchanged. Physics
  collision for it lives in `PhysXArenaRuntime` alongside the rest of the
  arena's static colliders, not in this Blender-only visual set.
- Quality boundary: this is authored production foundation data, not a
  blockout. It is still not the final AAA+ cinematic acceptance until O3DE
  import, Vulkan runtime capture, and T4 visual review pass.

The generated preview is authoring evidence only, not an O3DE/Vulkan capture.
