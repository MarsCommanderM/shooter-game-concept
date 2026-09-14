#pragma once

#include <cstdint>
#include <type_traits>

#include <glm/glm.hpp>

namespace stw {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    // xyz = tangent direction, w = handedness für Bitangent-Rekonstruktion
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
};

// Festes GPU-Interleave fuer statische und skinned Meshes. Die POD-Felder
// halten offsetof/Stride unabhaengig von optionalem GLM-Alignment stabil.
struct GpuVertex {
    float position[3]{0.0f, 0.0f, 0.0f};
    float normal[3]{0.0f, 1.0f, 0.0f};
    float texCoord[2]{0.0f, 0.0f};
    float tangent[4]{1.0f, 0.0f, 0.0f, 1.0f};
    std::uint32_t joints[4]{0, 0, 0, 0};
    float weights[4]{1.0f, 0.0f, 0.0f, 0.0f};
};

static_assert(std::is_standard_layout<GpuVertex>::value,
              "GpuVertex must remain a standard-layout type");
static_assert(sizeof(GpuVertex) == 80,
              "GpuVertex layout changed; update OpenGL attribute offsets");

}  // namespace stw
