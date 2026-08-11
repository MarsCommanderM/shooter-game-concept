// STW-ENGINE Phase 1: Fenster + Game-Loop + Input + glTF + Renderer-Backend
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstring>

#include "camera.h"
#include "gltf.h"
#include "renderer.h"

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
  if (!ren->Init(win)) {
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

  stw::StwMesh cube = stw::MakeCubeMesh();
  stw::StwMesh ground = stw::MakeGroundMesh(24.f);
  uint32_t hGround = ren->UploadMesh(ground);
  uint32_t hCube = ren->UploadMesh(cube);
  std::vector<uint32_t> hModel;
  if (haveModel) for (const auto& m : model.meshes) hModel.push_back(ren->UploadMesh(m));

  stw::StwMaterial matGround;
  matGround.base[0] = 0.05f; matGround.base[1] = 0.09f; matGround.base[2] = 0.05f;
  matGround.roughness = 0.95f; matGround.metallic = 0.f;
  stw::StwMaterial matCube;
  matCube.base[0] = 0.1f; matCube.base[1] = 0.55f; matCube.base[2] = 0.16f;
  matCube.roughness = 0.45f; matCube.metallic = 0.6f;

  // ---- Kamera + Input ----
  stw::Camera cam;
  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  bool running = true;
  Uint64 last = SDL_GetPerformanceCounter();
  const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = false;
      else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
      else if (ev.type == SDL_MOUSEBUTTONDOWN) {
        SDL_SetRelativeMouseMode(SDL_TRUE);  // Pointer-Lock wie im Bauplan
      }
      else if (ev.type == SDL_MOUSEMOTION && SDL_GetRelativeMouseMode()) {
        cam.yaw -= ev.motion.xrel * 0.0022f;
        cam.pitch = std::max(-1.4f, std::min(1.4f, cam.pitch - ev.motion.yrel * 0.0022f));
      }
      else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        cam.aspect = static_cast<float>(w) / std::max(1, h);
        glViewport(0, 0, w, h);
      }
    }

    Uint64 now = SDL_GetPerformanceCounter();
    float dt = static_cast<float>((now - last) / freq);
    last = now;
    dt = std::min(dt, 0.05f);

    // ---- Bewegung: WASD relativ zur Blickrichtung (identisch zum Prototyp) ----
    glm::vec3 f = cam.forward();
    f.y = 0; f = glm::normalize(f);
    glm::vec3 r = cam.right();
    glm::vec3 move{0};
    if (keys[SDL_SCANCODE_W]) move += f;
    if (keys[SDL_SCANCODE_S]) move -= f;
    if (keys[SDL_SCANCODE_D]) move += r;
    if (keys[SDL_SCANCODE_A]) move -= r;
    if (glm::length(move) > 0.001f) {
      move = glm::normalize(move);
      float sp = keys[SDL_SCANCODE_LSHIFT] ? 8.5f : 5.2f;
      cam.pos += move * sp * dt;
    }
    cam.pos.y = 1.7f;

    // ---- Frame ----
    ren->BeginFrame();
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
      // Test-Szene: drei Boxen mit unterschiedlichem Metallic/Roughness (PBR-Check)
      for (int i = 0; i < 3; i++) {
        stw::StwMaterial m = matCube;
        m.metallic = i * 0.5f;
        m.roughness = 0.2f + i * 0.3f;
        glm::mat4 t = glm::translate(id, glm::vec3((i - 1) * 2.f, 0.5f, -2.f));
        ren->Draw(hCube, m, glm::value_ptr(t));
      }
    }

    ren->EndFrame();
  }

  ren->Shutdown();
  delete ren;
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
