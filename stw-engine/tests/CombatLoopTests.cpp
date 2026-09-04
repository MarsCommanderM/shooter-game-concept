#include "game/CombatBotAnimationRuntime.hpp"
#include "game/CombatLoop.hpp"

#include "assets/StwcFormat.hpp"
#include "assets/gltf.h"
#include "runtime/ModelInstance.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace stw {
namespace {

static_assert(STWC_VERSION == 3u,
              "Combat Loop V1 must not change the STWC cache format");

void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

bool Near(float left, float right, float epsilon = 1.0e-4f) {
  return std::fabs(left - right) <= epsilon;
}

bool Finite(const glm::vec3& value) {
  return std::isfinite(static_cast<double>(value.x)) &&
      std::isfinite(static_cast<double>(value.y)) &&
      std::isfinite(static_cast<double>(value.z));
}

bool InsideExpandedObstacle(const glm::vec3& position,
                            float radius,
                            const CombatObstacle& obstacle) {
  const float nearestX = std::max(
      obstacle.center.x - obstacle.halfExtents.x,
      std::min(position.x,
               obstacle.center.x + obstacle.halfExtents.x));
  const float nearestZ = std::max(
      obstacle.center.z - obstacle.halfExtents.z,
      std::min(position.z,
               obstacle.center.z + obstacle.halfExtents.z));
  const float dx = position.x - nearestX;
  const float dz = position.z - nearestZ;
  return dx * dx + dz * dz < radius * radius - 1.0e-5f;
}

CombatArenaLayout OpenArena() {
  CombatArenaLayout layout;
  layout.playerSpawn = glm::vec3(0.0f, 1.7f, 0.0f);
  layout.minimum = glm::vec2(-20.0f, -30.0f);
  layout.maximum = glm::vec2(20.0f, 5.0f);
  layout.obstacleCount = 0u;
  layout.botSpawns[0] = {
      glm::vec3(0.0f, 0.3f, -6.0f), glm::vec3(0.0f, 0.0f, 1.0f),
      {glm::vec3(0.0f, 0.3f, -6.0f),
       glm::vec3(2.0f, 0.3f, -6.0f)}};
  layout.botSpawns[1] = {
      glm::vec3(-16.0f, 0.3f, -25.0f), glm::vec3(0.0f, 0.0f, -1.0f),
      {glm::vec3(-16.0f, 0.3f, -25.0f),
       glm::vec3(-14.0f, 0.3f, -25.0f)}};
  layout.botSpawns[2] = {
      glm::vec3(16.0f, 0.3f, -25.0f), glm::vec3(0.0f, 0.0f, -1.0f),
      {glm::vec3(16.0f, 0.3f, -25.0f),
       glm::vec3(14.0f, 0.3f, -25.0f)}};
  return layout;
}

CombatTuning TestTuning() {
  CombatTuning tuning;
  tuning.sightRange = 12.0f;
  tuning.fieldOfViewDegrees = 90.0f;
  tuning.acquireSeconds = 0.0f;
  tuning.loseTargetSeconds = 0.4f;
  tuning.attackRange = 10.0f;
  tuning.botFireCooldown = 0.1f;
  tuning.botDamage = 10;
  tuning.botSpreadRadians = 0.0f;
  return tuning;
}

CombatArenaLayout CoverTraversalArena() {
  CombatArenaLayout layout = OpenArena();
  layout.minimum = glm::vec2(-5.0f, -10.0f);
  layout.maximum = glm::vec2(5.0f, 2.0f);
  layout.botSpawns[0] = {
      glm::vec3(0.0f, 0.3f, -8.0f), glm::vec3(0.0f, 0.0f, 1.0f),
      {glm::vec3(0.0f, 0.3f, -8.0f),
       glm::vec3(0.0f, 0.3f, 0.0f)}};
  layout.botSpawns[1] = {
      glm::vec3(-4.0f, 0.3f, -9.0f), glm::vec3(0.0f, 0.0f, 1.0f),
      {glm::vec3(-4.0f, 0.3f, -9.0f),
       glm::vec3(-3.0f, 0.3f, -8.0f)}};
  layout.botSpawns[2] = {
      glm::vec3(4.0f, 0.3f, -9.0f), glm::vec3(0.0f, 0.0f, 1.0f),
      {glm::vec3(4.0f, 0.3f, -9.0f),
       glm::vec3(3.0f, 0.3f, -8.0f)}};
  layout.obstacles[0] = {
      glm::vec3(0.0f, 1.0f, -4.0f),
      glm::vec3(0.9f, 1.0f, 0.75f), true, true};
  layout.obstacleCount = 1u;
  return layout;
}

void TestPlayerStartsFullAndBotAcquiresWithLos() {
  CombatSandbox combat(OpenArena(), TestTuning());
  Require(combat.player().alive && combat.player().health == 100 &&
              combat.player().maxHealth == 100 &&
              combat.player().deathCount == 0u,
          "player did not start alive with full health");
  std::string error;
  Require(combat.Update(0.2f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  Require(combat.bots()[0].hasTarget &&
              combat.bots()[0].state == BotAiState::Attack,
          "bot did not acquire an in-range player inside FOV and LOS");
}

void TestBlockedLosPreventsAcquireAndFire() {
  CombatArenaLayout layout = OpenArena();
  layout.obstacles[0] = {
      glm::vec3(0.0f, 1.2f, -3.0f), glm::vec3(1.5f, 1.5f, 0.5f), true, true};
  layout.obstacleCount = 1u;
  CombatSandbox combat(layout, TestTuning());
  std::string error;
  for (int step = 0; step < 20; ++step) {
    Require(combat.Update(0.1f, layout.playerSpawn,
                          glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
    Require(combat.events().botShotCount == 0u,
            "bot fired through sight-blocking arena cover");
  }
  Require(!combat.bots()[0].hasTarget,
          "bot acquired the player through sight-blocking arena cover");
}

void TestBotFireDamagesPlayerAndDeathIsSingleTransition() {
  CombatSandbox combat(OpenArena(), TestTuning());
  std::string error;
  Require(combat.Update(0.2f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  Require(combat.events().botShotCount == 1u &&
              combat.events().botShots[0].hitPlayer &&
              combat.player().health == 90,
          "timed native bot fire did not reduce player health");

  Require(combat.ApplyDamageToPlayer(200, &error), error);
  Require(!combat.player().alive && combat.player().health == 0 &&
              combat.player().deathCount == 1u &&
              combat.events().playerDied,
          "lethal player damage did not produce exactly one death");
  Require(combat.ApplyDamageToPlayer(50, &error), error);
  Require(combat.player().deathCount == 1u,
          "damage to an already-dead player duplicated the death transition");
}

void TestPlayerRespawnRestoresHealth() {
  CombatTuning tuning = TestTuning();
  tuning.sightRange = 0.1f;
  tuning.playerRespawnSeconds = 0.5f;
  CombatSandbox combat(OpenArena(), tuning);
  std::string error;
  Require(combat.ApplyDamageToPlayer(100, &error), error);
  Require(combat.Update(0.49f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  Require(!combat.player().alive,
          "player respawned before the deterministic delay");
  Require(combat.Update(0.02f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  Require(combat.player().alive && combat.player().health == 100 &&
              combat.events().playerRespawned,
          "player respawn did not restore valid full-health state");
}

void TestPlayerRayDamagesAndKillsBot() {
  CombatSandbox combat(OpenArena(), TestTuning());
  const glm::vec3 origin = combat.layout().playerSpawn;
  const glm::vec3 botCenter =
      combat.bots()[0].position + glm::vec3(0.0f, 1.0f, 0.0f);
  const std::optional<BotRayHit> hit =
      combat.RaycastBots(origin, glm::normalize(botCenter - origin), 30.0f);
  Require(hit && hit->botIndex == 0u,
          "valid authoritative player ray did not hit the nearest bot");
  std::string error;
  Require(combat.ApplyDamageToBot(hit->botIndex, 40.0f, &error), error);
  Require(Near(combat.bots()[0].health, 60.0f) && combat.bots()[0].alive,
          "valid player shot did not reduce independent bot health");
  Require(combat.ApplyDamageToBot(hit->botIndex, 60.0f, &error), error);
  Require(!combat.bots()[0].alive &&
              combat.bots()[0].state == BotAiState::Dead &&
              !combat.bots()[0].moving() && combat.events().botDied[0],
          "lethal player shot did not stop bot movement/combat");
}

void TestBotRespawnAndIndependentTimers() {
  CombatTuning tuning = TestTuning();
  tuning.sightRange = 0.1f;
  tuning.botRespawnSeconds = 0.5f;
  CombatSandbox combat(OpenArena(), tuning);
  const float secondCooldown = combat.bots()[1].weaponCooldown;
  std::string error;
  Require(combat.ApplyDamageToBot(0u, 100.0f, &error), error);
  Require(!combat.bots()[0].alive && combat.bots()[1].alive &&
              Near(combat.bots()[1].health, combat.bots()[1].maxHealth),
          "one bot death corrupted a separate bot instance");
  Require(combat.Update(0.2f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  Require(!combat.bots()[0].alive && combat.bots()[1].alive &&
              combat.bots()[1].weaponCooldown < secondCooldown,
          "separate bot timers did not advance independently");
  Require(combat.Update(0.31f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  Require(combat.bots()[0].alive &&
              Near(combat.bots()[0].health, combat.bots()[0].maxHealth) &&
              combat.events().botRespawned[0] &&
              combat.bots()[0].respawnedThisFrame,
          "bot respawn did not restore a valid independent state");
}

void TestBotAnimationUsesProductionRuntimeAndRemainsIndependent() {
  auto asset = std::make_shared<StwModel>();
  std::string error;
  Require(LoadGLTF(STW_TEST_GAMEPLAY_ASSET, *asset, &error), error);
  BotCombatState moving;
  moving.position = glm::vec3(0.0f, 0.3f, -6.0f);
  moving.velocity = glm::vec3(1.0f, 0.0f, 0.0f);
  moving.forward = glm::vec3(1.0f, 0.0f, 0.0f);
  BotCombatState idle = moving;
  idle.id = 1;
  idle.position.x = 3.0f;
  idle.velocity = glm::vec3(0.0f);

  CombatBotAnimationRuntime first;
  CombatBotAnimationRuntime second;
  Require(CombatBotAnimationRuntime::Create(asset, 0u, 1.75f, moving,
                                             first, &error), error);
  Require(CombatBotAnimationRuntime::Create(asset, 0u, 1.75f, idle,
                                             second, &error), error);
  Require(first.Update(moving, 0.1f, &error), error);
  Require(second.Update(idle, 0.1f, &error), error);
  Require(first.controller().state() == CharacterAnimationState::Move &&
              second.controller().state() == CharacterAnimationState::Idle &&
              first.instance().animator() != second.instance().animator() &&
              first.instance().palette() != second.instance().palette(),
          "two bots shared mutable animation runtime state");
  const glm::mat4& movingTransform = first.instance().transform();
  Require(Near(movingTransform[3].x, moving.position.x) &&
              Near(movingTransform[3].y, moving.position.y) &&
              Near(movingTransform[3].z, moving.position.z),
          "rendered bot instance did not use authoritative gameplay position");

  moving.firedThisFrame = true;
  Require(first.Update(moving, 0.0f, &error), error);
  Require(first.controller().state() == CharacterAnimationState::Fire,
          "authoritative bot fire did not drive the existing Fire animation");
  moving.firedThisFrame = false;
  Require(first.Update(moving, first.instance().animationDuration(), &error),
          error);
  Require(first.Update(moving, 0.0f, &error), error);
  Require(first.controller().state() == CharacterAnimationState::Move,
          "completed bot Fire did not return to Move while moving");
  moving.velocity = glm::vec3(0.0f);
  Require(first.Update(moving, 0.0f, &error), error);
  Require(first.controller().state() == CharacterAnimationState::Idle,
          "stopped bot did not return from Move to Idle");
}

void TestPatrolCollisionAndSteeringRemainStableOverTime() {
  const CombatArenaLayout layout = CoverTraversalArena();
  CombatTuning tuning = TestTuning();
  tuning.sightRange = 0.1f;
  tuning.botWalkSpeed = 2.25f;
  tuning.botRadius = 0.55f;
  CombatSandbox combat(layout, tuning);
  std::string error;
  glm::vec3 previous = combat.bots()[0].position;
  int lateralSign = 0;
  int lateralSignChanges = 0;
  float maximumStep = 0.0f;
  bool passedCover = false;
  for (int frame = 0; frame < 720; ++frame) {
    Require(combat.Update(1.0f / 60.0f, layout.playerSpawn,
                          glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
    const BotCombatState& bot = combat.bots()[0];
    const glm::vec3 displacement = bot.position - previous;
    const float distance = glm::length(displacement);
    maximumStep = std::max(maximumStep, distance);
    Require(Finite(bot.position) && Finite(bot.velocity) &&
                Finite(bot.forward),
            "patrolling bot produced a non-finite transform");
    Require(Near(bot.position.y, layout.botSpawns[0].position.y),
            "gameplay steering introduced vertical bot bobbing");
    Require(bot.position.x >= layout.minimum.x + tuning.botRadius - 1.0e-4f &&
                bot.position.x <= layout.maximum.x - tuning.botRadius + 1.0e-4f &&
                bot.position.z >= layout.minimum.y + tuning.botRadius - 1.0e-4f &&
                bot.position.z <= layout.maximum.y - tuning.botRadius + 1.0e-4f,
            "patrolling bot crossed the radius-safe arena boundary");
    Require(!InsideExpandedObstacle(bot.position, tuning.botRadius,
                                    layout.obstacles[0]),
            "patrolling bot ended inside major cover");

    if (!passedCover && bot.position.z > -6.0f && bot.position.z < -2.0f &&
        std::fabs(displacement.x) > 1.0e-4f) {
      const int nextSign = displacement.x > 0.0f ? 1 : -1;
      if (lateralSign != 0 && nextSign != lateralSign) ++lateralSignChanges;
      lateralSign = nextSign;
    }
    passedCover = passedCover || bot.position.z > -2.5f;
    previous = bot.position;
  }
  Require(maximumStep <= tuning.botWalkSpeed / 60.0f + 1.0e-4f,
          "patrolling bot made a teleport-sized frame displacement");
  Require(passedCover, "stable avoidance did not navigate around major cover");
  Require(lateralSignChanges <= 2,
          "cover approach alternated lateral steering and visibly oscillated");
}

void TestUnobstructedChaseGenerallyClosesDistance() {
  CombatArenaLayout layout = OpenArena();
  layout.botSpawns[0].position = glm::vec3(0.0f, 0.3f, -18.0f);
  layout.botSpawns[0].forward = glm::vec3(0.0f, 0.0f, 1.0f);
  layout.botSpawns[0].patrolPoints = {
      layout.botSpawns[0].position, layout.botSpawns[0].position};
  CombatTuning tuning = TestTuning();
  tuning.sightRange = 40.0f;
  tuning.attackRange = 2.0f;
  tuning.fieldOfViewDegrees = 180.0f;
  CombatSandbox combat(layout, tuning);
  const float initialDistance = glm::length(glm::vec2(
      combat.bots()[0].position.x - layout.playerSpawn.x,
      combat.bots()[0].position.z - layout.playerSpawn.z));
  std::string error;
  float previousDistance = initialDistance;
  int increasingFrames = 0;
  for (int frame = 0; frame < 180; ++frame) {
    Require(combat.Update(1.0f / 60.0f, layout.playerSpawn,
                          glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
    const float distance = glm::length(glm::vec2(
        combat.bots()[0].position.x - layout.playerSpawn.x,
        combat.bots()[0].position.z - layout.playerSpawn.z));
    if (distance > previousDistance + 1.0e-4f) ++increasingFrames;
    previousDistance = distance;
  }
  Require(previousDistance < initialDistance - 4.0f &&
              increasingFrames <= 1,
          "unobstructed chase failed to close player distance stably");
}

void TestDeadAndRespawningBotHasNoMovementSpike() {
  CombatTuning tuning = TestTuning();
  tuning.sightRange = 0.1f;
  tuning.botRespawnSeconds = 0.5f;
  CombatSandbox combat(OpenArena(), tuning);
  std::string error;
  const glm::vec3 deathPosition = combat.bots()[0].position;
  Require(combat.ApplyDamageToBot(0u, 1000.0f, &error), error);
  Require(combat.Update(0.25f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  Require(!combat.bots()[0].alive &&
              glm::length(combat.bots()[0].position - deathPosition) <= 1.0e-5f &&
              glm::length(combat.bots()[0].velocity) <= 1.0e-5f,
          "dead bot continued moving");
  Require(combat.Update(0.26f, combat.layout().playerSpawn,
                        glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
  const BotCombatState& respawned = combat.bots()[0];
  Require(respawned.alive && respawned.respawnedThisFrame &&
              glm::length(respawned.velocity) <= 1.0e-5f &&
              glm::length(respawned.position -
                          combat.layout().botSpawns[0].position) <= 1.0e-5f,
          "bot respawn applied movement or a transform spike in the same frame");
}

void TestArenaWallUsesRadiusSafeCollision() {
  CombatArenaLayout layout = OpenArena();
  layout.minimum = glm::vec2(-2.0f, -8.0f);
  layout.maximum = glm::vec2(2.0f, 2.0f);
  layout.botSpawns[0] = {
      glm::vec3(0.0f, 0.3f, -4.0f), glm::vec3(1.0f, 0.0f, 0.0f),
      {glm::vec3(0.0f, 0.3f, -4.0f),
       glm::vec3(20.0f, 0.3f, -4.0f)}};
  layout.botSpawns[1] = {
      glm::vec3(-1.0f, 0.3f, -7.0f), glm::vec3(0.0f, 0.0f, 1.0f),
      {glm::vec3(-1.0f, 0.3f, -7.0f),
       glm::vec3(-1.0f, 0.3f, -6.0f)}};
  layout.botSpawns[2] = {
      glm::vec3(1.0f, 0.3f, -7.0f), glm::vec3(0.0f, 0.0f, 1.0f),
      {glm::vec3(1.0f, 0.3f, -7.0f),
       glm::vec3(1.0f, 0.3f, -6.0f)}};
  CombatTuning tuning = TestTuning();
  tuning.sightRange = 0.1f;
  CombatSandbox combat(layout, tuning);
  std::string error;
  for (int frame = 0; frame < 600; ++frame) {
    Require(combat.Update(1.0f / 60.0f, layout.playerSpawn,
                          glm::vec3(0.0f, 0.0f, -1.0f), &error), error);
    Require(combat.bots()[0].position.x <=
                layout.maximum.x - tuning.botRadius + 1.0e-4f,
            "bot center allowed its collision radius through arena wall");
  }
}

void TestStaticModelRegression() {
  auto asset = std::make_shared<StwModel>();
  std::string error;
  Require(LoadGLTF(STW_TEST_STATIC_ASSET, *asset, &error), error);
  std::vector<RuntimeModelInstance> instances;
  Require(RuntimeModelInstance::CreateDefaultSet(asset, instances, &error),
          error);
  bool foundStatic = false;
  for (const RuntimeModelInstance& instance : instances) {
    if (!instance.skinned()) foundStatic = true;
  }
  Require(foundStatic,
          "static/non-bot model no longer uses the existing static runtime");
}

}  // namespace
}  // namespace stw

int main() {
  try {
    stw::TestPlayerStartsFullAndBotAcquiresWithLos();
    stw::TestBlockedLosPreventsAcquireAndFire();
    stw::TestBotFireDamagesPlayerAndDeathIsSingleTransition();
    stw::TestPlayerRespawnRestoresHealth();
    stw::TestPlayerRayDamagesAndKillsBot();
    stw::TestBotRespawnAndIndependentTimers();
    stw::TestBotAnimationUsesProductionRuntimeAndRemainsIndependent();
    stw::TestPatrolCollisionAndSteeringRemainStableOverTime();
    stw::TestUnobstructedChaseGenerallyClosesDistance();
    stw::TestDeadAndRespawningBotHasNoMovementSpike();
    stw::TestArenaWallUsesRadiusSafeCollision();
    stw::TestStaticModelRegression();
  } catch (const std::exception& exception) {
    std::cerr << "CombatLoopTests FAILED: " << exception.what() << '\n';
    return 1;
  }
  std::cout << "CombatLoopTests PASSED\n";
  return 0;
}
