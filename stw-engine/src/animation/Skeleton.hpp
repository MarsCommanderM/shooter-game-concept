#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace stw {

struct SkeletonJoint {
    std::string name;
    int parent = -1;
    glm::vec3 bindTranslation{0.0f};
    glm::quat bindRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 bindScale{1.0f};
    glm::mat4 inverseBindMatrix{1.0f};
};

struct SkeletonBuildInput {
    std::vector<SkeletonJoint> joints;
};

class Skeleton {
public:
    static bool Build(const SkeletonBuildInput& input,
                      Skeleton& out,
                      std::string* error = nullptr);

    bool empty() const noexcept;
    std::size_t size() const noexcept;
    std::size_t jointCount() const noexcept;

    const SkeletonJoint& joint(std::size_t index) const;
    int parentIndex(std::size_t index) const;
    const glm::vec3& bindTranslation(std::size_t index) const;
    const glm::quat& bindRotation(std::size_t index) const;
    const glm::vec3& bindScale(std::size_t index) const;
    const glm::mat4& inverseBindMatrix(std::size_t index) const;
    const glm::mat4& bindMatrix(std::size_t index) const;
    const glm::mat4& localBindMatrix(std::size_t index) const;
    const glm::mat4& globalBindMatrix(std::size_t index) const;

private:
    std::vector<SkeletonJoint> joints_;
    std::vector<glm::mat4> localBindMatrices_;
    std::vector<glm::mat4> globalBindMatrices_;
};

} // namespace stw
