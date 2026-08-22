#include "game/CombatBotAnimationRuntime.hpp"
#include "game/CombatLoop.hpp"

#include "assets/StwcFormat.hpp"
#include "assets/gltf.h"
#include "runtime/ModelInstance.hpp"

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
    stw::TestStaticModelRegression();
  } catch (const std::exception& exception) {
    std::cerr << "CombatLoopTests FAILED: " << exception.what() << '\n';
    return 1;
  }
  std::cout << "CombatLoopTests PASSED\n";
  return 0;
}
