#pragma once

#include "animation/Animator.hpp"
#include "animation/SkinningPalette.hpp"
#include "assets/gltf.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace stw {

class IRenderer;

enum class ModelPlaybackState {
  BindPose,
  Playing,
  Paused,
};

// Mutable state for one draw instance of an imported model. The immutable
// model, skeleton, meshes, and animation clips remain asset-owned. Animator,
// Pose (inside Animator), playback time, and palette are unique to this object.
class RuntimeModelInstance {
 public:
  RuntimeModelInstance() = default;
  RuntimeModelInstance(const RuntimeModelInstance&) = delete;
  RuntimeModelInstance& operator=(const RuntimeModelInstance&) = delete;
  RuntimeModelInstance(RuntimeModelInstance&&) noexcept = default;
  RuntimeModelInstance& operator=(RuntimeModelInstance&&) noexcept = default;

  static bool CreateStatic(std::shared_ptr<const StwModel> asset,
                           std::size_t meshIndex,
                           RuntimeModelInstance& out,
                           std::string* error = nullptr);

  // skinnedMeshIndex addresses StwModel::skinnedMeshes. This preserves the
  // imported node -> flattened mesh -> skin association without index guesses.
  static bool CreateSkinned(std::shared_ptr<const StwModel> asset,
                            std::size_t skinnedMeshIndex,
                            RuntimeModelInstance& out,
                            std::string* error = nullptr);

  // Creates one instance for every imported skinned node and one static
  // instance for every remaining mesh. The output is unchanged on failure.
  static bool CreateDefaultSet(std::shared_ptr<const StwModel> asset,
                               std::vector<RuntimeModelInstance>& out,
                               std::string* error = nullptr);

  bool SelectAnimation(std::size_t animationIndex,
                       std::string* error = nullptr);
  bool SelectAnimationByName(std::string_view animationName,
                             std::string* error = nullptr);
  bool SelectBindPose(std::string* error = nullptr);
  bool SetAnimationTime(float seconds, std::string* error = nullptr);
  bool SetPaused(bool paused, std::string* error = nullptr);
  void SetLooping(bool looping) noexcept;

  // Advances and rebuilds the palette as one transaction. On failure the
  // previous valid Animator/Pose/time/palette are preserved.
  bool Update(float deltaSeconds, std::string* error = nullptr);

  bool SetTransform(const glm::mat4& transform,
                    std::string* error = nullptr);

  // Submits through the existing static Draw or T3-A DrawWithSkinning path.
  bool Submit(IRenderer& renderer,
              const std::vector<std::uint32_t>& meshHandles,
              const StwMaterial& fallbackMaterial,
              std::string* error = nullptr) const;

  bool valid() const noexcept;
  bool skinned() const noexcept;
  std::size_t meshIndex() const noexcept;
  std::optional<std::size_t> skinnedMeshIndex() const noexcept;
  std::optional<std::size_t> skinIndex() const noexcept;
  std::optional<std::uint32_t> sourceNodeIndex() const noexcept;
  std::optional<std::size_t> selectedAnimationIndex() const noexcept;
  std::string animationName() const;
  float animationTime() const noexcept;
  ModelPlaybackState playbackState() const noexcept;
  const Skeleton* skeleton() const noexcept;
  const Animator* animator() const noexcept;
  const SkinningPalette* palette() const noexcept;
  const glm::mat4& transform() const noexcept;
  const std::shared_ptr<const StwModel>& asset() const noexcept;

 private:
  bool BuildAndCommitPalette(Animator&& candidateAnimator,
                             std::optional<std::size_t> animationIndex,
                             ModelPlaybackState state,
                             std::string* error);

  std::shared_ptr<const StwModel> asset_;
  std::size_t meshIndex_ = 0;
  std::optional<std::size_t> skinnedMeshIndex_;
  std::optional<std::size_t> skinIndex_;
  std::optional<std::uint32_t> sourceNodeIndex_;
  std::optional<std::size_t> selectedAnimationIndex_;
  Animator animator_;
  SkinningPalette palette_;
  ModelPlaybackState playbackState_ = ModelPlaybackState::BindPose;
  glm::mat4 transform_{1.0f};
};

}  // namespace stw
