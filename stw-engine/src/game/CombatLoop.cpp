#include "CombatLoop.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace stw {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDirectionEpsilon = 1.0e-5f;
constexpr float kAvoidanceCommitSeconds = 0.8f;
constexpr float kBotTurnRadiansPerSecond = 5.5f;

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool IsFinite(float value) {
  return std::isfinite(static_cast<double>(value));
}

bool IsFinite(const glm::vec2& value) {
  return IsFinite(value.x) && IsFinite(value.y);
}

bool IsFinite(const glm::vec3& value) {
  return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

glm::vec3 HorizontalDirection(const glm::vec3& value,
                              const glm::vec3& fallback) {
  const glm::vec3 horizontal(value.x, 0.0f, value.z);
  const float length = glm::length(horizontal);
  if (!IsFinite(length) || length <= kDirectionEpsilon) return fallback;
  return horizontal / length;
}

glm::vec3 TurnHorizontalToward(const glm::vec3& current,
                               const glm::vec3& target,
                               float maximumRadians) {
  const glm::vec3 from = HorizontalDirection(
      current, glm::vec3(0.0f, 0.0f, 1.0f));
  const glm::vec3 to = HorizontalDirection(target, from);
  const float fromYaw = std::atan2(from.x, from.z);
  const float toYaw = std::atan2(to.x, to.z);
  const float difference = std::atan2(std::sin(toYaw - fromYaw),
                                      std::cos(toYaw - fromYaw));
  const float yaw = fromYaw + std::clamp(
      difference, -std::max(0.0f, maximumRadians),
      std::max(0.0f, maximumRadians));
  return glm::vec3(std::sin(yaw), 0.0f, std::cos(yaw));
}

std::optional<float> RaySphereDistance(const glm::vec3& origin,
                                       const glm::vec3& direction,
                                       const glm::vec3& center,
                                       float radius,
                                       float maxDistance) {
  const glm::vec3 relative = origin - center;
  const float projection = glm::dot(relative, direction);
  const float constant = glm::dot(relative, relative) - radius * radius;
  const float discriminant = projection * projection - constant;
  if (discriminant < 0.0f) return std::nullopt;
  const float root = std::sqrt(discriminant);
  float distance = -projection - root;
  if (distance < 0.0f) distance = -projection + root;
  if (distance < 0.0f || distance > maxDistance) return std::nullopt;
  return distance;
}

std::optional<float> RayBoxDistance(const glm::vec3& origin,
                                    const glm::vec3& direction,
                                    const CombatObstacle& obstacle,
                                    float maxDistance) {
  float nearDistance = 0.0f;
  float farDistance = maxDistance;
  const glm::vec3 minimum = obstacle.center - obstacle.halfExtents;
  const glm::vec3 maximum = obstacle.center + obstacle.halfExtents;
  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(direction[axis]) <= kDirectionEpsilon) {
      if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) {
        return std::nullopt;
      }
      continue;
    }
    const float inverse = 1.0f / direction[axis];
    float first = (minimum[axis] - origin[axis]) * inverse;
    float second = (maximum[axis] - origin[axis]) * inverse;
    if (first > second) std::swap(first, second);
    nearDistance = std::max(nearDistance, first);
    farDistance = std::min(farDistance, second);
    if (nearDistance > farDistance) return std::nullopt;
  }
  if (farDistance < 0.0f || nearDistance > maxDistance) return std::nullopt;
  return std::max(nearDistance, 0.0f);
}

bool SegmentBlocked(const CombatArenaLayout& layout,
                    const glm::vec3& from,
                    const glm::vec3& to) {
  const glm::vec3 delta = to - from;
  const float distance = glm::length(delta);
  if (!IsFinite(distance) || distance <= kDirectionEpsilon) return false;
  const glm::vec3 direction = delta / distance;
  for (std::size_t index = 0; index < layout.obstacleCount; ++index) {
    const CombatObstacle& obstacle = layout.obstacles[index];
    if (!obstacle.blocksSight) continue;
    const std::optional<float> hit =
        RayBoxDistance(from, direction, obstacle, distance);
    if (hit && *hit > 1.0e-3f && *hit < distance - 1.0e-3f) return true;
  }
  return false;
}

bool CircleOverlapsBox(const glm::vec3& position,
                       float radius,
                       const CombatObstacle& obstacle) {
  const float nearestX = std::clamp(position.x,
      obstacle.center.x - obstacle.halfExtents.x,
      obstacle.center.x + obstacle.halfExtents.x);
  const float nearestZ = std::clamp(position.z,
      obstacle.center.z - obstacle.halfExtents.z,
      obstacle.center.z + obstacle.halfExtents.z);
  const float dx = position.x - nearestX;
  const float dz = position.z - nearestZ;
  return dx * dx + dz * dz < radius * radius;
}

}  // namespace

const char* BotAiStateName(BotAiState state) noexcept {
  switch (state) {
    case BotAiState::Idle: return "Idle";
    case BotAiState::Patrol: return "Patrol";
    case BotAiState::Acquire: return "Acquire";
    case BotAiState::Chase: return "Chase";
    case BotAiState::Attack: return "Attack";
    case BotAiState::Dead: return "Dead";
  }
  return "Unknown";
}

bool BotCombatState::moving() const noexcept {
  return alive && glm::dot(velocity, velocity) > 1.0e-4f;
}

void CombatFrameEvents::Clear() noexcept { *this = CombatFrameEvents{}; }

CombatArenaLayout MakeTrainingCombatArena() {
  CombatArenaLayout layout;
  layout.playerSpawn = glm::vec3(0.0f, 1.7f, 0.0f);
  layout.minimum = glm::vec2(-10.8f, -24.5f);
  layout.maximum = glm::vec2(10.8f, 3.0f);
  layout.botSpawns = {{
      {glm::vec3(0.0f, 0.3f, -22.0f), glm::vec3(0.0f, 0.0f, 1.0f),
       {glm::vec3(0.0f, 0.3f, -22.0f),
        glm::vec3(-3.8f, 0.3f, -17.0f)}},
      {glm::vec3(3.2f, 0.3f, -18.0f), glm::vec3(0.0f, 0.0f, 1.0f),
       {glm::vec3(3.2f, 0.3f, -18.0f),
        glm::vec3(6.8f, 0.3f, -12.0f)}},
      {glm::vec3(6.4f, 0.3f, -14.0f), glm::vec3(0.0f, 0.0f, 1.0f),
       {glm::vec3(6.4f, 0.3f, -14.0f),
        glm::vec3(2.4f, 0.3f, -10.5f)}},
  }};
  layout.obstacles[0] = {{-9.0f, 0.72f, -8.0f},
                         {1.0f, 0.72f, 1.0f}, true, true};
  layout.obstacles[1] = {{9.0f, 0.72f, -8.0f},
                         {1.0f, 0.72f, 1.0f}, true, true};
  layout.obstacles[2] = {{-9.5f, 1.25f, -18.0f},
                         {0.5f, 1.25f, 1.5f}, true, true};
  layout.obstacles[3] = {{9.5f, 1.25f, -18.0f},
                         {0.5f, 1.25f, 1.5f}, true, true};
  // The low central platform affects feet but cannot occlude eye-to-eye LOS.
  layout.obstacles[4] = {{0.0f, 0.15f, -8.0f},
                         {1.5f, 0.15f, 0.9f}, true, false};
  layout.obstacleCount = 5u;
  return layout;
}

CombatSandbox::CombatSandbox(CombatArenaLayout layout, CombatTuning tuning)
    : layout_(std::move(layout)), tuning_(tuning) {
  Reset();
}

void CombatSandbox::Reset() noexcept {
  player_ = PlayerCombatState{};
  player_.maxHealth = std::max(tuning_.playerMaxHealth, 1);
  player_.health = player_.maxHealth;
  for (std::size_t index = 0; index < bots_.size(); ++index) {
    BotCombatState& bot = bots_[index];
    bot = BotCombatState{};
    bot.id = static_cast<int>(index);
    bot.position = layout_.botSpawns[index].position;
    bot.forward = HorizontalDirection(layout_.botSpawns[index].forward,
                                      glm::vec3(0.0f, 0.0f, 1.0f));
    bot.maxHealth = std::max(tuning_.botMaxHealth, 1.0f);
    bot.health = bot.maxHealth;
    bot.weaponCooldown = 0.18f + static_cast<float>(index) * 0.16f;
    bot.randomState = 0x9e3779b9u ^
        (static_cast<std::uint32_t>(index) + 1u) * 0x85ebca6bu;
    bot.avoidanceSign = index % 2u == 0u ? 1 : -1;
    bot.lastKnownPlayerPosition = layout_.playerSpawn;
  }
  events_.Clear();
  elapsedSeconds_ = 0.0;
}

bool CombatSandbox::ValidateConfiguration(std::string* error) const {
  if (layout_.obstacleCount > layout_.obstacles.size() ||
      !IsFinite(layout_.playerSpawn) || !IsFinite(layout_.minimum) ||
      !IsFinite(layout_.maximum) || layout_.minimum.x >= layout_.maximum.x ||
      layout_.minimum.y >= layout_.maximum.y) {
    return Fail(error, "combat arena layout is invalid");
  }
  if (tuning_.playerMaxHealth <= 0 || tuning_.botDamage <= 0 ||
      !IsFinite(tuning_.playerRespawnSeconds) ||
      tuning_.playerRespawnSeconds < 0.0f ||
      !IsFinite(tuning_.playerHitFeedbackSeconds) ||
      tuning_.playerHitFeedbackSeconds < 0.0f ||
      !IsFinite(tuning_.botRespawnSeconds) ||
      tuning_.botRespawnSeconds < 0.0f ||
      !IsFinite(tuning_.botMaxHealth) || tuning_.botMaxHealth <= 0.0f ||
      !IsFinite(tuning_.botRadius) || tuning_.botRadius <= 0.0f ||
      !IsFinite(tuning_.botWalkSpeed) || tuning_.botWalkSpeed < 0.0f ||
      !IsFinite(tuning_.patrolArrivalRadius) ||
      tuning_.patrolArrivalRadius < 0.0f ||
      !IsFinite(tuning_.sightRange) || tuning_.sightRange <= 0.0f ||
      !IsFinite(tuning_.fieldOfViewDegrees) ||
      tuning_.fieldOfViewDegrees <= 0.0f ||
      tuning_.fieldOfViewDegrees > 360.0f ||
      !IsFinite(tuning_.acquireSeconds) || tuning_.acquireSeconds < 0.0f ||
      !IsFinite(tuning_.loseTargetSeconds) ||
      tuning_.loseTargetSeconds < 0.0f ||
      !IsFinite(tuning_.attackRange) || tuning_.attackRange <= 0.0f ||
      !IsFinite(tuning_.botFireCooldown) ||
      tuning_.botFireCooldown <= 0.0f ||
      !IsFinite(tuning_.botSpreadRadians) ||
      tuning_.botSpreadRadians < 0.0f) {
    return Fail(error, "combat tuning is invalid");
  }
  for (const BotSpawnDefinition& spawn : layout_.botSpawns) {
    if (!IsFinite(spawn.position) || !IsFinite(spawn.forward)) {
      return Fail(error, "combat bot spawn is not finite");
    }
    for (const glm::vec3& patrol : spawn.patrolPoints) {
      if (!IsFinite(patrol)) return Fail(error, "combat patrol point is not finite");
    }
  }
  for (std::size_t index = 0; index < layout_.obstacleCount; ++index) {
    const CombatObstacle& obstacle = layout_.obstacles[index];
    if (!IsFinite(obstacle.center) || !IsFinite(obstacle.halfExtents) ||
        obstacle.halfExtents.x < 0.0f || obstacle.halfExtents.y < 0.0f ||
        obstacle.halfExtents.z < 0.0f) {
      return Fail(error, "combat obstacle is invalid");
    }
  }
  return true;
}

bool CombatSandbox::Update(float deltaSeconds,
                           const glm::vec3& playerPosition,
                           const glm::vec3& playerForward,
                           std::string* error) {
  if (error) error->clear();
  if (!IsFinite(deltaSeconds) || deltaSeconds < 0.0f ||
      !IsFinite(playerPosition) || !IsFinite(playerForward) ||
      glm::length(playerForward) <= kDirectionEpsilon ||
      !ValidateConfiguration(error)) {
    if (error && error->empty()) {
      *error = "combat update input must be finite and non-negative";
    }
    return false;
  }

  events_.Clear();
  elapsedSeconds_ += static_cast<double>(deltaSeconds);
  player_.hitFeedbackTimer =
      std::max(0.0f, player_.hitFeedbackTimer - deltaSeconds);
  if (!player_.alive) {
    player_.respawnTimer = std::max(0.0f,
                                    player_.respawnTimer - deltaSeconds);
    if (player_.respawnTimer <= 0.0f) RespawnPlayer();
  }

  const glm::vec3 activePlayerPosition =
      events_.playerRespawned ? layout_.playerSpawn : playerPosition;
  for (std::size_t index = 0; index < bots_.size(); ++index) {
    UpdateBot(index, deltaSeconds, activePlayerPosition, playerForward,
              events_.playerRespawned);
  }
  return true;
}

void CombatSandbox::RespawnPlayer() noexcept {
  player_.health = player_.maxHealth;
  player_.alive = true;
  player_.respawnTimer = 0.0f;
  player_.hitFeedbackTimer = 0.0f;
  events_.playerRespawned = true;
}

void CombatSandbox::RespawnBot(std::size_t index) noexcept {
  const BotSpawnDefinition& spawn = layout_.botSpawns[index];
  BotCombatState& bot = bots_[index];
  const std::uint32_t randomState = bot.randomState;
  glm::vec3 respawnPosition = spawn.position;
  for (std::size_t other = 0; other < bots_.size(); ++other) {
    if (other == index || !bots_[other].alive) continue;
    const glm::vec3 separation = bots_[other].position - respawnPosition;
    if (separation.x * separation.x + separation.z * separation.z < 2.25f) {
      respawnPosition = spawn.patrolPoints[1u];
      break;
    }
  }
  bot = BotCombatState{};
  bot.id = static_cast<int>(index);
  bot.position = respawnPosition;
  bot.forward = HorizontalDirection(spawn.forward,
                                    glm::vec3(0.0f, 0.0f, 1.0f));
  bot.maxHealth = std::max(tuning_.botMaxHealth, 1.0f);
  bot.health = bot.maxHealth;
  bot.weaponCooldown = 0.25f + static_cast<float>(index) * 0.12f;
  bot.randomState = randomState == 0u ? static_cast<std::uint32_t>(index + 1u)
                                     : randomState;
  bot.avoidanceSign = index % 2u == 0u ? 1 : -1;
  bot.lastKnownPlayerPosition = layout_.playerSpawn;
  bot.respawnedThisFrame = true;
  events_.botRespawned[index] = true;
}

void CombatSandbox::UpdateBot(std::size_t index,
                              float deltaSeconds,
                              const glm::vec3& playerPosition,
                              const glm::vec3&,
                              bool playerJustRespawned) noexcept {
  BotCombatState& bot = bots_[index];
  bot.firedThisFrame = false;
  bot.respawnedThisFrame = false;
  bot.weaponCooldown = std::max(0.0f, bot.weaponCooldown - deltaSeconds);
  if (!bot.alive) {
    bot.velocity = glm::vec3(0.0f);
    bot.state = BotAiState::Dead;
    bot.respawnTimer = std::max(0.0f, bot.respawnTimer - deltaSeconds);
    if (bot.respawnTimer <= 0.0f) RespawnBot(index);
    return;
  }

  if (!player_.alive) {
    bot.hasTarget = false;
    bot.acquireTimer = 0.0f;
    bot.loseTargetTimer = 0.0f;
  }
  if (playerJustRespawned) {
    bot.hasTarget = false;
    bot.acquireTimer = 0.0f;
    bot.loseTargetTimer = 0.0f;
    bot.weaponCooldown = std::max(bot.weaponCooldown, 0.25f);
  }

  const glm::vec3 botEye = bot.position + glm::vec3(0.0f, 1.25f, 0.0f);
  const glm::vec3 toPlayer = playerPosition - botEye;
  const float playerDistance = glm::length(toPlayer);
  const glm::vec3 horizontalPlayerDirection =
      HorizontalDirection(toPlayer, bot.forward);
  const float halfFovRadians =
      tuning_.fieldOfViewDegrees * 0.5f * kPi / 180.0f;
  const bool insideFov = tuning_.fieldOfViewDegrees >= 359.9f ||
      glm::dot(bot.forward, horizontalPlayerDirection) >=
          std::cos(halfFovRadians);
  const bool visible = player_.alive && !playerJustRespawned &&
      playerDistance <= tuning_.sightRange && insideFov &&
      HasLineOfSight(botEye, playerPosition);

  if (visible) {
    bot.lastKnownPlayerPosition = playerPosition;
    bot.loseTargetTimer = tuning_.loseTargetSeconds;
    if (!bot.hasTarget) {
      bot.acquireTimer += deltaSeconds;
      bot.state = BotAiState::Acquire;
      if (bot.acquireTimer + 1.0e-6f >= tuning_.acquireSeconds) {
        bot.hasTarget = true;
      }
    }
  } else {
    bot.acquireTimer = 0.0f;
    if (bot.hasTarget) {
      bot.loseTargetTimer = std::max(0.0f,
                                     bot.loseTargetTimer - deltaSeconds);
      if (bot.loseTargetTimer <= 0.0f) bot.hasTarget = false;
    }
  }

  if (bot.hasTarget) {
    if (visible && playerDistance <= tuning_.attackRange) {
      bot.state = BotAiState::Attack;
      bot.velocity = glm::vec3(0.0f);
      bot.avoidanceTimer = 0.0f;
      bot.avoidanceDirection = glm::vec3(0.0f);
      bot.forward = TurnHorizontalToward(
          bot.forward, horizontalPlayerDirection,
          kBotTurnRadiansPerSecond * deltaSeconds);
      if (bot.weaponCooldown <= 0.0f) FireBot(index, playerPosition);
      return;
    }
    if (bot.state != BotAiState::Chase) {
      bot.avoidanceTimer = 0.0f;
      bot.avoidanceDirection = glm::vec3(0.0f);
    }
    bot.state = BotAiState::Chase;
    MoveBot(bot, bot.lastKnownPlayerPosition, deltaSeconds);
    return;
  }

  const glm::vec3 patrolGoal =
      layout_.botSpawns[index].patrolPoints[bot.patrolPoint];
  const glm::vec3 patrolDelta = patrolGoal - bot.position;
  const float patrolDistance = glm::length(glm::vec2(patrolDelta.x,
                                                     patrolDelta.z));
  if (patrolDistance <= tuning_.patrolArrivalRadius) {
    bot.patrolPoint = (bot.patrolPoint + 1u) % kBotPatrolPointCount;
    bot.avoidanceTimer = 0.0f;
    bot.avoidanceDirection = glm::vec3(0.0f);
  }
  const BotAiState movementState =
      deltaSeconds <= 0.0f ? BotAiState::Idle : BotAiState::Patrol;
  if (bot.state != movementState) {
    bot.avoidanceTimer = 0.0f;
    bot.avoidanceDirection = glm::vec3(0.0f);
  }
  bot.state = movementState;
  MoveBot(bot, layout_.botSpawns[index].patrolPoints[bot.patrolPoint],
          deltaSeconds);
}

void CombatSandbox::MoveBot(BotCombatState& bot,
                            const glm::vec3& goal,
                            float deltaSeconds) const noexcept {
  const glm::vec3 desired = HorizontalDirection(goal - bot.position,
                                                 glm::vec3(0.0f));
  if (glm::length(desired) <= kDirectionEpsilon || deltaSeconds <= 0.0f) {
    bot.velocity = glm::vec3(0.0f);
    if (glm::length(desired) <= kDirectionEpsilon) {
      bot.avoidanceTimer = 0.0f;
      bot.avoidanceDirection = glm::vec3(0.0f);
    }
    return;
  }
  bot.avoidanceTimer = std::max(0.0f, bot.avoidanceTimer - deltaSeconds);
  const float step = tuning_.botWalkSpeed * deltaSeconds;
  const float directLookAhead = std::max(step * 4.0f,
                                         tuning_.botRadius * 2.0f);
  const bool directCorridorClear = CanTraverse(
      bot.position, bot.position + desired * directLookAhead);
  bool avoiding = glm::length(bot.avoidanceDirection) > kDirectionEpsilon;
  if (avoiding && bot.avoidanceTimer <= 0.0f && directCorridorClear) {
    bot.avoidanceDirection = glm::vec3(0.0f);
    avoiding = false;
  }
  if (!avoiding && !directCorridorClear) {
    const float sideSign = bot.avoidanceSign >= 0 ? 1.0f : -1.0f;
    bot.avoidanceDirection = glm::vec3(
        -desired.z * sideSign, 0.0f, desired.x * sideSign);
    bot.avoidanceTimer = kAvoidanceCommitSeconds;
    avoiding = true;
  }

  const glm::vec3 preferredSide = HorizontalDirection(
      bot.avoidanceDirection, glm::vec3(-desired.z, 0.0f, desired.x));
  const glm::vec3 otherSide = -preferredSide;
  const glm::vec3 preferredBlend = HorizontalDirection(
      preferredSide * 0.92f + desired * 0.38f, preferredSide);
  const glm::vec3 otherBlend = HorizontalDirection(
      otherSide * 0.92f + desired * 0.38f, otherSide);

  struct CandidateDirection {
    glm::vec3 direction;
    int side;
    bool direct;
  };
  std::array<CandidateDirection, 4u> candidates{};
  std::size_t candidateCount = 0u;
  if (avoiding) {
    candidates[0] = {preferredSide, bot.avoidanceSign, false};
    candidates[1] = {preferredBlend, bot.avoidanceSign, false};
    candidates[2] = {otherSide, -bot.avoidanceSign, false};
    candidates[3] = {otherBlend, -bot.avoidanceSign, false};
    candidateCount = candidates.size();
  } else {
    candidates[0] = {desired, 0, true};
    candidateCount = 1u;
  }

  for (std::size_t index = 0; index < candidateCount; ++index) {
    const CandidateDirection& movement = candidates[index];
    glm::vec3 candidate = bot.position + movement.direction * step;
    candidate.y = bot.position.y;
    if (!CanTraverse(bot.position, candidate)) continue;
    bot.velocity = (candidate - bot.position) / deltaSeconds;
    bot.position = candidate;
    if (!movement.direct) {
      if (movement.side != bot.avoidanceSign) {
        bot.avoidanceSign = movement.side;
        bot.avoidanceDirection = movement.direction;
        bot.avoidanceTimer = kAvoidanceCommitSeconds;
      }
    }
    bot.forward = TurnHorizontalToward(
        bot.forward, bot.velocity, kBotTurnRadiansPerSecond * deltaSeconds);
    return;
  }
  bot.velocity = glm::vec3(0.0f);
}

void CombatSandbox::FireBot(std::size_t index,
                            const glm::vec3& playerPosition) noexcept {
  BotCombatState& bot = bots_[index];
  const glm::vec3 origin = bot.position + glm::vec3(0.0f, 1.3f, 0.0f);
  glm::vec3 baseDirection = playerPosition - origin;
  const float baseLength = glm::length(baseDirection);
  if (baseLength <= kDirectionEpsilon) return;
  baseDirection /= baseLength;
  glm::vec3 right = glm::cross(baseDirection, glm::vec3(0.0f, 1.0f, 0.0f));
  if (glm::length(right) <= kDirectionEpsilon) {
    right = glm::vec3(1.0f, 0.0f, 0.0f);
  } else {
    right = glm::normalize(right);
  }
  const glm::vec3 up = glm::normalize(glm::cross(right, baseDirection));
  const glm::vec3 direction = glm::normalize(
      baseDirection + right * NextSigned(bot) * tuning_.botSpreadRadians +
      up * NextSigned(bot) * tuning_.botSpreadRadians);

  const std::optional<float> playerHit = RaySphereDistance(
      origin, direction, playerPosition - glm::vec3(0.0f, 0.35f, 0.0f),
      0.62f, tuning_.attackRange);
  const bool hitPlayer = playerHit.has_value() &&
      HasLineOfSight(origin, origin + direction * *playerHit);
  BotShotEvent event;
  event.botIndex = index;
  event.origin = origin;
  event.direction = direction;
  event.hitPlayer = hitPlayer;
  event.damage = hitPlayer ? tuning_.botDamage : 0;
  event.end = hitPlayer ? origin + direction * *playerHit
                        : origin + direction * tuning_.attackRange;
  if (events_.botShotCount < events_.botShots.size()) {
    events_.botShots[events_.botShotCount++] = event;
  }
  bot.firedThisFrame = true;
  bot.weaponCooldown = tuning_.botFireCooldown;
  if (hitPlayer) ApplyDamageToPlayer(tuning_.botDamage, nullptr);
}

bool CombatSandbox::CanOccupy(const glm::vec3& position) const noexcept {
  if (position.x < layout_.minimum.x + tuning_.botRadius ||
      position.x > layout_.maximum.x - tuning_.botRadius ||
      position.z < layout_.minimum.y + tuning_.botRadius ||
      position.z > layout_.maximum.y - tuning_.botRadius) {
    return false;
  }
  for (std::size_t index = 0; index < layout_.obstacleCount; ++index) {
    const CombatObstacle& obstacle = layout_.obstacles[index];
    if (obstacle.blocksMovement &&
        CircleOverlapsBox(position, tuning_.botRadius, obstacle)) {
      return false;
    }
  }
  return true;
}

bool CombatSandbox::CanTraverse(const glm::vec3& from,
                                const glm::vec3& to) const noexcept {
  if (!IsFinite(from) || !IsFinite(to)) return false;
  const glm::vec3 delta(to.x - from.x, 0.0f, to.z - from.z);
  const float distance = glm::length(delta);
  if (!IsFinite(distance)) return false;
  if (distance <= kDirectionEpsilon) return CanOccupy(to);

  // Sample at less than half a bot radius so a large update cannot tunnel
  // through a wall or expanded cover volume. GameRuntime already bounds dt;
  // this additionally protects deterministic tests and transient stalls.
  const float sampleDistance = std::max(tuning_.botRadius * 0.4f, 0.05f);
  const float sampleCount = std::ceil(distance / sampleDistance);
  if (!IsFinite(sampleCount) || sampleCount > 4096.0f) return false;
  const int samples = std::max(1, static_cast<int>(sampleCount));
  for (int sample = 1; sample <= samples; ++sample) {
    const float amount = static_cast<float>(sample) /
                         static_cast<float>(samples);
    glm::vec3 position = from + delta * amount;
    position.y = from.y;
    if (!CanOccupy(position)) return false;
  }
  return true;
}

float CombatSandbox::NextSigned(BotCombatState& bot) noexcept {
  std::uint32_t value = bot.randomState;
  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  bot.randomState = value == 0u ? 1u : value;
  return static_cast<float>(bot.randomState & 0x00ffffffu) /
             static_cast<float>(0x007fffffu) -
         1.0f;
}

std::optional<BotRayHit> CombatSandbox::RaycastBots(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxRange) const {
  if (!IsFinite(origin) || !IsFinite(direction) || !IsFinite(maxRange) ||
      maxRange <= 0.0f) {
    return std::nullopt;
  }
  const float directionLength = glm::length(direction);
  if (directionLength <= kDirectionEpsilon) return std::nullopt;
  const glm::vec3 normalizedDirection = direction / directionLength;

  float obstacleDistance = maxRange;
  for (std::size_t index = 0; index < layout_.obstacleCount; ++index) {
    const CombatObstacle& obstacle = layout_.obstacles[index];
    if (!obstacle.blocksSight) continue;
    const std::optional<float> hit =
        RayBoxDistance(origin, normalizedDirection, obstacle, maxRange);
    if (hit) obstacleDistance = std::min(obstacleDistance, *hit);
  }

  std::optional<BotRayHit> closest;
  float closestDistance = std::min(maxRange, obstacleDistance);
  for (std::size_t index = 0; index < bots_.size(); ++index) {
    const BotCombatState& bot = bots_[index];
    if (!bot.alive) continue;
    const glm::vec3 center = bot.position + glm::vec3(0.0f, 1.0f, 0.0f);
    const std::optional<float> hit = RaySphereDistance(
        origin, normalizedDirection, center, 0.78f, closestDistance);
    if (!hit || *hit >= closestDistance) continue;
    closestDistance = *hit;
    closest = BotRayHit{index, origin + normalizedDirection * *hit, *hit};
  }
  return closest;
}

bool CombatSandbox::ApplyDamageToBot(std::size_t botIndex,
                                     float damage,
                                     std::string* error) {
  if (error) error->clear();
  if (botIndex >= bots_.size()) return Fail(error, "combat bot index is invalid");
  if (!IsFinite(damage) || damage <= 0.0f) {
    return Fail(error, "combat bot damage must be positive and finite");
  }
  BotCombatState& bot = bots_[botIndex];
  if (!bot.alive) return true;
  bot.health = std::max(0.0f, bot.health - damage);
  if (bot.health <= 0.0f) {
    bot.alive = false;
    bot.state = BotAiState::Dead;
    bot.hasTarget = false;
    bot.velocity = glm::vec3(0.0f);
    bot.respawnTimer = tuning_.botRespawnSeconds;
    bot.firedThisFrame = false;
    events_.botDied[botIndex] = true;
  }
  return true;
}

bool CombatSandbox::ApplyDamageToPlayer(int damage, std::string* error) {
  if (error) error->clear();
  if (damage <= 0) return Fail(error, "player damage must be positive");
  if (!player_.alive) return true;
  player_.health = std::max(0, player_.health - damage);
  player_.lastDamageTime = elapsedSeconds_;
  player_.hitFeedbackTimer = tuning_.playerHitFeedbackSeconds;
  events_.playerDamaged = true;
  if (player_.health == 0) {
    player_.alive = false;
    ++player_.deathCount;
    player_.respawnTimer = tuning_.playerRespawnSeconds;
    events_.playerDied = true;
  }
  return true;
}

bool CombatSandbox::HasLineOfSight(const glm::vec3& from,
                                   const glm::vec3& to) const noexcept {
  if (!IsFinite(from) || !IsFinite(to)) return false;
  return !SegmentBlocked(layout_, from, to);
}

const PlayerCombatState& CombatSandbox::player() const noexcept {
  return player_;
}

const std::array<BotCombatState, kCombatBotCount>& CombatSandbox::bots()
    const noexcept {
  return bots_;
}

const CombatFrameEvents& CombatSandbox::events() const noexcept {
  return events_;
}

const CombatArenaLayout& CombatSandbox::layout() const noexcept {
  return layout_;
}

const CombatTuning& CombatSandbox::tuning() const noexcept { return tuning_; }

std::size_t CombatSandbox::botsAlive() const noexcept {
  std::size_t alive = 0u;
  for (const BotCombatState& bot : bots_) {
    if (bot.alive) ++alive;
  }
  return alive;
}

double CombatSandbox::elapsedSeconds() const noexcept {
  return elapsedSeconds_;
}

}  // namespace stw
