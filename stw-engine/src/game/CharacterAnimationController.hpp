#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace stw {

class RuntimeModelInstance;
struct StwModel;

enum class CharacterAnimationState {
  Idle,
  Move,
  Fire,
};

const char* CharacterAnimationStateName(CharacterAnimationState state) noexcept;

struct CharacterAnimationInput {
  bool moving = false;
  // True only when gameplay actually emitted a weapon fire event.
  bool fireTriggered = false;
};

struct CharacterAnimationClips {
  std::optional<std::size_t> idle;
  std::optional<std::size_t> move;
  std::optional<std::size_t> fire;
};

// Maps authoritative gameplay state onto one existing RuntimeModelInstance.
// It owns no Animator, Pose, palette, or animation clock.
class CharacterAnimationController {
 public:
  static bool Create(RuntimeModelInstance& instance,
                     CharacterAnimationController& out,
                     std::string* error = nullptr);

  bool Update(const CharacterAnimationInput& input,
              RuntimeModelInstance& instance,
              std::string* error = nullptr);
  bool Reset(RuntimeModelInstance& instance,
             std::string* error = nullptr);

  CharacterAnimationState state() const noexcept;
  const CharacterAnimationClips& clips() const noexcept;
  const std::string& lastDiagnostic() const noexcept;

 private:
  bool ValidateInstance(const RuntimeModelInstance& instance,
                        std::string* error) const;
  bool ApplyState(CharacterAnimationState desired,
                  bool restart,
                  RuntimeModelInstance& instance,
                  std::string* error);
  bool FireFinished(const RuntimeModelInstance& instance,
                    bool& finished,
                    std::string* error) const;
  std::optional<std::size_t> ClipFor(CharacterAnimationState state) const;

  std::weak_ptr<const StwModel> asset_;
  std::optional<std::size_t> skinIndex_;
  std::optional<std::size_t> skinnedMeshIndex_;
  CharacterAnimationClips clips_;
  CharacterAnimationState state_ = CharacterAnimationState::Idle;
  bool initialized_ = false;
  std::string lastDiagnostic_;
};

}  // namespace stw
