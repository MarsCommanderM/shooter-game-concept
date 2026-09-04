#include "CharacterAnimationController.hpp"

#include "runtime/ModelInstance.hpp"

#include <cctype>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

namespace stw {
namespace {

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

using Aliases = std::vector<std::string_view>;

const Aliases& StateAliases(CharacterAnimationState state) {
  static const Aliases idle{"Idle"};
  static const Aliases move{"Move", "Walk", "Run"};
  static const Aliases fire{"Fire", "Shoot", "Attack"};
  switch (state) {
    case CharacterAnimationState::Idle:
      return idle;
    case CharacterAnimationState::Move:
      return move;
    case CharacterAnimationState::Fire:
      return fire;
  }
  return idle;
}

std::string Lower(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const unsigned char character : value) {
    result.push_back(static_cast<char>(std::tolower(character)));
  }
  return result;
}

std::vector<std::string> SemanticTokens(std::string_view value) {
  std::vector<std::string> tokens;
  std::string current;
  unsigned char previous = 0;
  for (const unsigned char character : value) {
    const bool alphaNumeric = std::isalnum(character) != 0;
    const bool camelBoundary = std::isupper(character) != 0 &&
                               std::islower(previous) != 0;
    if (!alphaNumeric || camelBoundary) {
      if (!current.empty()) {
        tokens.push_back(std::move(current));
        current.clear();
      }
      if (!alphaNumeric) {
        previous = character;
        continue;
      }
    }
    current.push_back(static_cast<char>(std::tolower(character)));
    previous = character;
  }
  if (!current.empty()) tokens.push_back(std::move(current));
  return tokens;
}

bool SemanticMatch(std::string_view animationName, std::string_view alias) {
  const std::string normalizedAlias = Lower(alias);
  if (Lower(animationName) == normalizedAlias) return true;
  for (const std::string& token : SemanticTokens(animationName)) {
    if (token == normalizedAlias) return true;
  }
  return false;
}

const AnimationClip* CompatibleClip(const StwAnimation& animation,
                                    std::size_t skinIndex,
                                    std::size_t skinCount) {
  if (animation.skinClips.size() != skinCount ||
      !animation.skinClips[skinIndex]) {
    return nullptr;
  }
  return &*animation.skinClips[skinIndex];
}

template <typename Match>
bool AnimationNameMatches(const StwAnimation& animation,
                          const AnimationClip& clip,
                          const Aliases& aliases,
                          Match&& match) {
  for (const std::string_view alias : aliases) {
    if ((!animation.name.empty() && match(animation.name, alias)) ||
        (!clip.name().empty() && match(clip.name(), alias))) {
      return true;
    }
  }
  return false;
}

std::optional<std::size_t> ResolveClip(
    const RuntimeModelInstance& instance,
    CharacterAnimationState state) {
  const std::shared_ptr<const StwModel>& asset = instance.asset();
  const std::optional<std::size_t> skinIndex = instance.skinIndex();
  if (!asset || !skinIndex || *skinIndex >= asset->skins.size()) {
    return std::nullopt;
  }

  const Aliases& aliases = StateAliases(state);
  // Preserve glTF source order inside each match tier: exact names first,
  // then normalized case-insensitive semantic tokens.
  for (std::size_t index = 0; index < asset->animations.size(); ++index) {
    const StwAnimation& animation = asset->animations[index];
    const AnimationClip* clip = CompatibleClip(animation, *skinIndex,
                                               asset->skins.size());
    if (clip && AnimationNameMatches(
                    animation, *clip, aliases,
                    [](std::string_view name, std::string_view alias) {
                      return name == alias;
                    })) {
      return index;
    }
  }
  for (std::size_t index = 0; index < asset->animations.size(); ++index) {
    const StwAnimation& animation = asset->animations[index];
    const AnimationClip* clip = CompatibleClip(animation, *skinIndex,
                                               asset->skins.size());
    if (clip && AnimationNameMatches(
                    animation, *clip, aliases,
                    [](std::string_view name, std::string_view alias) {
                      return SemanticMatch(name, alias);
                    })) {
      return index;
    }
  }
  return std::nullopt;
}

bool StateLoops(CharacterAnimationState state) {
  return state != CharacterAnimationState::Fire;
}

std::string MissingClipDiagnostic(CharacterAnimationState state) {
  return std::string("missing compatible ") + CharacterAnimationStateName(state) +
         " animation; retaining the previous valid clip or bind pose";
}

}  // namespace

const char* CharacterAnimationStateName(CharacterAnimationState state) noexcept {
  switch (state) {
    case CharacterAnimationState::Idle:
      return "Idle";
    case CharacterAnimationState::Move:
      return "Move";
    case CharacterAnimationState::Fire:
      return "Fire";
  }
  return "Unknown";
}

bool CharacterAnimationController::Create(
    RuntimeModelInstance& instance,
    CharacterAnimationController& out,
    std::string* error) {
  if (error) error->clear();
  if (!instance.valid() || !instance.skinned() || !instance.asset() ||
      !instance.skinIndex() || !instance.skinnedMeshIndex()) {
    return Fail(error,
                "character animation requires a valid skinned model instance");
  }

  CharacterAnimationController candidate;
  candidate.asset_ = instance.asset();
  candidate.skinIndex_ = instance.skinIndex();
  candidate.skinnedMeshIndex_ = instance.skinnedMeshIndex();
  candidate.clips_.idle = ResolveClip(instance, CharacterAnimationState::Idle);
  candidate.clips_.move = ResolveClip(instance, CharacterAnimationState::Move);
  candidate.clips_.fire = ResolveClip(instance, CharacterAnimationState::Fire);
  if (!candidate.ApplyState(CharacterAnimationState::Idle, true, instance,
                            error)) {
    return false;
  }

  out = std::move(candidate);
  return true;
}

bool CharacterAnimationController::Update(
    const CharacterAnimationInput& input,
    RuntimeModelInstance& instance,
    std::string* error) {
  if (error) error->clear();
  if (!ValidateInstance(instance, error)) return false;
  const CharacterAnimationState locomotion =
      input.moving ? CharacterAnimationState::Move
                   : CharacterAnimationState::Idle;

  if (state_ == CharacterAnimationState::Fire && clips_.fire) {
    // A new authoritative WeaponFiredEvent may restart a one-shot; a held
    // trigger without such an event never restarts it per rendered frame.
    if (input.fireTriggered) {
      return ApplyState(CharacterAnimationState::Fire, true, instance, error);
    }
    bool finished = false;
    if (!FireFinished(instance, finished, error)) return false;
    if (!finished) return true;
    return ApplyState(locomotion, false, instance, error);
  }

  if (input.fireTriggered && clips_.fire) {
    return ApplyState(CharacterAnimationState::Fire, false, instance, error);
  }

  if (!ApplyState(locomotion, false, instance, error)) return false;
  if (input.fireTriggered && !clips_.fire) {
    lastDiagnostic_ = MissingClipDiagnostic(CharacterAnimationState::Fire);
  }
  return true;
}

bool CharacterAnimationController::Reset(RuntimeModelInstance& instance,
                                         std::string* error) {
  if (error) error->clear();
  if (!ValidateInstance(instance, error)) return false;
  return ApplyState(CharacterAnimationState::Idle, true, instance, error);
}

CharacterAnimationState CharacterAnimationController::state() const noexcept {
  return state_;
}

const CharacterAnimationClips& CharacterAnimationController::clips()
    const noexcept {
  return clips_;
}

const std::string& CharacterAnimationController::lastDiagnostic()
    const noexcept {
  return lastDiagnostic_;
}

bool CharacterAnimationController::ValidateInstance(
    const RuntimeModelInstance& instance,
    std::string* error) const {
  const std::shared_ptr<const StwModel> asset = asset_.lock();
  if (!initialized_ || !asset || !instance.valid() || !instance.skinned() ||
      instance.asset().get() != asset.get() ||
      instance.skinIndex() != skinIndex_ ||
      instance.skinnedMeshIndex() != skinnedMeshIndex_) {
    return Fail(error,
                "character animation controller/model instance mismatch");
  }
  return true;
}

bool CharacterAnimationController::ApplyState(
    CharacterAnimationState desired,
    bool restart,
    RuntimeModelInstance& instance,
    std::string* error) {
  if (initialized_ && state_ == desired && !restart) return true;

  const std::optional<std::size_t> clip = ClipFor(desired);
  if (!clip) {
    state_ = desired;
    initialized_ = true;
    lastDiagnostic_ = MissingClipDiagnostic(desired);
    return true;
  }
  if (!instance.SelectAnimation(*clip, error)) return false;
  instance.SetLooping(StateLoops(desired));
  state_ = desired;
  initialized_ = true;
  lastDiagnostic_.clear();
  return true;
}

bool CharacterAnimationController::FireFinished(
    const RuntimeModelInstance& instance,
    bool& finished,
    std::string* error) const {
  if (!clips_.fire || instance.selectedAnimationIndex() != clips_.fire ||
      instance.looping()) {
    return Fail(error,
                "character Fire state does not match its non-looping clip");
  }
  const float duration = instance.animationDuration();
  const float time = instance.animationTime();
  if (!std::isfinite(static_cast<double>(duration)) || duration < 0.0f ||
      !std::isfinite(static_cast<double>(time)) || time < 0.0f) {
    return Fail(error, "character Fire animation time is invalid");
  }
  finished = duration <= 0.0f || time + 1.0e-5f >= duration;
  return true;
}

std::optional<std::size_t> CharacterAnimationController::ClipFor(
    CharacterAnimationState state) const {
  switch (state) {
    case CharacterAnimationState::Idle:
      return clips_.idle;
    case CharacterAnimationState::Move:
      return clips_.move;
    case CharacterAnimationState::Fire:
      return clips_.fire;
  }
  return std::nullopt;
}

}  // namespace stw
