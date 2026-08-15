#include "Skeleton.hpp"

#include <cmath>
#include <functional>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

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

bool IsFinite(const glm::mat4& value) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!IsFinite(value[column][row])) return false;
        }
    }
    return true;
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

glm::mat4 ComposeBindMatrix(const SkeletonJoint& joint) {
    const glm::mat4 translation = glm::translate(glm::mat4(1.0f), joint.bindTranslation);
    const glm::mat4 rotation = glm::mat4_cast(joint.bindRotation);
    const glm::mat4 scale = glm::scale(glm::mat4(1.0f), joint.bindScale);
    return translation * rotation * scale;
}

} // namespace

bool Skeleton::Build(const SkeletonBuildInput& input,
                     Skeleton& out,
                     std::string* error) {
    if (error) error->clear();

    Skeleton candidate;
    candidate.joints_ = input.joints;
    candidate.localBindMatrices_.resize(candidate.joints_.size(), glm::mat4(1.0f));
    candidate.globalBindMatrices_.resize(candidate.joints_.size(), glm::mat4(1.0f));

    const std::size_t jointCount = candidate.joints_.size();
    for (std::size_t index = 0; index < jointCount; ++index) {
        SkeletonJoint& joint = candidate.joints_[index];

        if (joint.parent < -1 ||
            (joint.parent >= 0 && static_cast<std::size_t>(joint.parent) >= jointCount)) {
            return Fail(error, "joint " + std::to_string(index) + " has an invalid parent index");
        }
        if (joint.parent >= 0 && static_cast<std::size_t>(joint.parent) == index) {
            return Fail(error, "joint " + std::to_string(index) + " cannot be its own parent");
        }
        if (!IsFinite(joint.bindTranslation) || !IsFinite(joint.bindRotation) ||
            !IsFinite(joint.bindScale) || !IsFinite(joint.inverseBindMatrix)) {
            return Fail(error, "joint " + std::to_string(index) + " contains a non-finite transform");
        }
        if (!NormalizeQuaternion(joint.bindRotation)) {
            return Fail(error, "joint " + std::to_string(index) + " has a zero-length bind rotation");
        }

        candidate.localBindMatrices_[index] = ComposeBindMatrix(joint);
        if (!IsFinite(candidate.localBindMatrices_[index])) {
            return Fail(error, "joint " + std::to_string(index) + " produces a non-finite local bind matrix");
        }
    }

    // Resolve parents recursively so the input does not need to be topologically sorted.
    // State values are: 0 = unvisited, 1 = visiting, 2 = resolved.
    std::vector<unsigned char> state(jointCount, 0);
    std::function<bool(std::size_t)> resolveGlobal = [&](std::size_t index) {
        if (state[index] == 2) return true;
        if (state[index] == 1) {
            return Fail(error, "joint hierarchy contains a cycle at joint " + std::to_string(index));
        }

        state[index] = 1;
        const int parent = candidate.joints_[index].parent;
        if (parent >= 0) {
            const std::size_t parentIndex = static_cast<std::size_t>(parent);
            if (!resolveGlobal(parentIndex)) return false;
            candidate.globalBindMatrices_[index] =
                candidate.globalBindMatrices_[parentIndex] * candidate.localBindMatrices_[index];
        } else {
            candidate.globalBindMatrices_[index] = candidate.localBindMatrices_[index];
        }

        if (!IsFinite(candidate.globalBindMatrices_[index])) {
            return Fail(error, "joint " + std::to_string(index) + " produces a non-finite global bind matrix");
        }
        state[index] = 2;
        return true;
    };

    for (std::size_t index = 0; index < jointCount; ++index) {
        if (!resolveGlobal(index)) return false;
    }

    out = std::move(candidate);
    return true;
}

bool Skeleton::empty() const noexcept {
    return joints_.empty();
}

std::size_t Skeleton::size() const noexcept {
    return joints_.size();
}

std::size_t Skeleton::jointCount() const noexcept {
    return joints_.size();
}

const SkeletonJoint& Skeleton::joint(std::size_t index) const {
    return joints_.at(index);
}

int Skeleton::parentIndex(std::size_t index) const {
    return joints_.at(index).parent;
}

const glm::vec3& Skeleton::bindTranslation(std::size_t index) const {
    return joints_.at(index).bindTranslation;
}

const glm::quat& Skeleton::bindRotation(std::size_t index) const {
    return joints_.at(index).bindRotation;
}

const glm::vec3& Skeleton::bindScale(std::size_t index) const {
    return joints_.at(index).bindScale;
}

const glm::mat4& Skeleton::inverseBindMatrix(std::size_t index) const {
    return joints_.at(index).inverseBindMatrix;
}

const glm::mat4& Skeleton::bindMatrix(std::size_t index) const {
    return localBindMatrices_.at(index);
}

const glm::mat4& Skeleton::localBindMatrix(std::size_t index) const {
    return localBindMatrices_.at(index);
}

const glm::mat4& Skeleton::globalBindMatrix(std::size_t index) const {
    return globalBindMatrices_.at(index);
}

} // namespace stw
