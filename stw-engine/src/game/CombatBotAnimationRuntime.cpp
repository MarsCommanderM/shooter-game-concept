#include "CombatBotAnimationRuntime.hpp"

#include <cmath>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

namespace stw {
namespace {

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool IsFinite(const glm::vec3& value) {
  return std::isfinite(static_cast<double>(value.x)) &&
      std::isfinite(static_cast<double>(value.y)) &&
      std::isfinite(static_cast<double>(value.z));
}

}  // namespace

bool CombatBotAnimationRuntime::Create(
    std::shared_ptr<const StwModel> asset,
    std::size_t skinnedMeshIndex,
    float uniformScale,
    const BotCombatState& bot,
    CombatBotAnimationRuntime& out,
    std::string* error) {
  if (error) error->clear();
  if (!std::isfinite(static_cast<double>(uniformScale)) ||
      uniformScale <= 0.0f) {
    return Fail(error, "combat bot model scale must be positive and finite");
  }

  CombatBotAnimationRuntime candidate;
  if (!RuntimeModelInstance::CreateSkinned(
          std::move(asset), skinnedMeshIndex, candidate.instance_, error) ||
      !CharacterAnimationController::Create(
          candidate.instance_, candidate.controller_, error)) {
    return false;
  }
  candidate.uniformScale_ = uniformScale;
  candidate.visible_ = bot.alive;
  if (!candidate.ApplyTransform(bot, error)) return false;
  out = std::move(candidate);
  return true;
}

bool CombatBotAnimationRuntime::Update(const BotCombatState& bot,
                                       float deltaSeconds,
                                       std::string* error) {
  if (error) error->clear();
  if (!std::isfinite(static_cast<double>(deltaSeconds)) ||
      deltaSeconds < 0.0f || !IsFinite(bot.position) ||
      !IsFinite(bot.forward) || !IsFinite(bot.velocity)) {
    return Fail(error,
                "combat bot animation input must be finite and non-negative");
  }

  if (!bot.alive) {
    visible_ = false;
    return true;
  }
  if ((!visible_ || bot.respawnedThisFrame) &&
      !controller_.Reset(instance_, error)) {
    return false;
  }

  CharacterAnimationInput input;
  input.moving = bot.moving();
  input.fireTriggered = bot.firedThisFrame;
  if (!controller_.Update(input, instance_, error) ||
      !instance_.Update(deltaSeconds, error) ||
      !ApplyTransform(bot, error)) {
    return false;
  }
  visible_ = true;
  return true;
}

bool CombatBotAnimationRuntime::Reset(const BotCombatState& bot,
                                      std::string* error) {
  if (error) error->clear();
  if (!bot.alive) {
    visible_ = false;
    return ApplyTransform(bot, error);
  }
  if (!controller_.Reset(instance_, error) || !ApplyTransform(bot, error)) {
    return false;
  }
  visible_ = true;
  return true;
}

bool CombatBotAnimationRuntime::ApplyTransform(const BotCombatState& bot,
                                               std::string* error) {
  if (!IsFinite(bot.position) || !IsFinite(bot.forward)) {
    return Fail(error, "combat bot transform input is not finite");
  }
  glm::vec3 forward(bot.forward.x, 0.0f, bot.forward.z);
  if (glm::length(forward) <= 1.0e-5f) {
    forward = glm::vec3(0.0f, 0.0f, 1.0f);
  } else {
    forward = glm::normalize(forward);
  }
  const float yaw = std::atan2(forward.x, forward.z);
  const glm::mat4 transform =
      glm::translate(glm::mat4(1.0f), bot.position) *
      glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(uniformScale_));
  return instance_.SetTransform(transform, error);
}

const RuntimeModelInstance& CombatBotAnimationRuntime::instance()
    const noexcept {
  return instance_;
}

RuntimeModelInstance& CombatBotAnimationRuntime::instance() noexcept {
  return instance_;
}

const CharacterAnimationController&
CombatBotAnimationRuntime::controller() const noexcept {
  return controller_;
}

bool CombatBotAnimationRuntime::visible() const noexcept { return visible_; }

}  // namespace stw
