#include "Playtest.hpp"

#include "animation/AnimationClip.hpp"
#include "animation/Animator.hpp"
#include "animation/Skeleton.hpp"
#include "animation/SkinInfluence.hpp"
#include "animation/SkinningPalette.hpp"
#include "assets/gltf.h"
#include "render/renderer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace stw {
namespace {

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

StwMesh BuildRibbonMesh() {
  StwMesh mesh;
  constexpr int kRows = 7;
  constexpr float kHalfWidth = 0.38f;
  for (int row = 0; row < kRows; ++row) {
    const float fraction = static_cast<float>(row) /
                           static_cast<float>(kRows - 1);
    const float y = fraction * 2.4f;
    for (int side = 0; side < 2; ++side) {
      const float x = side == 0 ? -kHalfWidth : kHalfWidth;
      mesh.pos.insert(mesh.pos.end(), {x, y, 0.0f});
      mesh.nrm.insert(mesh.nrm.end(), {0.0f, 0.0f, 1.0f});
      mesh.uv.insert(mesh.uv.end(),
                     {side == 0 ? 0.0f : 1.0f, fraction * 3.0f});
      mesh.tan.insert(mesh.tan.end(), {1.0f, 0.0f, 0.0f, 1.0f});

      SkinInfluence4 influence;
      influence.joints = {0u, 1u, 0u, 0u};
      const float childWeight = glm::clamp((y - 0.55f) / 1.1f, 0.0f, 1.0f);
      influence.weights = glm::vec4(1.0f - childWeight, childWeight,
                                    0.0f, 0.0f);
      mesh.skinInfluences.push_back(influence);
    }
  }

  for (int row = 0; row < kRows - 1; ++row) {
    const std::uint32_t lowerLeft = static_cast<std::uint32_t>(row * 2);
    const std::uint32_t lowerRight = lowerLeft + 1u;
    const std::uint32_t upperLeft = lowerLeft + 2u;
    const std::uint32_t upperRight = lowerLeft + 3u;
    mesh.idx.insert(mesh.idx.end(),
                    {lowerLeft, lowerRight, upperRight,
                     lowerLeft, upperRight, upperLeft});
  }
  mesh.bmin[0] = -kHalfWidth;
  mesh.bmin[1] = 0.0f;
  mesh.bmin[2] = 0.0f;
  mesh.bmax[0] = kHalfWidth;
  mesh.bmax[1] = 2.4f;
  mesh.bmax[2] = 0.0f;
  return mesh;
}

bool BuildRig(Skeleton& skeleton,
              AnimationClip& clip,
              std::string* error) {
  SkeletonBuildInput skeletonInput;
  SkeletonJoint root;
  root.name = "root";
  root.parent = -1;
  root.inverseBindMatrix = glm::mat4(1.0f);
  skeletonInput.joints.push_back(root);

  SkeletonJoint bend;
  bend.name = "bend";
  bend.parent = 0;
  bend.bindTranslation = glm::vec3(0.0f, 1.05f, 0.0f);
  bend.inverseBindMatrix = glm::translate(
      glm::mat4(1.0f), glm::vec3(0.0f, -1.05f, 0.0f));
  skeletonInput.joints.push_back(bend);
  if (!Skeleton::Build(skeletonInput, skeleton, error)) return false;

  AnimationChannel bendChannel;
  bendChannel.targetIndex = 1;
  bendChannel.targetPath = AnimationTarget::Rotation;
  bendChannel.interpolation = AnimationInterpolation::Linear;
  bendChannel.keyTimes = {0.0f, 0.75f, 1.5f, 2.25f, 3.0f};
  bendChannel.values = AnimationQuatValues{
      glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::angleAxis(glm::radians(-58.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::angleAxis(glm::radians(58.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
  };

  AnimationClipInput clipInput;
  clipInput.name = "Procedural Bend";
  clipInput.channels.push_back(std::move(bendChannel));
  return AnimationClip::Build(clipInput, clip, error);
}

class SkinningPlaytest final : public IPlaytestScene {
 public:
  const char* name() const noexcept override { return "skinning"; }

  bool Initialize(IRenderer& renderer, std::string* error) override {
    if (error) error->clear();
    StwMesh mesh = BuildRibbonMesh();
    if (!BuildRig(skeleton_, clip_, error) ||
        !Animator::Create(skeleton_, animator_, error)) {
      return false;
    }
    animator_.setLooping(true);
    if (!animator_.setClip(&clip_, true, error) ||
        !animator_.buildSkinningPalette(palette_, error)) {
      return false;
    }
    if (!renderer.UploadMeshChecked(mesh, meshHandle_, error)) return false;

    // A small directional normal map makes tangent/normal deformation visible
    // without introducing an asset or renderer dependency into animation code.
    const std::array<unsigned char, 16> normalPixels{
        104, 128, 252, 255, 152, 128, 252, 255,
        128, 104, 252, 255, 128, 152, 252, 255,
    };
    const std::uint32_t normalTexture =
        renderer.UploadTextureRGBA(normalPixels.data(), 2, 2);
    if (normalTexture != 0u) renderer.SetNormalTexture(normalTexture);

    material_.base[0] = 0.08f;
    material_.base[1] = 0.72f;
    material_.base[2] = 0.95f;
    material_.base[3] = 1.0f;
    material_.metallic = 0.15f;
    material_.roughness = 0.42f;
    material_.hasNormal = normalTexture != 0u;
    material_.normalScale = 0.7f;
    modelMatrix_ = glm::translate(glm::mat4(1.0f),
                                  glm::vec3(0.0f, -0.25f, 0.0f));
    return true;
  }

  bool SelectAnimation(bool resetTime, std::string* error) override {
    animator_.setLooping(true);
    if (!animator_.setClip(&clip_, resetTime, error)) return false;
    return animator_.buildSkinningPalette(palette_, error);
  }

  bool SelectBindPose(std::string* error) override {
    if (!animator_.setClip(nullptr, true, error)) return false;
    return animator_.buildSkinningPalette(palette_, error);
  }

  bool Update(float deltaSeconds, std::string* error) override {
    if (!animator_.update(deltaSeconds, error)) return false;
    return animator_.buildSkinningPalette(palette_, error);
  }

  bool Draw(IRenderer& renderer, std::string* error) const override {
    if (meshHandle_ == kInvalidMeshHandle) {
      return Fail(error, "skinning playtest mesh was not uploaded");
    }
    return renderer.DrawWithSkinning(meshHandle_, material_,
                                     glm::value_ptr(modelMatrix_),
                                     &palette_, error);
  }

  const std::string& clipName() const noexcept override { return clip_.name(); }
  float animationTime() const noexcept override { return animator_.time(); }
  std::size_t jointCount() const noexcept override { return skeleton_.jointCount(); }
  std::size_t paletteSize() const noexcept override { return palette_.size(); }
  bool looping() const noexcept override { return animator_.looping(); }

 private:
  Skeleton skeleton_;
  AnimationClip clip_;
  Animator animator_;
  SkinningPalette palette_;
  std::uint32_t meshHandle_ = kInvalidMeshHandle;
  StwMaterial material_;
  glm::mat4 modelMatrix_{1.0f};
};

}  // namespace

std::unique_ptr<IPlaytestScene> CreateSkinningPlaytest() {
  return std::make_unique<SkinningPlaytest>();
}

}  // namespace stw
