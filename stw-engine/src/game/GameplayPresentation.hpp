#pragma once

#include "Events.hpp"
#include "FpsController.hpp"
#include "Targets.hpp"
#include "Weapons.hpp"
#include "renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace stw {

// Client-side presentation driven exclusively by authoritative gameplay
// events/state. It owns no weapon, damage, input, or animation decisions.
class GameplayPresentation {
 public:
  void Reset() noexcept;
  void Update(float deltaSeconds, float movementAmount) noexcept;
  void OnWeaponFired(const WeaponFiredEvent& event,
                     float range,
                     std::optional<glm::vec3> hitPoint) noexcept;
  void OnWeaponSwitched() noexcept;
  void OnReloadStarted() noexcept;
  void OnBotWeaponFired(const glm::vec3& origin,
                        const glm::vec3& end) noexcept;
  void OnPlayerDamaged() noexcept;

  void SubmitFirstPersonWeapon(IRenderer& renderer,
                               std::uint32_t cubeMesh,
                               const FpsController& controller,
                               const WeaponSystem& weapon) const;
  void SubmitTransientEffects(IRenderer& renderer,
                              std::uint32_t cubeMesh) const;
  void SubmitCrosshair(IRenderer& renderer,
                       std::uint32_t cubeMesh,
                       const FpsController& controller) const;

  float muzzleIntensity() const noexcept;

 private:
  struct BotShotVisual {
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
    float remaining = 0.0f;
  };

  float elapsed_ = 0.0f;
  float movementAmount_ = 0.0f;
  float recoil_ = 0.0f;
  float muzzleRemaining_ = 0.0f;
  float tracerRemaining_ = 0.0f;
  float hitRemaining_ = 0.0f;
  float switchKick_ = 0.0f;
  float reloadKick_ = 0.0f;
  float damageRemaining_ = 0.0f;
  glm::vec3 tracerStart_{0.0f};
  glm::vec3 tracerEnd_{0.0f};
  glm::vec3 hitPoint_{0.0f};
  std::array<BotShotVisual, 3u> botShots_{};
  std::size_t nextBotShot_ = 0u;
};

void SubmitTrainingTarget(IRenderer& renderer,
                          std::uint32_t cubeMesh,
                          const Target& target);

}  // namespace stw
