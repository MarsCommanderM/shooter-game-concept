#pragma once
#include <glm/glm.hpp>
namespace stw {
struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    // xyz = tangent direction, w = handedness für Bitangent-Rekonstruktion
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
};
}  // namespace stw
