#include "ModelAnimationBinding.hpp"

#include "Animator.hpp"
#include "assets/gltf.h"

#include <utility>

namespace stw {
namespace {

bool Fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

} // namespace

bool BindAnimation(const StwModel& model,
                   std::size_t skinIndex,
                   std::size_t animationIndex,
                   Animator& animator,
                   std::string* error) {
    if (error) error->clear();
    if (skinIndex >= model.skins.size()) {
        return Fail(error, "animation binding skin index is out of range");
    }
    if (animationIndex >= model.animations.size()) {
        return Fail(error, "animation binding animation index is out of range");
    }

    const StwAnimation& animation = model.animations[animationIndex];
    if (animation.skinClips.size() != model.skins.size()) {
        return Fail(error, "animation skin clip map does not match the model skins");
    }
    const std::optional<AnimationClip>& clip = animation.skinClips[skinIndex];
    if (!clip) {
        return Fail(error, "animation contains no channels for the selected skin");
    }

    Animator candidate;
    if (!Animator::Create(model.skins[skinIndex].skeleton, candidate, error) ||
        !candidate.setClip(&*clip, true, error)) {
        return false;
    }

    animator = std::move(candidate);
    return true;
}

} // namespace stw
