#pragma once
// FPS-Controller ohne jede Renderer-Abhängigkeit.
#include <glm/glm.hpp>

namespace stw {
struct FpsInput {
    float fwd = 0.f;    // -1..1
    float strafe = 0.f;
    bool sprint = false;
    float lookDx = 0.f;  // akkumuliert pro Frame
    float lookDy = 0.f;
};

class FpsController {
   public:
    glm::vec3 pos{0.f, 1.7f, 0.f};
    float yaw = 0.f;
    float pitch = 0.f;
    bool sprinting = false;

    void update(const FpsInput& in, float dt) {
        yaw -= in.lookDx * 0.0022f;
        pitch = glm::clamp(pitch - in.lookDy * 0.0022f, -1.4f, 1.4f);
        glm::vec3 f(-sinf(yaw), 0.f, -cosf(yaw));
        glm::vec3 r(cosf(yaw), 0.f, -sinf(yaw));
        glm::vec3 move = f * in.fwd + r * in.strafe;
        sprinting = in.sprint && glm::length(move) > 0.01f;
        if (glm::length(move) > 0.001f) {
            move = glm::normalize(move);
            pos += move * (sprinting ? 8.5f : 5.2f) * dt;
        }
        pos.y = 1.7f;
    }
    glm::vec3 forward() const {
        return glm::normalize(glm::vec3(-sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch)));
    }
    glm::vec3 eye() const { return pos; }
};
}  // namespace stw
