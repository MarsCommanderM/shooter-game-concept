#include "AnimationClip.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace stw {
namespace {

bool IsFinite(float value) {
    return std::isfinite(static_cast<double>(value));
}

bool IsFinite(const glm::vec3& value) {
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

bool IsFinite(const glm::quat& value) {
    return IsFinite(value.w) && IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

bool NormalizeQuaternion(glm::quat& value) {
    const double lengthSquared =
        static_cast<double>(value.w) * value.w +
        static_cast<double>(value.x) * value.x +
        static_cast<double>(value.y) * value.y +
        static_cast<double>(value.z) * value.z;
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0) return false;

    const double length = std::sqrt(lengthSquared);
    value = glm::quat(static_cast<float>(value.w / length),
                      static_cast<float>(value.x / length),
                      static_cast<float>(value.y / length),
                      static_cast<float>(value.z / length));
    return IsFinite(value);
}

bool IsKnownInterpolation(AnimationInterpolation interpolation) {
    switch (interpolation) {
        case AnimationInterpolation::Step:
        case AnimationInterpolation::Linear:
            return true;
    }
    return false;
}

bool IsKnownTarget(AnimationTarget target) {
    switch (target) {
        case AnimationTarget::Translation:
        case AnimationTarget::Rotation:
        case AnimationTarget::Scale:
            return true;
    }
    return false;
}

} // namespace

bool AnimationClip::Build(const AnimationClipInput& input,
                          AnimationClip& out,
                          std::string* error) {
    if (error) error->clear();
    if (input.channels.empty()) {
        return Fail(error, "animation clip must contain at least one channel");
    }

    AnimationClip candidate;
    candidate.name_ = input.name;
    candidate.channels_ = input.channels;

    for (std::size_t channelIndex = 0; channelIndex < candidate.channels_.size(); ++channelIndex) {
        AnimationChannel& channel = candidate.channels_[channelIndex];
        const std::string prefix = "animation channel " + std::to_string(channelIndex);

        if (channel.targetIndex < 0) {
            return Fail(error, prefix + " has a negative target index");
        }
        if (!IsKnownTarget(channel.targetPath)) {
            return Fail(error, prefix + " has an unknown target path");
        }
        if (!IsKnownInterpolation(channel.interpolation)) {
            return Fail(error, prefix + " has an unknown interpolation mode");
        }
        if (channel.keyTimes.empty()) {
            return Fail(error, prefix + " contains no keys");
        }

        float previousTime = 0.0f;
        for (std::size_t keyIndex = 0; keyIndex < channel.keyTimes.size(); ++keyIndex) {
            const float time = channel.keyTimes[keyIndex];
            if (!IsFinite(time) || time < 0.0f) {
                return Fail(error, prefix + " contains an invalid key time at index " +
                                       std::to_string(keyIndex));
            }
            if (keyIndex > 0 && time < previousTime) {
                return Fail(error, prefix + " key times are not monotonic");
            }
            previousTime = time;
        }

        if (channel.targetPath == AnimationTarget::Rotation) {
            if (!std::holds_alternative<AnimationQuatValues>(channel.values)) {
                return Fail(error, prefix + " requires quaternion values");
            }

            AnimationQuatValues& values = std::get<AnimationQuatValues>(channel.values);
            if (values.size() != channel.keyTimes.size()) {
                return Fail(error, prefix + " key/value counts do not match");
            }
            for (std::size_t keyIndex = 0; keyIndex < values.size(); ++keyIndex) {
                if (!IsFinite(values[keyIndex]) || !NormalizeQuaternion(values[keyIndex])) {
                    return Fail(error, prefix + " contains an invalid rotation at index " +
                                           std::to_string(keyIndex));
                }
            }
        } else {
            if (!std::holds_alternative<AnimationVec3Values>(channel.values)) {
                return Fail(error, prefix + " requires vec3 values");
            }

            const AnimationVec3Values& values = std::get<AnimationVec3Values>(channel.values);
            if (values.size() != channel.keyTimes.size()) {
                return Fail(error, prefix + " key/value counts do not match");
            }
            for (std::size_t keyIndex = 0; keyIndex < values.size(); ++keyIndex) {
                if (!IsFinite(values[keyIndex])) {
                    return Fail(error, prefix + " contains a non-finite value at index " +
                                           std::to_string(keyIndex));
                }
            }
        }

        candidate.duration_ = std::max(candidate.duration_, channel.keyTimes.back());
    }

    out = std::move(candidate);
    return true;
}

const std::string& AnimationClip::name() const noexcept {
    return name_;
}

float AnimationClip::duration() const noexcept {
    return duration_;
}

bool AnimationClip::empty() const noexcept {
    return channels_.empty();
}

std::size_t AnimationClip::size() const noexcept {
    return channels_.size();
}

std::size_t AnimationClip::channelCount() const noexcept {
    return channels_.size();
}

const AnimationChannel& AnimationClip::channel(std::size_t index) const {
    return channels_.at(index);
}

const std::vector<AnimationChannel>& AnimationClip::channels() const noexcept {
    return channels_;
}

} // namespace stw
