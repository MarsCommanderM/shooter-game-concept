#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace stw {

enum class AnimationInterpolation {
    Step,
    Linear,
};

enum class AnimationTarget {
    Translation,
    Rotation,
    Scale,
};

using AnimationVec3Values = std::vector<glm::vec3>;
using AnimationQuatValues = std::vector<glm::quat>;
using AnimationValues = std::variant<AnimationVec3Values, AnimationQuatValues>;

struct AnimationChannel {
    int targetIndex = -1;
    AnimationTarget targetPath = AnimationTarget::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<float> keyTimes;
    AnimationValues values{AnimationVec3Values{}};
};

struct AnimationClipInput {
    std::string name;
    std::vector<AnimationChannel> channels;
};

class AnimationClip {
public:
    static bool Build(const AnimationClipInput& input,
                      AnimationClip& out,
                      std::string* error = nullptr);

    const std::string& name() const noexcept;
    float duration() const noexcept;
    bool empty() const noexcept;
    std::size_t size() const noexcept;
    std::size_t channelCount() const noexcept;
    const AnimationChannel& channel(std::size_t index) const;
    const std::vector<AnimationChannel>& channels() const noexcept;

private:
    std::string name_;
    std::vector<AnimationChannel> channels_;
    float duration_ = 0.0f;
};

} // namespace stw
