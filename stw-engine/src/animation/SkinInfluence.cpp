#include "SkinInfluence.hpp"

#include <array>
#include <cmath>
#include <utility>

namespace stw {
namespace {

constexpr double kMinimumWeightSum = 1.0e-8;

bool Fail(std::string* error, const char* message) {
    if (error) {
        *error = message;
    }
    return false;
}

} // namespace

bool SkinInfluence4::ValidateAndNormalize(SkinInfluence4& influence,
                                         std::size_t jointCount,
                                         std::string* error) {
    if (error) {
        error->clear();
    }

    SkinInfluence4 candidate = influence;
    const std::array<float, 4> inputWeights{
        candidate.weights.x,
        candidate.weights.y,
        candidate.weights.z,
        candidate.weights.w,
    };

    double weightSum = 0.0;
    for (std::size_t i = 0; i < inputWeights.size(); ++i) {
        const float weight = inputWeights[i];
        if (!std::isfinite(weight)) {
            return Fail(error, "skin influence weight must be finite");
        }
        if (weight < 0.0f) {
            return Fail(error, "skin influence weight must not be negative");
        }
        if (weight > 0.0f) {
            if (candidate.joints[i] >= jointCount) {
                return Fail(error, "positive skin influence joint is out of range");
            }
        } else {
            candidate.joints[i] = 0;
        }
        weightSum += static_cast<double>(weight);
    }

    if (!std::isfinite(weightSum)) {
        return Fail(error, "skin influence weight sum must be finite");
    }
    if (weightSum <= kMinimumWeightSum) {
        return Fail(error, "skin influence weight sum must be greater than zero");
    }

    const glm::vec4 normalized(
        static_cast<float>(static_cast<double>(inputWeights[0]) / weightSum),
        static_cast<float>(static_cast<double>(inputWeights[1]) / weightSum),
        static_cast<float>(static_cast<double>(inputWeights[2]) / weightSum),
        static_cast<float>(static_cast<double>(inputWeights[3]) / weightSum));
    if (!std::isfinite(normalized.x) ||
        !std::isfinite(normalized.y) ||
        !std::isfinite(normalized.z) ||
        !std::isfinite(normalized.w)) {
        return Fail(error, "normalized skin influence weights must be finite");
    }

    candidate.weights = normalized;
    influence = std::move(candidate);
    return true;
}

} // namespace stw
