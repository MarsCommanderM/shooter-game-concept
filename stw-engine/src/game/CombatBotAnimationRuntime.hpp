#pragma once

#include "game/CharacterAnimationController.hpp"
#include "game/CombatLoop.hpp"
#include "runtime/ModelInstance.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace stw {

// Per-bot presentation state. The immutable imported asset is shared, while
// RuntimeModelInstance keeps Animator/Pose/time/palette isolated per entity.
class CombatBotAnimationRuntime {
 public:
  CombatBotAnimationRuntime() = default;
  CombatBotAnimationRuntime(const CombatBotAnimationRuntime&) = delete;
  CombatBotAnimationRuntime& operator=(const CombatBotAnimationRuntime&) =
      delete;
  CombatBotAnimationRuntime(CombatBotAnimationRuntime&&) noexcept = default;
  CombatBotAnimationRuntime& operator=(CombatBotAnimationRuntime&&) noexcept =
      default;

  static bool Create(std::shared_ptr<const StwModel> asset,
                     std::size_t skinnedMeshIndex,
                     float uniformScale,
                     const BotCombatState& bot,
                     CombatBotAnimationRuntime& out,
                     std::string* error = nullptr);

  bool Update(const BotCombatState& bot,
              float deltaSeconds,
              std::string* error = nullptr);
  bool Reset(const BotCombatState& bot, std::string* error = nullptr);

  const RuntimeModelInstance& instance() const noexcept;
  RuntimeModelInstance& instance() noexcept;
  const CharacterAnimationController& controller() const noexcept;
  bool visible() const noexcept;

 private:
  bool ApplyTransform(const BotCombatState& bot, std::string* error);

  RuntimeModelInstance instance_;
  CharacterAnimationController controller_;
  float uniformScale_ = 1.0f;
  bool visible_ = false;
};

}  // namespace stw
