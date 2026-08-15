#include "Animator.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

bool NormalizePlaybackTime(const AnimationClip* clip,
                           bool looping,
                           double seconds,
                           float& out,
                           std::string* error) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        return Fail(error, "animator time must be finite and non-negative");
    }
    if (!clip || clip->duration() == 0.0f) {
        out = 0.0f;
        return true;
    }

    const double duration = static_cast<double>(clip->duration());
    if (!std::isfinite(duration) || duration < 0.0) {
        return Fail(error, "animation clip has an invalid duration");
    }

    if (looping) {
        const double wrapped = std::fmod(seconds, duration);
        if (!std::isfinite(wrapped)) return Fail(error, "animator time wrapping failed");
        out = static_cast<float>(wrapped);
        if (out >= clip->duration()) out = 0.0f;
    } else {
        out = static_cast<float>(std::min(seconds, duration));
    }

    if (!IsFinite(out)) return Fail(error, "animator produced a non-finite time");
    return true;
}

bool BuildGlobalMatrices(const Skeleton& skeleton,
                         const Pose& pose,
                         std::vector<glm::mat4>& out,
                         std::string* error) {
    if (pose.jointCount() != skeleton.jointCount()) {
        return Fail(error, "pose joint count does not match the skeleton");
    }

    const std::size_t jointCount = skeleton.jointCount();
    std::vector<glm::mat4> localMatrices(jointCount, glm::mat4(1.0f));
    std::vector<glm::mat4> candidate(jointCount, glm::mat4(1.0f));

    for (std::size_t index = 0; index < jointCount; ++index) {
        const JointPose& jointPose = pose.joint(index);
        glm::quat rotation = jointPose.rotation;
        if (!IsFinite(jointPose.translation) || !IsFinite(rotation) ||
            !IsFinite(jointPose.scale)) {
            return Fail(error, "pose joint " + std::to_string(index) +
                                   " contains a non-finite transform");
        }
        if (!NormalizeQuaternion(rotation)) {
            return Fail(error, "pose joint " + std::to_string(index) +
                                   " has a zero-length rotation");
        }

        localMatrices[index] =
            glm::translate(glm::mat4(1.0f), jointPose.translation) *
            glm::mat4_cast(rotation) *
            glm::scale(glm::mat4(1.0f), jointPose.scale);
        if (!IsFinite(localMatrices[index])) {
            return Fail(error, "pose joint " + std::to_string(index) +
                                   " produces a non-finite local matrix");
        }
    }

    std::vector<unsigned char> state(jointCount, 0);
    std::function<bool(std::size_t)> resolveGlobal = [&](std::size_t index) {
        if (state[index] == 2) return true;
        if (state[index] == 1) {
            return Fail(error, "skeleton hierarchy contains a cycle during evaluation");
        }

        state[index] = 1;
        const int parent = skeleton.parentIndex(index);
        if (parent < -1 ||
            (parent >= 0 && static_cast<std::size_t>(parent) >= jointCount)) {
            return Fail(error, "skeleton contains an invalid parent during evaluation");
        }

        if (parent < 0) {
            candidate[index] = localMatrices[index];
        } else {
            const std::size_t parentIndex = static_cast<std::size_t>(parent);
            if (!resolveGlobal(parentIndex)) return false;
            candidate[index] = candidate[parentIndex] * localMatrices[index];
        }

        if (!IsFinite(candidate[index])) {
            return Fail(error, "pose joint " + std::to_string(index) +
                                   " produces a non-finite global matrix");
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

bool EvaluateState(const Skeleton& skeleton,
                   const AnimationClip* clip,
                   float time,
                   Pose& outPose,
                   std::vector<glm::mat4>& outGlobals,
                   std::string* error) {
    Pose candidatePose;
    if (!Pose::FromBindPose(skeleton, candidatePose, error)) return false;
    if (clip && !clip->Sample(time, candidatePose, error)) return false;

    std::vector<glm::mat4> candidateGlobals;
    if (!BuildGlobalMatrices(skeleton, candidatePose, candidateGlobals, error)) return false;

    outPose = std::move(candidatePose);
    outGlobals = std::move(candidateGlobals);
    return true;
}

} // namespace

bool Animator::Create(const Skeleton& skeleton,
                      Animator& out,
                      std::string* error) {
    if (error) error->clear();

    Animator candidate;
    candidate.skeleton_ = &skeleton;
    if (!EvaluateState(skeleton, nullptr, 0.0f,
                       candidate.pose_, candidate.globalMatrices_, error)) {
        return false;
    }

    out = std::move(candidate);
    return true;
}

bool Animator::setClip(const AnimationClip* clip,
                       bool resetTime,
                       std::string* error) {
    if (error) error->clear();
    if (!skeleton_) return Fail(error, "animator is not initialized");

    float candidateTime = 0.0f;
    const double requestedTime = resetTime ? 0.0 : static_cast<double>(time_);
    if (!NormalizePlaybackTime(clip, looping_, requestedTime, candidateTime, error)) {
        return false;
    }

    Pose candidatePose;
    std::vector<glm::mat4> candidateGlobals;
    if (!EvaluateState(*skeleton_, clip, candidateTime,
                       candidatePose, candidateGlobals, error)) {
        return false;
    }

    clip_ = clip;
    time_ = candidateTime;
    pose_ = std::move(candidatePose);
    globalMatrices_ = std::move(candidateGlobals);
    return true;
}

void Animator::setLooping(bool looping) noexcept {
    looping_ = looping;
}

bool Animator::looping() const noexcept {
    return looping_;
}

bool Animator::setTime(float seconds, std::string* error) {
    if (error) error->clear();
    if (!skeleton_) return Fail(error, "animator is not initialized");

    float candidateTime = 0.0f;
    if (!NormalizePlaybackTime(clip_, looping_, static_cast<double>(seconds),
                               candidateTime, error)) {
        return false;
    }

    Pose candidatePose;
    std::vector<glm::mat4> candidateGlobals;
    if (!EvaluateState(*skeleton_, clip_, candidateTime,
                       candidatePose, candidateGlobals, error)) {
        return false;
    }

    time_ = candidateTime;
    pose_ = std::move(candidatePose);
    globalMatrices_ = std::move(candidateGlobals);
    return true;
}

float Animator::time() const noexcept {
    return time_;
}

bool Animator::update(float deltaSeconds, std::string* error) {
    if (error) error->clear();
    if (!skeleton_) return Fail(error, "animator is not initialized");
    if (!IsFinite(deltaSeconds) || deltaSeconds < 0.0f) {
        return Fail(error, "animator delta time must be finite and non-negative");
    }

    const double requestedTime =
        static_cast<double>(time_) + static_cast<double>(deltaSeconds);
    float candidateTime = 0.0f;
    if (!NormalizePlaybackTime(clip_, looping_, requestedTime,
                               candidateTime, error)) {
        return false;
    }

    Pose candidatePose;
    std::vector<glm::mat4> candidateGlobals;
    if (!EvaluateState(*skeleton_, clip_, candidateTime,
                       candidatePose, candidateGlobals, error)) {
        return false;
    }

    time_ = candidateTime;
    pose_ = std::move(candidatePose);
    globalMatrices_ = std::move(candidateGlobals);
    return true;
}

bool Animator::evaluate(std::string* error) {
    if (error) error->clear();
    if (!skeleton_) return Fail(error, "animator is not initialized");

    Pose candidatePose;
    std::vector<glm::mat4> candidateGlobals;
    if (!EvaluateState(*skeleton_, clip_, time_,
                       candidatePose, candidateGlobals, error)) {
        return false;
    }

    pose_ = std::move(candidatePose);
    globalMatrices_ = std::move(candidateGlobals);
    return true;
}

const Pose& Animator::pose() const noexcept {
    return pose_;
}

const std::vector<glm::mat4>& Animator::globalMatrices() const noexcept {
    return globalMatrices_;
}

} // namespace stw
