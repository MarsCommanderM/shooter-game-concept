#pragma once
// STW-ENGINE: Renderer-Backend austauschbar (Bauplan-Vorgabe).
// Phase 1: OpenGL-4.5-Backend. Spaeter: Web-Backend (WebGL2) mit gleicher Schnittstelle.
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include "animation/SkinningPalette.hpp"
#include "gltf.h"
#include "render/IBL.hpp"

namespace stw {

inline constexpr std::size_t kMaxSkinJoints = 64;
inline constexpr std::uint32_t kInvalidMeshHandle =
    std::numeric_limits<std::uint32_t>::max();

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
  virtual bool UploadMeshChecked(const StwMesh& m, uint32_t& handle,
                                 std::string* error = nullptr) {
    const uint32_t candidate = UploadMesh(m);
    if (candidate == kInvalidMeshHandle) {
      if (error) *error = "mesh upload failed";
      return false;
    }
    handle = candidate;
    if (error) error->clear();
    return true;
  }
  virtual void Draw(uint32_t mesh, const StwMaterial& mat, const float model[16]) = 0;
  // Die Palette ist Draw-/Instanz-Zustand, nicht dauerhaftes Mesh-Eigentum.
  // nullptr/leer bezeichnet den statischen Pfad; skinned Meshes brauchen eine
  // nicht-leere, passende Palette und werden sonst sicher abgelehnt.
  // T3-A transformiert Normalen/Tangenten mit mat3(skin) und setzt damit
  // rigide bzw. uniform skalierte Joints voraus.
  virtual bool DrawWithSkinning(uint32_t mesh, const StwMaterial& mat,
                                const float model[16],
                                const SkinningPalette* palette,
                                std::string* error = nullptr) {
    if (palette && !palette->empty()) {
      if (error) *error = "renderer backend does not support skinning";
      return false;
    }
    Draw(mesh, mat, model);
    if (error) error->clear();
    return true;
  }
  virtual void SetIBL(const IBLResources& r) { (void)r; }
  virtual uint32_t UploadTextureRGBA(const unsigned char* px, int w, int h) { (void)px; (void)w; (void)h; return 0; }
  virtual void SetNormalTexture(uint32_t t) { (void)t; }
  // Queues at most one readback of the real renderer output. A newer request
  // replaces an older pending request so remote streaming cannot build an
  // unbounded frame backlog. The result becomes available after EndFrame().
  virtual bool RequestFrameCapture(const std::string& path,
                                   std::string* error = nullptr) {
    (void)path;
    if (error) *error = "renderer backend does not support frame capture";
    return false;
  }
  virtual bool ConsumeFrameCaptureResult(std::string* error = nullptr) {
    if (error) *error = "renderer backend has no frame capture result";
    return false;
  }
  virtual void EndFrame() = 0;
  virtual void Shutdown() = 0;
};

// Factory: hier spaeter Backend-Wahl (GL / Web / ...)
IRenderer* CreateGLRenderer();

}  // namespace stw
