#include "SkinningPalette.hpp"

#include <cmath>
#include <utility>

namespace stw {
namespace {

bool Fail(std::string* error, const char* message) {
    if (error) {
        *error = message;
    }
    return false;
}

bool IsFinite(const glm::mat4& matrix) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

bool SkinningPalette::Build(const Skeleton& skeleton,
                            const std::vector<glm::mat4>& animatedGlobals,
                            SkinningPalette& out,
                            std::string* error) {
    if (error) {
        error->clear();
    }
    if (animatedGlobals.size() != skeleton.jointCount()) {
        return Fail(error, "animated global matrix count must match skeleton joint count");
    }

    SkinningPalette candidate;
    candidate.matrices_.reserve(animatedGlobals.size());
    for (std::size_t jointIndex = 0; jointIndex < animatedGlobals.size(); ++jointIndex) {
        const glm::mat4& animatedGlobal = animatedGlobals[jointIndex];
        const glm::mat4& inverseBind = skeleton.inverseBindMatrix(jointIndex);
        if (!IsFinite(animatedGlobal)) {
            return Fail(error, "animated global matrix must be finite");
        }
        if (!IsFinite(inverseBind)) {
            return Fail(error, "inverse bind matrix must be finite");
        }

        const glm::mat4 paletteMatrix = animatedGlobal * inverseBind;
        if (!IsFinite(paletteMatrix)) {
            return Fail(error, "skinning palette matrix must be finite");
        }
        candidate.matrices_.push_back(paletteMatrix);
    }

    out = std::move(candidate);
    return true;
}

bool SkinningPalette::empty() const noexcept {
    return matrices_.empty();
}

std::size_t SkinningPalette::size() const noexcept {
    return matrices_.size();
}

std::size_t SkinningPalette::jointCount() const noexcept {
    return matrices_.size();
}

const glm::mat4& SkinningPalette::matrix(std::size_t index) const {
    return matrices_.at(index);
}

const std::vector<glm::mat4>& SkinningPalette::matrices() const noexcept {
    return matrices_;
}

} // namespace stw
