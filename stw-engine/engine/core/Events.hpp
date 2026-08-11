#pragma once
// Engine/Game-Trennung: Gameplay erzeugt NUR Events, Renderer reagiert darauf.
// Kein WeaponSystem darf je OpenGL anfassen.
#include <glm/glm.hpp>

#include <vector>

namespace stw {
struct WeaponFiredEvent {
    glm::vec3 origin;
    glm::vec3 dir;
    int weapon = 0;
};
struct HitEvent {
    glm::vec3 pos;
    int targetId = -1;
};
struct DamageEvent {
    int targetId = -1;
    float dmg = 0.f;
};
struct DeathEvent {
    int targetId = -1;
};
struct RespawnEvent {
    int targetId = -1;
    glm::vec3 pos;
};

struct FrameEvents {
    std::vector<WeaponFiredEvent> fired;
    std::vector<HitEvent> hits;
    std::vector<DamageEvent> damage;
    std::vector<DeathEvent> deaths;
    std::vector<RespawnEvent> respawns;
    void clear() {
        fired.clear();
        hits.clear();
        damage.clear();
        deaths.clear();
        respawns.clear();
    }
};
}  // namespace stw
