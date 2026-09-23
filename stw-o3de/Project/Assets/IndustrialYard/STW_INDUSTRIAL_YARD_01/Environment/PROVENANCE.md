# STW_INDUSTRIAL_YARD_01 environment provenance

- Origin: original procedural geometry generated for STW by
  `tools/blender/generate_industrial_yard.py`.
- External mesh or texture sources: none.
- Authoring tool: Blender 4.5.14 LTS Linux x64.
- Tool archive SHA-256:
  `9ba871ff2ecd36526b77432745980b7e6664ecd0c7ca11c48849073dcfe06da3`.
- Coordinate convention: source Z-up; FBX export forward `-Y`, up `Z`.
- License status: original project work; no third-party asset license attached.
- Validation: `STW_INDUSTRIAL_YARD_01.report.json` records all 56 post-import
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
- North Scrapyard + crane (added 2026-09-23): a second landmark, deliberately
  not a copy of the West Annex. The north wall was split around a 3 m
  doorway; beyond it is an open steel-lattice gantry crane (no walls - a
  real crane reads as girders) reached by two switchback ramps around a
  small tower footprint, topped with a platform and a 15 m cantilevered
  boom walked out over the whole arena as a sniper/camper perch. Short
  crate cover sits at scrapyard ground level. Same bucketing/physics
  pattern as the West Annex; validated by a new Anchor::WestAnnex-style
  envelope (Anchor::NorthScrapyard) since the boom deliberately reaches
  back over the yard well above the 4 m wall height.
- East Containerhof (added 2026-09-23): a third landmark, deliberately not a
  copy of either of the first two. The east wall was split around a 3 m
  doorway; beyond it are stacked shipping containers used as low cover with
  one elevated platform (~2.5 m) reached by a straight ramp - no walls, no
  crane, a genuinely different silhouette and a much lower max height (~2.6 m)
  than either prior landmark. Built as a real greybox pass (simple boxes, no
  high-detail dressing) as the first step of a route-network-first map plan:
  the yard now has three enterable directions (west/north/east) plus a
  fourth planned (south), meant to give the central hof real crossing routes
  rather than a single hub with isolated dead-end arms. Same bucketing/
  physics pattern as the other two; validated by a new Anchor::
  EastContainerhof envelope.
- South Verladezone (added 2026-09-23): fourth and final cardinal landmark.
  The south wall was split around a 3 m doorway; beyond it is a raised
  concrete loading dock platform reached by a concrete ramp (every other
  ramp so far is steel - deliberate material variety), flanked by two
  parked-trailer cover blocks that split the approach into two lanes, plus
  a small crate cluster near the doorway. Completes the yard's crossing
  route network (west<->east, north<->south through the central hof)
  instead of a hub with isolated dead-end arms, per the route-network-first
  map design guide shared this session. Same bucketing/physics pattern as
  the other three; validated by a new Anchor::SouthVerladezone envelope.
- NW connector (added 2026-09-23): the first genuine crossing route beyond
  the four cardinal hub-and-spoke landmarks - an outdoor L-shaped walkway
  linking the West Annex directly to the North Scrapyard, bypassing the
  central hof entirely, in the exterior corner (x<-12, y>12) that had no
  floor at all before this. Flat, no ramp, two sightline-break cover
  pieces. Validated by a new Anchor::ConnectorNW envelope.
- NE connector (added 2026-09-23): mirrors the NW connector - links the
  North Scrapyard directly to the East Containerhof through the exterior
  corner (x>12, y>12). Validated by a new Anchor::ConnectorNE envelope.
- Quality boundary: this is authored production foundation data, not a
  blockout. It is still not the final AAA+ cinematic acceptance until O3DE
  import, Vulkan runtime capture, and T4 visual review pass.

The generated preview is authoring evidence only, not an O3DE/Vulkan capture.
