#pragma once

#include "FpsController.hpp"
#include "camera.h"

namespace stw {

// Single production mapping from authoritative FPS state to the camera state
// consumed by Camera::viewProj and IRenderer::SetViewProj.
inline void SyncCameraFromFpsController(const FpsController& controller,
                                        Camera& camera) noexcept {
  camera.pos = controller.eye();
  camera.yaw = controller.yaw;
  camera.pitch = controller.pitch;
}

}  // namespace stw
