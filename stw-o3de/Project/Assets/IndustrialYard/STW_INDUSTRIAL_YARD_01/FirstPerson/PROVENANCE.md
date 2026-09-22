# STW_FP_01 provenance

- Origin: original procedural arms, gloves, fictional rifle, skeleton and clips
  generated for STW by `tools/blender/generate_first_person_assets.py`.
- External mesh, texture, weapon-brand, or character sources: none.
- Authoring tool: Blender 4.5.14 LTS Linux x64.
- Tool archive SHA-256:
  `9ba871ff2ecd36526b77432745980b7e6664ecd0c7ca11c48849073dcfe06da3`.
- Coordinate convention: source Z-up; FBX export forward `-Y`, up `Z`.
- Required clips: `idle`, `ads`, `reload`; exported as one O3DE exchange FBX
  per clip (`STW_FP_01.fbx`, `STW_FP_01_ads.fbx`, and
  `STW_FP_01_reload.fbx`). The editable `.blend` remains the multi-clip
  authority.
- Production foundation: a 65-mesh authored first-person set with 36
  weapon-mechanical detail components and 12 glove detail components, while
  preserving the eight-bone rig and all three clip names.
- License status: original project work; no third-party asset license attached.

The Blender preview proves source generation and the local geometry contract;
it is not a final photorealistic acceptance. O3DE import, EMotionFX clip
mapping, gameplay timing, camera composition and T4 visual quality remain gates.
