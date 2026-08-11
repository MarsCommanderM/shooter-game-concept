# STW-ENGINE — eigene Engine für SAVE THE WORLD

**Eigene Engine statt Unreal/Unity.** Ziel-Stack laut Bauplan: SDL2 + OpenGL 4.5 + GLM + glTF 2.0,
Renderer-Backend austauschbar (später Web), Binary-Caches gegen Load-Lags.

## Antworten auf die drei Rückfragen (offizielle Entscheidungen)
1. **Forward+ oder Deferred?** → **Forward+** (wie empfohlen: einfacher Start, genug für unsere Lichtmenge;
   Deferred bleibt Option für Phase 4+ wenn viele Lichter dazukommen).
2. **glTF als Hauptformat?** → **Ja.** Loader unterstützt .gltf/.glb (POSITION/NORMAL/TEXCOORD_0,
   metallic-roughness-Materialien, embedded base64 + externe .bin + GLB-BIN-Chunk).
3. **ECS oder simple Scene Objects?** → **ECS light** ab Phase 2: Phase 1 startet mit simplen
   Scene-Objects (Renderer-Handle + Transform), Komponenten-Arrays kommen mit Gameplay-Systemen dazu.

## Status: Phase 1 ✅ + Phase 2 ✅ + Shadows ✅ (kompiliert)
- **Phase 2**: Render-Pässe sauber getrennt — `Draw()` enqueued Commands, `EndFrame()` rendert
  Pass 1 (Shadow-Depth) + Pass 2 (Main). **Frustum-Culling** (Gribb/Hartmann-Planes, Bounds-Sphere-Test
  pro Mesh), Depth-Test LEQUAL.
- **Shadows (Phase 4a vorgezogen)**: 2048² Depth-Map, orthografische Light-VP, Front-Culling gegen
  Acne, **PCF 3×3** mit Slope-Bias, Border-Clamp = außerhalb = Licht.

## Status: Phase 1 ✅ kompiliert
- SDL2-Fenster + Game-Loop (dt-geclampt)
- Input: WASD blickrelativ, Shift-Sprint, Maus mit **Pointer-Lock**, ESC quit
- Renderer-Interface `IRenderer` (Backend austauschbar: `CreateGLRenderer()`, später `CreateWebRenderer()`)
- OpenGL-4.5-Core-Backend: VAO/VBO/IBO, Depth + Culling
- **PBR-Shader** (metallic/roughness, Cook-Torrance GGX, ACES-Tonemapping, Gamma) — Phase 3 vorgezogen
- glTF-Loader + **Binary-Cache** (`<asset>.stwc`, wird nur neu gelesen wenn Cache ≥ Quell-MTime)
- Fallback-Testszene: 3 Boxen mit Metallic 0/0.5/1.0 & Roughness-Staffelung (PBR-Check)

## Bauen (Ubuntu/Debian)
```bash
sudo apt install cmake g++ libsdl2-dev libglm-dev
cd stw-engine
mkdir -p build && cd build
cmake .. && make -j$(nproc)
./stw ../assets/test_scene.gltf   # oder ohne Argument für Fallback-Szene
```

## Roadmap (aus dem Bauplan übernommen)
- **Phase 2** ✅: Frustum-Culling, Render-Pässe getrennt, Depth LEQUAL
- **Phase 3**: IBL (Prefiltered Specular + Diffuse Irradiance + BRDF-LUT), Normal-Maps aus glTF
- **Phase 4** ✅ (Basis): Directional Shadow-Map + PCF 3×3 · offen: CSM für große Outdoor-Szenen
- **Phase 5**: Bloom (HDR-Pipeline), SSAO, FXAA→TAA, Volumetric Fog
- **Tools**: Asset-Import-Verzeichnis + Binary-Cache (✅ Cache da), später Editor-Anbindung
- **Gameplay-Port**: Controller-/Waffensystem-Werte 1:1 aus `unity-starter/` übernehmen
  (Feuerraten, FOV-Kicks, Coyote-Time — alles bewehrt im Browser-Prototyp)

## Warum das den Unity/Unreal-Unterschied klein macht
Der „Engine-Look" kommt aus: PBR + IBL + Tonemapping + Shadows + PostFX.
Genau diese Kette bauen wir in Phase 3–5 nach — im eigenen Code, ohne Lizenz, ohne GPU-Server-Zwang,
und derselbe Renderer-Interface läuft später auch als WebGL2-Backend im Browser.
