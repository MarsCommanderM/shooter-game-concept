#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <glm/glm.hpp>

namespace stw {

inline constexpr std::size_t kCombatBotCount = 3u;
inline constexpr std::size_t kCombatObstacleCapacity = 8u;
inline constexpr std::size_t kBotPatrolPointCount = 2u;

enum class BotAiState {
  Idle,
  Patrol,
  Acquire,
  Chase,
  Attack,
  Dead,
};

const char* BotAiStateName(BotAiState state) noexcept;

struct PlayerCombatState {
  int maxHealth = 100;
  int health = 100;
  bool alive = true;
  unsigned int deathCount = 0u;
  float respawnTimer = 0.0f;
  double lastDamageTime = -1.0;
  float hitFeedbackTimer = 0.0f;
};

struct CombatObstacle {
  glm::vec3 center{0.0f};
  glm::vec3 halfExtents{0.0f};
  bool blocksMovement = true;
  bool blocksSight = true;
};

struct BotSpawnDefinition {
  glm::vec3 position{0.0f};
  glm::vec3 forward{0.0f, 0.0f, 1.0f};
  std::array<glm::vec3, kBotPatrolPointCount> patrolPoints{};
};

struct CombatArenaLayout {
  glm::vec3 playerSpawn{0.0f, 1.7f, 0.0f};
  glm::vec2 minimum{-10.8f, -24.5f};
  glm::vec2 maximum{10.8f, 3.0f};
  std::array<BotSpawnDefinition, kCombatBotCount> botSpawns{};
  std::array<CombatObstacle, kCombatObstacleCapacity> obstacles{};
  std::size_t obstacleCount = 0u;
};

CombatArenaLayout MakeTrainingCombatArena();

struct CombatTuning {
  int playerMaxHealth = 100;
  float playerRespawnSeconds = 3.0f;
  float playerHitFeedbackSeconds = 0.22f;
  float botMaxHealth = 100.0f;
  float botRespawnSeconds = 4.0f;
  float botRadius = 0.55f;
  float botWalkSpeed = 2.25f;
  float patrolArrivalRadius = 0.45f;
  float sightRange = 32.0f;
  float fieldOfViewDegrees = 130.0f;
  float acquireSeconds = 0.16f;
  float loseTargetSeconds = 0.85f;
  float attackRange = 18.0f;
  float botFireCooldown = 0.72f;
  int botDamage = 8;
  float botSpreadRadians = 0.028f;
};

struct BotCombatState {
  int id = 0;
  glm::vec3 position{0.0f};
  glm::vec3 forward{0.0f, 0.0f, 1.0f};
  glm::vec3 velocity{0.0f};
  float maxHealth = 100.0f;
  float health = 100.0f;
  bool alive = true;
  float respawnTimer = 0.0f;
  BotAiState state = BotAiState::Idle;
  bool hasTarget = false;
  float acquireTimer = 0.0f;
  float loseTargetTimer = 0.0f;
  float weaponCooldown = 0.0f;
  glm::vec3 lastKnownPlayerPosition{0.0f};
  std::size_t patrolPoint = 0u;
  std::uint32_t randomState = 1u;
  bool firedThisFrame = false;
  bool respawnedThisFrame = false;

  bool moving() const noexcept;
};

struct BotRayHit {
  std::size_t botIndex = 0u;
  glm::vec3 position{0.0f};
  float distance = 0.0f;
};

struct BotShotEvent {
  std::size_t botIndex = 0u;
  glm::vec3 origin{0.0f};
  glm::vec3 direction{0.0f, 0.0f, 1.0f};
  glm::vec3 end{0.0f};
  bool hitPlayer = false;
  int damage = 0;
};

struct CombatFrameEvents {
  std::array<BotShotEvent, kCombatBotCount> botShots{};
  std::size_t botShotCount = 0u;
  std::array<bool, kCombatBotCount> botDied{};
  std::array<bool, kCombatBotCount> botRespawned{};
  bool playerDamaged = false;
  bool playerDied = false;
  bool playerRespawned = false;

  void Clear() noexcept;
};

// Renderer-free, fixed-capacity combat simulation for the native training
// arena. Browser and presentation layers can observe it but never author
// health, perception, fire timing, damage, death, or respawn decisions.
class CombatSandbox {
 public:
  explicit CombatSandbox(
      CombatArenaLayout layout = MakeTrainingCombatArena(),
      CombatTuning tuning = {});

  void Reset() noexcept;

  // All input is validated before state changes. A failed update leaves the
  // previous valid simulation untouched.
  bool Update(float deltaSeconds,
              const glm::vec3& playerPosition,
              const glm::vec3& playerForward,
              std::string* error = nullptr);

  std::optional<BotRayHit> RaycastBots(const glm::vec3& origin,
                                       const glm::vec3& direction,
                                       float maxRange) const;
  bool ApplyDamageToBot(std::size_t botIndex,
                        float damage,
                        std::string* error = nullptr);
  bool ApplyDamageToPlayer(int damage, std::string* error = nullptr);

  bool HasLineOfSight(const glm::vec3& from,
                      const glm::vec3& to) const noexcept;

  const PlayerCombatState& player() const noexcept;
  const std::array<BotCombatState, kCombatBotCount>& bots() const noexcept;
  const CombatFrameEvents& events() const noexcept;
  const CombatArenaLayout& layout() const noexcept;
  const CombatTuning& tuning() const noexcept;
  std::size_t botsAlive() const noexcept;
  double elapsedSeconds() const noexcept;

 private:
  bool ValidateConfiguration(std::string* error) const;
  void RespawnPlayer() noexcept;
  void RespawnBot(std::size_t index) noexcept;
  void UpdateBot(std::size_t index,
                 float deltaSeconds,
                 const glm::vec3& playerPosition,
                 const glm::vec3& playerForward,
                 bool playerJustRespawned) noexcept;
  void MoveBot(BotCombatState& bot,
               const glm::vec3& goal,
               float deltaSeconds) const noexcept;
  void FireBot(std::size_t index,
               const glm::vec3& playerPosition) noexcept;
  bool CanOccupy(const glm::vec3& position) const noexcept;
  float NextSigned(BotCombatState& bot) noexcept;

  CombatArenaLayout layout_;
  CombatTuning tuning_;
  PlayerCombatState player_;
  std::array<BotCombatState, kCombatBotCount> bots_{};
  CombatFrameEvents events_;
  double elapsedSeconds_ = 0.0;
};

}  // namespace stw
