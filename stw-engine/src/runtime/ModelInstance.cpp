#include "ModelInstance.hpp"

#include "animation/ModelAnimationBinding.hpp"
#include "render/renderer.h"

#include <cmath>
#include <utility>

#include <glm/gtc/type_ptr.hpp>

namespace stw {
namespace {

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool IsFinite(const glm::mat4& matrix) {
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      if (!std::isfinite(static_cast<double>(matrix[column][row]))) {
        return false;
      }
    }
  }
  return true;
}

bool ValidateSkinnedMesh(const StwModel& model,
                         const StwSkinnedMesh& binding,
                         std::string* error) {
  if (binding.meshIndex >= model.meshes.size()) {
    return Fail(error, "skinned model instance mesh index is out of range");
  }
  if (binding.skinIndex >= model.skins.size()) {
    return Fail(error, "skinned model instance skin index is out of range");
  }

  const StwMesh& mesh = model.meshes[binding.meshIndex];
  const Skeleton& skeleton = model.skins[binding.skinIndex].skeleton;
  if (skeleton.empty()) {
    return Fail(error, "skinned model instance skeleton is empty");
  }
  if (mesh.pos.size() % 3u != 0u ||
      mesh.skinInfluences.size() != mesh.pos.size() / 3u) {
    return Fail(error,
                "skinned model instance influence count does not match vertices");
  }

  for (const SkinInfluence4& influence : mesh.skinInfluences) {
    SkinInfluence4 candidate = influence;
    if (!SkinInfluence4::ValidateAndNormalize(candidate,
                                               skeleton.jointCount(), error)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool RuntimeModelInstance::CreateStatic(std::shared_ptr<const StwModel> asset,
                                        std::size_t meshIndex,
                                        RuntimeModelInstance& out,
                                        std::string* error) {
  if (error) error->clear();
  if (!asset) return Fail(error, "model instance asset must not be null");
  if (meshIndex >= asset->meshes.size()) {
    return Fail(error, "static model instance mesh index is out of range");
  }
  if (!asset->meshes[meshIndex].skinInfluences.empty()) {
    return Fail(error,
                "skin-influenced mesh cannot use the static model path");
  }

  RuntimeModelInstance candidate;
  candidate.asset_ = std::move(asset);
  candidate.meshIndex_ = meshIndex;
  out = std::move(candidate);
  return true;
}

bool RuntimeModelInstance::CreateSkinned(std::shared_ptr<const StwModel> asset,
                                         std::size_t skinnedMeshIndex,
                                         RuntimeModelInstance& out,
                                         std::string* error) {
  if (error) error->clear();
  if (!asset) return Fail(error, "model instance asset must not be null");
  if (skinnedMeshIndex >= asset->skinnedMeshes.size()) {
    return Fail(error, "skinned model binding index is out of range");
  }

  const StwSkinnedMesh& binding = asset->skinnedMeshes[skinnedMeshIndex];
  if (!ValidateSkinnedMesh(*asset, binding, error)) return false;
  for (const StwAnimation& animation : asset->animations) {
    if (animation.skinClips.size() != asset->skins.size()) {
      return Fail(error,
                  "animation skin clip map does not match the model skins");
    }
  }

  RuntimeModelInstance candidate;
  candidate.asset_ = std::move(asset);
  candidate.meshIndex_ = binding.meshIndex;
  candidate.skinnedMeshIndex_ = skinnedMeshIndex;
  candidate.skinIndex_ = binding.skinIndex;
  candidate.sourceNodeIndex_ = binding.nodeIndex;

  Animator bindAnimator;
  if (!Animator::Create(candidate.asset_->skins[binding.skinIndex].skeleton,
                        bindAnimator, error)) {
    return false;
  }
  bindAnimator.setLooping(true);
  if (!candidate.BuildAndCommitPalette(std::move(bindAnimator), std::nullopt,
                                       ModelPlaybackState::BindPose, error)) {
    return false;
  }

  // glTF source order is retained in StwModel::animations.
  for (std::size_t index = 0; index < candidate.asset_->animations.size();
       ++index) {
    const StwAnimation& animation = candidate.asset_->animations[index];
    if (animation.skinClips[binding.skinIndex]) {
      if (!candidate.SelectAnimation(index, error)) return false;
      break;
    }
  }

  out = std::move(candidate);
  return true;
}

bool RuntimeModelInstance::CreateDefaultSet(
    std::shared_ptr<const StwModel> asset,
    std::vector<RuntimeModelInstance>& out,
    std::string* error) {
  if (error) error->clear();
  if (!asset) return Fail(error, "model instance asset must not be null");
  if (asset->meshes.empty()) return Fail(error, "model asset contains no meshes");

  std::vector<RuntimeModelInstance> candidate;
  candidate.reserve(asset->meshes.size() + asset->skinnedMeshes.size());
  std::vector<bool> hasSkinnedBinding(asset->meshes.size(), false);

  for (std::size_t index = 0; index < asset->skinnedMeshes.size(); ++index) {
    RuntimeModelInstance instance;
    if (!CreateSkinned(asset, index, instance, error)) return false;
    hasSkinnedBinding[instance.meshIndex()] = true;
    candidate.push_back(std::move(instance));
  }

  for (std::size_t meshIndex = 0; meshIndex < asset->meshes.size();
       ++meshIndex) {
    if (hasSkinnedBinding[meshIndex]) continue;
    if (!asset->meshes[meshIndex].skinInfluences.empty()) {
      return Fail(error,
                  "skin-influenced mesh has no imported node/skin binding");
    }
    RuntimeModelInstance instance;
    if (!CreateStatic(asset, meshIndex, instance, error)) return false;
    candidate.push_back(std::move(instance));
  }

  out = std::move(candidate);
  return true;
}

bool RuntimeModelInstance::SelectAnimation(std::size_t animationIndex,
                                           std::string* error) {
  if (error) error->clear();
  if (!asset_ || !skinIndex_) {
    return Fail(error, "static model instance cannot select an animation");
  }

  Animator candidate;
  if (!BindAnimation(*asset_, *skinIndex_, animationIndex, candidate, error)) {
    return false;
  }
  candidate.setLooping(animator_.looping());
  return BuildAndCommitPalette(std::move(candidate), animationIndex,
                               ModelPlaybackState::Playing, error);
}

bool RuntimeModelInstance::SelectAnimationByName(
    std::string_view animationName,
    std::string* error) {
  if (error) error->clear();
  if (!asset_ || !skinIndex_) {
    return Fail(error, "static model instance cannot select an animation");
  }

  for (std::size_t index = 0; index < asset_->animations.size(); ++index) {
    const StwAnimation& animation = asset_->animations[index];
    if (animation.skinClips.size() != asset_->skins.size() ||
        !animation.skinClips[*skinIndex_]) {
      continue;
    }
    const std::string& clipName = animation.skinClips[*skinIndex_]->name();
    if (std::string_view(animation.name) == animationName ||
        std::string_view(clipName) == animationName) {
      return SelectAnimation(index, error);
    }
  }
  return Fail(error,
              "no compatible animation with the requested name exists");
}

bool RuntimeModelInstance::SelectBindPose(std::string* error) {
  if (error) error->clear();
  if (!asset_ || !skinIndex_) {
    return Fail(error, "static model instance has no bind pose");
  }

  Animator candidate;
  if (!Animator::Create(asset_->skins[*skinIndex_].skeleton, candidate, error)) {
    return false;
  }
  candidate.setLooping(animator_.looping());
  return BuildAndCommitPalette(std::move(candidate), std::nullopt,
                               ModelPlaybackState::BindPose, error);
}

bool RuntimeModelInstance::SetAnimationTime(float seconds,
                                            std::string* error) {
  if (error) error->clear();
  if (!asset_ || !skinIndex_) {
    return Fail(error, "static model instance has no animation time");
  }
  if (!selectedAnimationIndex_) {
    return Fail(error, "model instance has no selected animation");
  }

  Animator candidate = animator_;
  if (!candidate.setTime(seconds, error)) return false;
  return BuildAndCommitPalette(std::move(candidate), selectedAnimationIndex_,
                               playbackState_, error);
}

bool RuntimeModelInstance::SetPaused(bool paused, std::string* error) {
  if (error) error->clear();
  if (!selectedAnimationIndex_) {
    return Fail(error, "model instance has no selected animation");
  }
  playbackState_ = paused ? ModelPlaybackState::Paused
                          : ModelPlaybackState::Playing;
  return true;
}

void RuntimeModelInstance::SetLooping(bool looping) noexcept {
  if (skinIndex_) animator_.setLooping(looping);
}

bool RuntimeModelInstance::Update(float deltaSeconds, std::string* error) {
  if (error) error->clear();
  if (!std::isfinite(static_cast<double>(deltaSeconds)) ||
      deltaSeconds < 0.0f) {
    return Fail(error,
                "model instance delta time must be finite and non-negative");
  }
  if (!skinIndex_ || playbackState_ != ModelPlaybackState::Playing) {
    return true;
  }

  Animator candidate = animator_;
  if (!candidate.update(deltaSeconds, error)) return false;
  return BuildAndCommitPalette(std::move(candidate), selectedAnimationIndex_,
                               playbackState_, error);
}

bool RuntimeModelInstance::SetTransform(const glm::mat4& transform,
                                        std::string* error) {
  if (error) error->clear();
  if (!IsFinite(transform)) {
    return Fail(error, "model instance transform must be finite");
  }
  transform_ = transform;
  return true;
}

bool RuntimeModelInstance::Submit(
    IRenderer& renderer,
    const std::vector<std::uint32_t>& meshHandles,
    const StwMaterial& fallbackMaterial,
    std::string* error) const {
  if (error) error->clear();
  if (!asset_) return Fail(error, "model instance is not initialized");
  if (meshIndex_ >= asset_->meshes.size() || meshIndex_ >= meshHandles.size()) {
    return Fail(error, "model instance mesh handle mapping is out of range");
  }
  if (meshHandles[meshIndex_] == kInvalidMeshHandle) {
    return Fail(error, "model instance mesh was not uploaded");
  }

  const int materialIndex =
      meshIndex_ < asset_->meshMat.size() ? asset_->meshMat[meshIndex_] : -1;
  const StwMaterial* material = &fallbackMaterial;
  if (materialIndex >= 0) {
    if (static_cast<std::size_t>(materialIndex) >= asset_->mats.size()) {
      return Fail(error, "model instance material index is out of range");
    }
    material = &asset_->mats[static_cast<std::size_t>(materialIndex)];
  }

  if (!skinIndex_) {
    renderer.Draw(meshHandles[meshIndex_], *material,
                  glm::value_ptr(transform_));
    return true;
  }

  const Skeleton& activeSkeleton = asset_->skins[*skinIndex_].skeleton;
  if (palette_.empty() || palette_.jointCount() != activeSkeleton.jointCount()) {
    return Fail(error,
                "model instance palette does not match its active skeleton");
  }
  if (palette_.jointCount() > kMaxSkinJoints) {
    return Fail(error, "model instance palette exceeds the GPU joint limit");
  }
  return renderer.DrawWithSkinning(meshHandles[meshIndex_], *material,
                                   glm::value_ptr(transform_), &palette_, error);
}

bool RuntimeModelInstance::valid() const noexcept { return asset_ != nullptr; }

bool RuntimeModelInstance::skinned() const noexcept { return skinIndex_.has_value(); }

std::size_t RuntimeModelInstance::meshIndex() const noexcept { return meshIndex_; }

std::optional<std::size_t> RuntimeModelInstance::skinnedMeshIndex() const noexcept {
  return skinnedMeshIndex_;
}

std::optional<std::size_t> RuntimeModelInstance::skinIndex() const noexcept {
  return skinIndex_;
}

std::optional<std::uint32_t> RuntimeModelInstance::sourceNodeIndex() const noexcept {
  return sourceNodeIndex_;
}

std::optional<std::size_t>
RuntimeModelInstance::selectedAnimationIndex() const noexcept {
  return selectedAnimationIndex_;
}

std::string RuntimeModelInstance::animationName() const {
  if (!asset_ || !skinIndex_ || !selectedAnimationIndex_ ||
      *selectedAnimationIndex_ >= asset_->animations.size()) {
    return {};
  }
  const StwAnimation& animation = asset_->animations[*selectedAnimationIndex_];
  if (!animation.name.empty()) return animation.name;
  if (animation.skinClips.size() == asset_->skins.size() &&
      animation.skinClips[*skinIndex_]) {
    return animation.skinClips[*skinIndex_]->name();
  }
  return {};
}

float RuntimeModelInstance::animationTime() const noexcept {
  return skinIndex_ ? animator_.time() : 0.0f;
}

ModelPlaybackState RuntimeModelInstance::playbackState() const noexcept {
  return playbackState_;
}

const Skeleton* RuntimeModelInstance::skeleton() const noexcept {
  if (!asset_ || !skinIndex_ || *skinIndex_ >= asset_->skins.size()) {
    return nullptr;
  }
  return &asset_->skins[*skinIndex_].skeleton;
}

const Animator* RuntimeModelInstance::animator() const noexcept {
  return skinIndex_ ? &animator_ : nullptr;
}

const SkinningPalette* RuntimeModelInstance::palette() const noexcept {
  return skinIndex_ ? &palette_ : nullptr;
}

const glm::mat4& RuntimeModelInstance::transform() const noexcept {
  return transform_;
}

const std::shared_ptr<const StwModel>& RuntimeModelInstance::asset() const noexcept {
  return asset_;
}

bool RuntimeModelInstance::BuildAndCommitPalette(
    Animator&& candidateAnimator,
    std::optional<std::size_t> animationIndex,
    ModelPlaybackState state,
    std::string* error) {
  if (!asset_ || !skinIndex_ || *skinIndex_ >= asset_->skins.size()) {
    return Fail(error, "model instance has no valid active skin");
  }

  SkinningPalette candidatePalette;
  if (!candidateAnimator.buildSkinningPalette(candidatePalette, error)) {
    return false;
  }
  const std::size_t jointCount = asset_->skins[*skinIndex_].skeleton.jointCount();
  if (candidatePalette.empty() || candidatePalette.jointCount() != jointCount) {
    return Fail(error,
                "model instance palette does not match its active skeleton");
  }

  animator_ = std::move(candidateAnimator);
  palette_ = std::move(candidatePalette);
  selectedAnimationIndex_ = animationIndex;
  playbackState_ = state;
  return true;
}

}  // namespace stw
