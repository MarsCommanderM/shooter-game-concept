#pragma once
// STW-ENGINE: Renderer-Backend austauschbar (Bauplan-Vorgabe).
// Phase 1: OpenGL-4.5-Backend. Spaeter: Web-Backend (WebGL2) mit gleicher Schnittstelle.
#include <cstdint>

#include "gltf.h"

namespace stw {

struct Light {
  float dir[3] = {0.4f, -0.8f, 0.3f};
  float color[3] = {1.0f, 0.98f, 0.92f};
  float ambient[3] = {0.06f, 0.09f, 0.06f};
  float strength = 3.0f;
};

class IRenderer {
 public:
  virtual ~IRenderer() = default;
  virtual bool Init(void* sdlWindow) = 0;
  virtual void BeginFrame() = 0;
  virtual void SetViewProj(const float vp[16]) = 0;
  virtual void SetCameraPos(float x, float y, float z) = 0;
  virtual void SetLight(const Light& l) = 0;
  virtual uint32_t UploadMesh(const StwMesh& m) = 0;  // gibt Handle
  virtual void Draw(uint32_t mesh, const StwMaterial& mat, const float model[16]) = 0;
  virtual void EndFrame() = 0;
  virtual void Shutdown() = 0;
};

// Factory: hier spaeter Backend-Wahl (GL / Web / ...)
IRenderer* CreateGLRenderer();

}  // namespace stw
