#pragma once
#include <glm/glm.hpp>
#include <cstdint>
namespace stw {
using TextureHandle = std::uint32_t;
struct Material {
    glm::vec4 baseColorFactor{1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    TextureHandle baseColorTexture = 0;
    TextureHandle metallicRoughnessTexture = 0;
    TextureHandle normalTexture = 0;
    bool hasBaseColorTexture = false;
    bool hasMetallicRoughnessTexture = false;
    bool hasNormalTexture = false;
};
}  // namespace stw
