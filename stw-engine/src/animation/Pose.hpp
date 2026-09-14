#pragma once

#include "Skeleton.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace stw {

struct JointPose {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

class Pose {
public:
    static bool FromBindPose(const Skeleton& skeleton,
                             Pose& out,
                             std::string* error = nullptr);

    bool empty() const noexcept;
    std::size_t size() const noexcept;
    std::size_t jointCount() const noexcept;

    const JointPose& joint(std::size_t index) const;
    JointPose& joint(std::size_t index);
    const std::vector<JointPose>& joints() const noexcept;

private:
    std::vector<JointPose> joints_;
};

} // namespace stw
