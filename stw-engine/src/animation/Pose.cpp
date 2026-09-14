#include "Pose.hpp"

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

} // namespace

bool Pose::FromBindPose(const Skeleton& skeleton,
                        Pose& out,
                        std::string* error) {
    if (error) error->clear();

    Pose candidate;
    candidate.joints_.reserve(skeleton.jointCount());

    for (std::size_t index = 0; index < skeleton.jointCount(); ++index) {
        JointPose jointPose;
        jointPose.translation = skeleton.bindTranslation(index);
        jointPose.rotation = skeleton.bindRotation(index);
        jointPose.scale = skeleton.bindScale(index);

        if (!IsFinite(jointPose.translation) || !IsFinite(jointPose.rotation) ||
            !IsFinite(jointPose.scale)) {
            return Fail(error, "bind pose joint " + std::to_string(index) +
                                   " contains a non-finite transform");
        }
        if (!NormalizeQuaternion(jointPose.rotation)) {
            return Fail(error, "bind pose joint " + std::to_string(index) +
                                   " has a zero-length rotation");
        }

        candidate.joints_.push_back(jointPose);
    }

    if (candidate.joints_.size() != skeleton.jointCount()) {
        return Fail(error, "bind pose joint count does not match the skeleton");
    }

    out = std::move(candidate);
    return true;
}

bool Pose::empty() const noexcept {
    return joints_.empty();
}

std::size_t Pose::size() const noexcept {
    return joints_.size();
}

std::size_t Pose::jointCount() const noexcept {
    return joints_.size();
}

const JointPose& Pose::joint(std::size_t index) const {
    return joints_.at(index);
}

JointPose& Pose::joint(std::size_t index) {
    return joints_.at(index);
}

const std::vector<JointPose>& Pose::joints() const noexcept {
    return joints_;
}

} // namespace stw
