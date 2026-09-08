#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace stw {

struct SkinInfluence4 {
    std::array<std::uint32_t, 4> joints{0, 0, 0, 0};
    glm::vec4 weights{1.0f, 0.0f, 0.0f, 0.0f};

    static bool ValidateAndNormalize(SkinInfluence4& influence,
                                     std::size_t jointCount,
                                     std::string* error = nullptr);
};

} // namespace stw
