#include "Playtest.hpp"

#include "animation/Animator.hpp"
#include "animation/ModelAnimationBinding.hpp"
#include "animation/SkinningPalette.hpp"
#include "assets/gltf.h"
#include "render/renderer.h"

#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef STW_PLAYTEST_ASSET_DIR
#define STW_PLAYTEST_ASSET_DIR "playtest/assets"
#endif

namespace stw {
namespace {

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

class AnimationPlaytest final : public IPlaytestScene {
 public:
  const char* name() const noexcept override { return "animation"; }

  bool Initialize(IRenderer& renderer, std::string* error) override {
    if (error) error->clear();
    const std::string fixturePath =
        std::string(STW_PLAYTEST_ASSET_DIR) + "/imported_animation.gltf";
    if (!LoadGLTF(fixturePath, model_, error)) return false;
    if (model_.skins.empty()) {
      return Fail(error, "animation fixture did not import a skin");
    }
    if (model_.animations.empty()) {
      return Fail(error, "animation fixture did not import an animation");
    }
    if (model_.skinnedMeshes.empty()) {
      return Fail(error, "animation fixture did not bind a skinned mesh");
    }

    skinIndex_ = model_.skinnedMeshes.front().skinIndex;
    meshIndex_ = model_.skinnedMeshes.front().meshIndex;
    animationIndex_ = 0u;
    if (skinIndex_ >= model_.skins.size() || meshIndex_ >= model_.meshes.size() ||
        animationIndex_ >= model_.animations.size()) {
      return Fail(error, "animation fixture produced invalid runtime indices");
    }
    const StwAnimation& animation = model_.animations[animationIndex_];
    if (animation.skinClips.size() != model_.skins.size() ||
        !animation.skinClips[skinIndex_]) {
      return Fail(error, "animation fixture has no clip for its skin");
    }

    clipName_ = animation.name.empty()
                    ? animation.skinClips[skinIndex_]->name()
                    : animation.name;
    if (clipName_.empty()) clipName_ = "Imported animation 0";
    if (!BindAnimation(model_, skinIndex_, animationIndex_, animator_, error)) {
      return false;
    }
    animator_.setLooping(true);
    if (!animator_.buildSkinningPalette(palette_, error) ||
        !renderer.UploadMeshChecked(model_.meshes[meshIndex_], meshHandle_, error)) {
      return false;
    }

    const int materialIndex =
        meshIndex_ < model_.meshMat.size() ? model_.meshMat[meshIndex_] : -1;
    if (materialIndex >= 0 &&
        static_cast<std::size_t>(materialIndex) < model_.mats.size()) {
      material_ = model_.mats[static_cast<std::size_t>(materialIndex)];
    } else {
      material_.base[0] = 0.95f;
      material_.base[1] = 0.34f;
      material_.base[2] = 0.12f;
      material_.roughness = 0.5f;
    }
    modelMatrix_ = glm::translate(glm::mat4(1.0f),
                                  glm::vec3(0.0f, -0.25f, 0.0f));
    return true;
  }

  bool SelectAnimation(bool resetTime, std::string* error) override {
    if (resetTime) {
      if (!BindAnimation(model_, skinIndex_, animationIndex_, animator_, error)) {
        return false;
      }
    } else {
      const auto& clip = model_.animations[animationIndex_].skinClips[skinIndex_];
      if (!clip || !animator_.setClip(&*clip, false, error)) return false;
    }
    animator_.setLooping(true);
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
      return Fail(error, "animation playtest mesh was not uploaded");
    }
    return renderer.DrawWithSkinning(meshHandle_, material_,
                                     glm::value_ptr(modelMatrix_),
                                     &palette_, error);
  }

  const std::string& clipName() const noexcept override { return clipName_; }
  float animationTime() const noexcept override { return animator_.time(); }
  std::size_t jointCount() const noexcept override {
    return skinIndex_ < model_.skins.size()
               ? model_.skins[skinIndex_].skeleton.jointCount()
               : 0u;
  }
  std::size_t paletteSize() const noexcept override { return palette_.size(); }
  bool looping() const noexcept override { return animator_.looping(); }

 private:
  StwModel model_;
  Animator animator_;
  SkinningPalette palette_;
  std::size_t skinIndex_ = 0u;
  std::size_t meshIndex_ = 0u;
  std::size_t animationIndex_ = 0u;
  std::uint32_t meshHandle_ = kInvalidMeshHandle;
  StwMaterial material_;
  glm::mat4 modelMatrix_{1.0f};
  std::string clipName_;
};

}  // namespace

std::unique_ptr<IPlaytestScene> CreateAnimationPlaytest() {
  return std::make_unique<AnimationPlaytest>();
}

}  // namespace stw
