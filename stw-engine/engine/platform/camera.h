#pragma once
// STW-ENGINE: Kamera (View/Projection) via GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace stw {

struct Camera {
  glm::vec3 pos{0, 1.7f, 4};
  float yaw = 0;    // rad
  float pitch = 0;  // rad
  float fovDeg = 75;
  float aspect = 16.f / 9.f;

  glm::vec3 forward() const {
    return glm::normalize(glm::vec3(-sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch)));
  }
  glm::vec3 right() const { return glm::normalize(glm::vec3(cosf(yaw), 0, -sinf(yaw))); }

  glm::mat4 viewProj() const {
    glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 500.f);
    glm::mat4 view = glm::lookAt(pos, pos + forward(), glm::vec3(0, 1, 0));
    return proj * view;
  }
};

}  // namespace stw
