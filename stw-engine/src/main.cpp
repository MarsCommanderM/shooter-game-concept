// STW-ENGINE Phase 1: Fenster + Game-Loop + Input + glTF + Renderer-Backend
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstring>

#include "camera.h"
#include "gltf.h"
#include "renderer.h"
#include "IBL.hpp"
#include "HdrLoader.hpp"
#include "TangentGenerator.hpp"
#include "Events.hpp"
#include "Weapons.hpp"
#include "FpsController.hpp"
#include "Targets.hpp"

int main(int argc, char** argv) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    SDL_Log("SDL-Init fehlgeschlagen: %s", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_Window* win = SDL_CreateWindow("SAVE THE WORLD // STW-ENGINE", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 1280, 720,
                                     SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!win) {
    SDL_Log("Fenster fehlgeschlagen: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  stw::IRenderer* ren = stw::CreateGLRenderer();
  if (!ren || !ren->Init(win)) {
    SDL_Log("Renderer-Init fehlgeschlagen");
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  // ---- Assets: glTF laden (mit .stwc-Binary-Cache), Fallback: prozedural ----
  stw::StwModel model;
  bool haveModel = false;
  const char* assetPath = argc > 1 ? argv[1] : "assets/test_scene.gltf";
  if (stw::LoadGLTF(assetPath, model)) haveModel = true;

  // ---- prozedurale HDR-Equirect (Abendhimmel) -> IBL (Phase 3B) ----
  {
    const int W = 256, H = 128;
    std::vector<float> px(W * H * 3);
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        float u = float(x) / (W - 1), v = float(y) / (H - 1);
        float phi = u * 6.28318f, th = v * 3.14159f;
        glm::vec3 d(cosf(phi) * sinf(th), cosf(th), sinf(phi) * sinf(th));
        float h = glm::clamp(d.y, -1.0f, 1.0f);
        glm::vec3 col = glm::mix(glm::vec3(0.55f, 0.22f, 0.08f), glm::vec3(0.02f, 0.05f, 0.14f),
                                 powf(glm::clamp(h * 1.4f + 0.2f, 0.0f, 1.0f), 0.6f));
        glm::vec3 sund = glm::normalize(glm::vec3(-0.55f, 0.22f, -0.35f));
        float sd = std::max(glm::dot(d, sund), 0.0f);
        col += glm::vec3(1.0f, 0.45f, 0.15f) * (powf(sd, 600.f) * 30.f + powf(sd, 6.f) * 0.5f);
        px[(y * W + x) * 3 + 0] = col.r;
        px[(y * W + x) * 3 + 1] = col.g;
        px[(y * W + x) * 3 + 2] = col.b;
      }
    auto ibl = stw::BuildIBLFromEquirect(px.data(), W, H);
    ren->SetIBL(ibl);
    SDL_Log("IBL ready: %d", ibl.ready ? 1 : 0);
  }

  stw::StwMesh cube = stw::MakeCubeMesh();
  cube.tan.resize(cube.pos.size() / 3 * 4);
  stw::GenerateTangentsArrays(cube.pos.data(), cube.nrm.data(), cube.uv.data(), cube.idx.data(),
                              cube.idx.size(), cube.pos.size() / 3, cube.tan.data());
  stw::StwMesh ground = stw::MakeGroundMesh(24.f);
  ground.tan.resize(ground.pos.size() / 3 * 4);
  stw::GenerateTangentsArrays(ground.pos.data(), ground.nrm.data(), ground.uv.data(), ground.idx.data(),
                              ground.idx.size(), ground.pos.size() / 3, ground.tan.data());
  uint32_t hGround = ren->UploadMesh(ground);
  uint32_t hCube = ren->UploadMesh(cube);

  // ---- prozedurale Normalmap (Noppen) für den Abnahme-Test ----
  {
    const int S = 128;
    std::vector<unsigned char> nm(S * S * 4);
    for (int y = 0; y < S; y++)
      for (int x = 0; x < S; x++) {
        // Höhe: Noppen-Grid
        auto hf = [&](int xx, int yy) {
          float gx = fmodf(float(xx), 32.f) - 16.f, gy = fmodf(float(yy), 32.f) - 16.f;
          float d = sqrtf(gx * gx + gy * gy);
          return std::max(0.0f, 1.0f - d / 10.f);
        };
        float hx = hf(x + 1, y) - hf(x - 1, y);
        float hy = hf(x, y + 1) - hf(x, y - 1);
        glm::vec3 n = glm::normalize(glm::vec3(-hx * 2.0f, -hy * 2.0f, 1.0f));
        nm[(y * S + x) * 4 + 0] = (unsigned char)((n.x * 0.5f + 0.5f) * 255);
        nm[(y * S + x) * 4 + 1] = (unsigned char)((n.y * 0.5f + 0.5f) * 255);
        nm[(y * S + x) * 4 + 2] = (unsigned char)((n.z * 0.5f + 0.5f) * 255);
        nm[(y * S + x) * 4 + 3] = 255;
      }
    uint32_t ntex = ren->UploadTextureRGBA(nm.data(), S, S);
    ren->SetNormalTexture(ntex);
  }

  // ---- Phase-3-Abnahmematrix: Metallic (Zeilen) x Roughness (Spalten) ----
  struct TestObj { uint32_t mesh; stw::StwMaterial mat; float model[16]; };
  std::vector<TestObj> testObjs;
  {
    const float roughs[6] = {0.05f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    const float metals[4] = {0.0f, 0.33f, 0.66f, 1.0f};
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < 6; c++) {
        stw::StwMaterial m;
        m.base[0] = 0.85f; m.base[1] = 0.85f; m.base[2] = 0.9f;
        m.metallic = metals[r];
        m.roughness = roughs[c];
        glm::mat4 md = glm::translate(glm::mat4(1.0f), glm::vec3(-20.f + c * 4.f, 0.7f, -30.f + r * 3.f)) *
                       glm::scale(glm::mat4(1.0f), glm::vec3(1.4f));
        TestObj o; o.mesh = hCube; o.mat = m;
        std::memcpy(o.model, glm::value_ptr(md), sizeof(float) * 16);
        testObjs.push_back(o);
      }
    // Normalmap-Demo-Cube
    stw::StwMaterial mn;
    mn.base[0] = 0.7f; mn.base[1] = 0.7f; mn.base[2] = 0.75f;
    mn.metallic = 0.2f; mn.roughness = 0.5f; mn.hasNormal = true; mn.normalScale = 1.0f;
    glm::mat4 md = glm::translate(glm::mat4(1.0f), glm::vec3(0.f, 1.2f, -24.f)) *
                   glm::scale(glm::mat4(1.0f), glm::vec3(2.5f));
    TestObj o; o.mesh = hCube; o.mat = mn;
    std::memcpy(o.model, glm::value_ptr(md), sizeof(float) * 16);
    testObjs.push_back(o);
  }
  std::vector<uint32_t> hModel;
  if (haveModel) for (const auto& m : model.meshes) hModel.push_back(ren->UploadMesh(m));

  stw::StwMaterial matGround;
  matGround.base[0] = 0.05f; matGround.base[1] = 0.09f; matGround.base[2] = 0.05f;
  matGround.roughness = 0.95f; matGround.metallic = 0.f;
  stw::StwMaterial matCube;
  matCube.base[0] = 0.1f; matCube.base[1] = 0.55f; matCube.base[2] = 0.16f;
  matCube.roughness = 0.45f; matCube.metallic = 0.6f;

  // ---- Game-Layer: Controller, WeaponSystem, Targets, Events (kein GL im Gameplay) ----
  stw::Camera cam;
  stw::FpsController ctrl;
  stw::WeaponSystem weap;
  stw::TargetWorld targetWorld(5);
  stw::FrameEvents events;
  stw::FpsInput input;
  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  bool fireHeld = false;
  bool running = true;
  Uint64 last = SDL_GetPerformanceCounter();
  const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

  stw::StwMaterial matTarget;
  matTarget.base[0] = 0.8f; matTarget.base[1] = 0.15f; matTarget.base[2] = 0.12f;
  matTarget.roughness = 0.5f; matTarget.metallic = 0.2f;

  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = false;
      else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
      else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_q) weap.cycle();
      else if (ev.type == SDL_MOUSEBUTTONDOWN) { fireHeld = true; SDL_SetRelativeMouseMode(SDL_TRUE); }
      else if (ev.type == SDL_MOUSEBUTTONUP) fireHeld = false;
      else if (ev.type == SDL_MOUSEMOTION && SDL_GetRelativeMouseMode()) {
        input.lookDx += ev.motion.xrel;
        input.lookDy += ev.motion.yrel;
      } else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        glViewport(0, 0, w, h);
      }
    }

    Uint64 now = SDL_GetPerformanceCounter();
    float dt = static_cast<float>((now - last) / freq);
    last = now;
    dt = std::min(dt, 0.05f);

    input.fwd = (keys[SDL_SCANCODE_W] ? 1.f : 0.f) - (keys[SDL_SCANCODE_S] ? 1.f : 0.f);
    input.strafe = (keys[SDL_SCANCODE_D] ? 1.f : 0.f) - (keys[SDL_SCANCODE_A] ? 1.f : 0.f);
    input.sprint = keys[SDL_SCANCODE_LSHIFT] != 0;
    ctrl.update(input, dt);
    input.lookDx = input.lookDy = 0.f;
    weap.update(dt);

    events.clear();
    if (auto shot = weap.tryFire(fireHeld, ctrl.eye(), ctrl.forward(), events)) {
      if (auto hit = targetWorld.hitscan(shot->origin, shot->dir, weap.spec().range)) {
        events.hits.push_back({hit->second, hit->first});
        targetWorld.applyDamage(hit->first, weap.spec().damage, events);
      }
    }
    targetWorld.update(dt, events);

    // ---- Frame ----
    ren->BeginFrame();
    cam.pos = ctrl.eye();
    cam.yaw = ctrl.yaw;
    cam.pitch = ctrl.pitch;
    glm::mat4 vp = cam.viewProj();
    ren->SetViewProj(glm::value_ptr(vp));
    ren->SetCameraPos(cam.pos.x, cam.pos.y, cam.pos.z);
    stw::Light light;
    ren->SetLight(light);

    glm::mat4 id(1.f);
    ren->Draw(hGround, matGround, glm::value_ptr(id));

    if (haveModel && !hModel.empty()) {
      for (size_t i = 0; i < hModel.size(); i++) {
        const stw::StwMaterial& mm = model.meshMat[i] >= 0 ? model.mats[model.meshMat[i]] : matCube;
        ren->Draw(hModel[i], mm, glm::value_ptr(id));
      }
    } else {
      for (auto& o : testObjs) ren->Draw(o.mesh, o.mat, o.model);
    }

    for (const auto& t : targetWorld.targets()) {
      if (!t.alive) continue;
      glm::mat4 tm = glm::translate(id, t.pos) * glm::scale(id, glm::vec3(0.8f, 1.0f, 0.8f));
      ren->Draw(hCube, matTarget, glm::value_ptr(tm));
    }

    // Renderer reagiert NUR auf Events
    for (const auto& he : events.hits) {
      glm::mat4 hm = glm::translate(id, he.pos) * glm::scale(id, glm::vec3(0.12f));
      stw::StwMaterial spark;
      spark.base[0] = 1.f; spark.base[1] = 0.8f; spark.base[2] = 0.3f;
      spark.metallic = 0.f; spark.roughness = 0.4f;
      ren->Draw(hCube, spark, glm::value_ptr(hm));
    }

  ren->EndFrame();

  }

  ren->Shutdown();
  delete ren;
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
