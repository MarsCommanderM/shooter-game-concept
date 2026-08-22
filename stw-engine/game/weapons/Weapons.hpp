#pragma once
// WeaponSystem: reine Daten + Cooldown/Magazin/Reload + Events. Kein Rendering.
#include "../../engine/core/Events.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace stw {
struct WeaponSpec {
    const char* name;
    float rpm;
    float damage;
    float range;
    int magazineCapacity;
    int reserveCapacity;
    float reloadSeconds;
    float cooldown() const { return 60.0f / rpm; }
};
// Dieselbe Tabelle wird später NOVA-core spiegeln (Single Source of Truth pro Build-Target)
inline constexpr WeaponSpec kWeapons[] = {
    {"M16", 720.f, 26.f, 120.f, 30, 120, 1.90f},
    {"AK-47", 600.f, 34.f, 140.f, 30, 120, 2.25f},
    {"MP5", 800.f, 16.f, 60.f, 30, 150, 1.75f},
    {"MG4", 650.f, 24.f, 160.f, 100, 200, 3.80f},
    {"PUMP", 75.f, 100.f, 40.f, 8, 40, 2.80f},
};

inline constexpr std::size_t kWeaponCount =
    sizeof(kWeapons) / sizeof(kWeapons[0]);

struct WeaponAmmoState {
    int magazine = 0;
    int reserve = 0;
};

class WeaponSystem {
   public:
    WeaponSystem() {
        for (std::size_t index = 0; index < kWeaponCount; ++index) {
            ammo_[index] = {kWeapons[index].magazineCapacity,
                            kWeapons[index].reserveCapacity};
        }
    }

    void update(float dt) {
        if (!std::isfinite(static_cast<double>(dt)) || dt <= 0.0f) return;
        cd_ = std::max(0.0f, cd_ - dt);
        if (!reloading_) return;

        reloadRemaining_ = std::max(0.0f, reloadRemaining_ - dt);
        if (reloadRemaining_ > 0.0f) return;

        WeaponAmmoState& state = ammo_[static_cast<std::size_t>(idx_)];
        const int needed = spec().magazineCapacity - state.magazine;
        const int transferred = std::min(needed, state.reserve);
        state.magazine += transferred;
        state.reserve -= transferred;
        reloading_ = false;
    }

    void cycle() {
        cancelReload();
        idx_ = (idx_ + 1) % static_cast<int>(kWeaponCount);
        cd_ = 0.25f;
    }
    int current() const { return idx_; }
    const WeaponSpec& spec() const { return kWeapons[idx_]; }
    int magazineAmmo() const {
        return ammo_[static_cast<std::size_t>(idx_)].magazine;
    }
    int reserveAmmo() const {
        return ammo_[static_cast<std::size_t>(idx_)].reserve;
    }
    const WeaponAmmoState& ammoState(std::size_t weaponIndex) const {
        return ammo_.at(weaponIndex);
    }
    bool isReloading() const { return reloading_; }
    float reloadRemaining() const { return reloadRemaining_; }
    float reloadProgress() const {
        if (!reloading_ || spec().reloadSeconds <= 0.0f) return 0.0f;
        return std::clamp(1.0f - reloadRemaining_ / spec().reloadSeconds,
                          0.0f, 1.0f);
    }

    bool startReload() {
        WeaponAmmoState& state = ammo_[static_cast<std::size_t>(idx_)];
        if (reloading_ || state.magazine >= spec().magazineCapacity ||
            state.reserve <= 0) {
            return false;
        }
        reloading_ = true;
        reloadRemaining_ = spec().reloadSeconds;
        return true;
    }

    // erzeugt WeaponFiredEvent; Hitscan/Damage macht die World-Schicht
    std::optional<WeaponFiredEvent> tryFire(bool held, const glm::vec3& origin, const glm::vec3& dir, FrameEvents& ev) {
        WeaponAmmoState& state = ammo_[static_cast<std::size_t>(idx_)];
        if (!held || cd_ > 0.f || reloading_ || state.magazine <= 0) {
            return std::nullopt;
        }
        --state.magazine;
        cd_ = spec().cooldown();
        WeaponFiredEvent e{origin, dir, idx_};
        ev.fired.push_back(e);
        return e;
    }

   private:
    void cancelReload() {
        reloading_ = false;
        reloadRemaining_ = 0.0f;
    }

    std::array<WeaponAmmoState, kWeaponCount> ammo_{};
    int idx_ = 0;
    float cd_ = 0.f;
    bool reloading_ = false;
    float reloadRemaining_ = 0.0f;
};
}  // namespace stw
