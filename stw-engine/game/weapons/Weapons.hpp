#pragma once
// WeaponSystem: reine Daten + Cooldown + Events. Kein Rendering.
#include "../../engine/core/Events.hpp"

#include <optional>

namespace stw {
struct WeaponSpec {
    const char* name;
    float rpm;
    float damage;
    float range;
    float cooldown() const { return 60.0f / rpm; }
};
// Dieselbe Tabelle wird später NOVA-core spiegeln (Single Source of Truth pro Build-Target)
inline constexpr WeaponSpec kWeapons[] = {
    {"M16", 720.f, 26.f, 120.f},
    {"AK-47", 600.f, 34.f, 140.f},
    {"MP5", 800.f, 16.f, 60.f},
    {"MG4", 650.f, 24.f, 160.f},
    {"PUMP", 75.f, 100.f, 40.f},
};

class WeaponSystem {
   public:
    void update(float dt) { cd_ -= dt; }
    void cycle() {
        idx_ = (idx_ + 1) % 5;
        cd_ = 0.25f;
    }
    int current() const { return idx_; }
    const WeaponSpec& spec() const { return kWeapons[idx_]; }

    // erzeugt WeaponFiredEvent; Hitscan/Damage macht die World-Schicht
    std::optional<WeaponFiredEvent> tryFire(bool held, const glm::vec3& origin, const glm::vec3& dir, FrameEvents& ev) {
        if (!held || cd_ > 0.f) return std::nullopt;
        cd_ = spec().cooldown();
        WeaponFiredEvent e{origin, dir, idx_};
        ev.fired.push_back(e);
        return e;
    }

   private:
    int idx_ = 0;
    float cd_ = 0.f;
};
}  // namespace stw
