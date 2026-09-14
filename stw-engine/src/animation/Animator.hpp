#pragma once

#include "AnimationClip.hpp"
#include "Pose.hpp"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace stw {

class SkinningPalette;

class Animator {
public:
    static bool Create(const Skeleton& skeleton,
                       Animator& out,
                       std::string* error = nullptr);

    bool setClip(const AnimationClip* clip,
                 bool resetTime = true,
                 std::string* error = nullptr);

    void setLooping(bool looping) noexcept;
    bool looping() const noexcept;

    bool setTime(float seconds, std::string* error = nullptr);
    float time() const noexcept;

    bool update(float deltaSeconds, std::string* error = nullptr);
    bool evaluate(std::string* error = nullptr);
    bool buildSkinningPalette(SkinningPalette& out,
                              std::string* error = nullptr) const;

    const Pose& pose() const noexcept;
    const std::vector<glm::mat4>& globalMatrices() const noexcept;

private:
    const Skeleton* skeleton_ = nullptr;
    const AnimationClip* clip_ = nullptr;
    float time_ = 0.0f;
    bool looping_ = false;
    Pose pose_;
    std::vector<glm::mat4> globalMatrices_;
};

} // namespace stw
