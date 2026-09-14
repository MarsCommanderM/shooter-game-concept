#pragma once

#include "Skeleton.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace stw {

class SkinningPalette {
public:
    static bool Build(const Skeleton& skeleton,
                      const std::vector<glm::mat4>& animatedGlobals,
                      SkinningPalette& out,
                      std::string* error = nullptr);

    bool empty() const noexcept;
    std::size_t size() const noexcept;
    std::size_t jointCount() const noexcept;

    const glm::mat4& matrix(std::size_t index) const;
    const std::vector<glm::mat4>& matrices() const noexcept;

private:
    std::vector<glm::mat4> matrices_;
};

} // namespace stw
