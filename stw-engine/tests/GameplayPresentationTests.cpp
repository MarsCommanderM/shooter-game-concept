#include "Events.hpp"
#include "Targets.hpp"
#include "Weapons.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>

namespace stw {
namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

WeaponFiredEvent Fire(WeaponSystem& weapon, FrameEvents& events) {
  const auto fired = weapon.tryFire(
      true, glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), events);
  Require(fired.has_value(), "expected a valid weapon fire event");
  return *fired;
}

void TestFireConsumesMagazineAndRespectsCooldown() {
  WeaponSystem weapon;
  FrameEvents events;
  const int initialMagazine = weapon.magazineAmmo();

  const WeaponFiredEvent event = Fire(weapon, events);
  Require(event.weapon == weapon.current() && events.fired.size() == 1u,
          "valid fire did not publish the authoritative event");
  Require(weapon.magazineAmmo() == initialMagazine - 1,
          "valid fire did not consume exactly one cartridge");
  Require(!weapon.tryFire(true, glm::vec3(0.0f),
                          glm::vec3(0.0f, 0.0f, -1.0f), events),
          "cooldown allowed a duplicate fire event");

  weapon.update(weapon.spec().cooldown());
  Fire(weapon, events);
  Require(weapon.magazineAmmo() == initialMagazine - 2,
          "fire after cooldown did not consume one cartridge");
}

void TestReloadIsDelayedAndConservesAmmo() {
  WeaponSystem weapon;
  FrameEvents events;
  Fire(weapon, events);
  const int magazineBefore = weapon.magazineAmmo();
  const int reserveBefore = weapon.reserveAmmo();

  Require(weapon.startReload(), "a partially used magazine did not start reload");
  Require(weapon.isReloading(), "reload state was not exposed");
  Require(!weapon.startReload(), "duplicate reload was accepted");
  Require(!weapon.tryFire(true, glm::vec3(0.0f),
                          glm::vec3(0.0f, 0.0f, -1.0f), events),
          "weapon fired during reload");

  weapon.update(weapon.spec().reloadSeconds * 0.5f);
  Require(weapon.magazineAmmo() == magazineBefore &&
              weapon.reserveAmmo() == reserveBefore,
          "reload transferred ammunition before completion");
  Require(weapon.reloadProgress() > 0.0f && weapon.reloadProgress() < 1.0f,
          "reload progress was not meaningful while reloading");

  weapon.update(weapon.spec().reloadSeconds);
  Require(!weapon.isReloading(), "completed reload remained active");
  Require(weapon.magazineAmmo() == weapon.spec().magazineCapacity,
          "reload did not fill the current magazine");
  Require(weapon.reserveAmmo() == reserveBefore - 1,
          "reload did not conserve total ammunition");
}

void TestSwitchPreservesIndependentAmmoAndCancelsReload() {
  WeaponSystem weapon;
  FrameEvents events;
  Fire(weapon, events);
  const WeaponAmmoState firstState = weapon.ammoState(0u);
  Require(weapon.startReload(), "reload setup failed");

  weapon.cycle();
  Require(weapon.current() == 1 && !weapon.isReloading(),
          "weapon switch did not cancel reload deterministically");
  Require(weapon.magazineAmmo() == kWeapons[1].magazineCapacity,
          "new weapon did not retain its own magazine state");
  weapon.update(0.25f);
  Fire(weapon, events);
  const WeaponAmmoState secondState = weapon.ammoState(1u);

  for (std::size_t index = 1; index < kWeaponCount; ++index) weapon.cycle();
  Require(weapon.current() == 0, "weapon cycle did not wrap deterministically");
  Require(weapon.ammoState(0u).magazine == firstState.magazine &&
              weapon.ammoState(0u).reserve == firstState.reserve,
          "switching corrupted the first weapon ammunition");
  Require(weapon.ammoState(1u).magazine == secondState.magazine &&
              weapon.ammoState(1u).reserve == secondState.reserve,
          "switching corrupted the second weapon ammunition");
}

void TestFullMagazineAndEmptyMagazineGuards() {
  WeaponSystem weapon;
  Require(!weapon.startReload(), "full magazine incorrectly started reload");

  FrameEvents events;
  for (int shot = 0; shot < weapon.spec().magazineCapacity; ++shot) {
    Fire(weapon, events);
    weapon.update(weapon.spec().cooldown());
  }
  Require(weapon.magazineAmmo() == 0, "test did not empty the magazine");
  Require(!weapon.tryFire(true, glm::vec3(0.0f),
                          glm::vec3(0.0f, 0.0f, -1.0f), events),
          "empty weapon generated a fire event");
}

void TestTrainingTargetDamageDeathAndRespawn() {
  TargetWorld world(1);
  FrameEvents events;
  Require(world.targets().size() == 1u && world.targets()[0].alive,
          "training target did not spawn alive");
  world.applyDamage(world.targets()[0].id, 100.0f, events);
  Require(!world.targets()[0].alive && events.deaths.size() == 1u,
          "lethal damage did not produce target death");

  events.clear();
  world.update(2.9f, events);
  Require(!world.targets()[0].alive && events.respawns.empty(),
          "target respawned before its deterministic delay");
  world.update(0.2f, events);
  Require(world.targets()[0].alive && world.targets()[0].hp == 100.0f &&
              events.respawns.size() == 1u,
          "target did not respawn with full health after its delay");
}

}  // namespace
}  // namespace stw

int main() {
  try {
    stw::TestFireConsumesMagazineAndRespectsCooldown();
    stw::TestReloadIsDelayedAndConservesAmmo();
    stw::TestSwitchPreservesIndependentAmmoAndCancelsReload();
    stw::TestFullMagazineAndEmptyMagazineGuards();
    stw::TestTrainingTargetDamageDeathAndRespawn();
  } catch (const std::exception& error) {
    std::cerr << "Gameplay presentation test failed: " << error.what() << '\n';
    return 1;
  }
  std::cout << "Gameplay presentation tests passed\n";
  return 0;
}
