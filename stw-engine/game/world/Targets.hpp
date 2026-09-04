#pragma once
// Singleplayer-Kette: Hitscan -> Damage -> Health -> Death -> Respawn.
// Erzeugt ausschließlich Events (engine/core/Events.hpp).
#include "../../engine/core/Events.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace stw {
struct Target {
    int id = 0;
    glm::vec3 pos{0.f, 1.f, 0.f};
    float hp = 100.f;
    bool alive = true;
    float respawnAt = -1.f;
};

class TargetWorld {
   public:
    explicit TargetWorld(int count = 5) {
        for (int i = 0; i < count; i++) {
            Target t;
            t.id = i;
            t.pos = spawnFor(i);
            targets_.push_back(t);
        }
    }
    const std::vector<Target>& targets() const { return targets_; }

    // Hitscan: nächste Kugel (r=0.8, h=2) entlang des Strahls
    std::optional<std::pair<int, glm::vec3>> hitscan(const glm::vec3& o, const glm::vec3& d, float maxRange) const {
        float best = maxRange;
        std::optional<std::pair<int, glm::vec3>> hit;
        for (const auto& t : targets_) {
            if (!t.alive) continue;
            // Ray vs vertikale Kapsel ~ Kugel in Brusthöhe
            glm::vec3 c = t.pos + glm::vec3(0, 0.2f, 0);
            glm::vec3 oc = o - c;
            float b = glm::dot(oc, d);
            float cq = glm::dot(oc, oc) - 0.8f * 0.8f;
            float disc = b * b - cq;
            if (disc < 0.f) continue;
            float tt = -b - sqrtf(disc);
            if (tt < 0.f || tt > best) continue;
            glm::vec3 hp = o + d * tt;
            if (hp.y < 0.f || hp.y > 2.f) continue;
            best = tt;
            hit = {{t.id, hp}};
        }
        return hit;
    }

    void applyDamage(int id, float dmg, FrameEvents& ev) {
        for (auto& t : targets_) {
            if (t.id != id || !t.alive) continue;
            t.hp -= dmg;
            ev.damage.push_back({id, dmg});
            if (t.hp <= 0.f) {
                t.alive = false;
                t.respawnAt = time_ + 3.f;
                ev.deaths.push_back({id});
            }
        }
    }

    void update(float dt, FrameEvents& ev) {
        time_ += dt;
        for (auto& t : targets_) {
            if (!t.alive && t.respawnAt >= 0.f && time_ >= t.respawnAt) {
                t.alive = true;
                t.hp = 100.f;
                t.pos = spawnFor(t.id);
                t.respawnAt = -1.f;
                ev.respawns.push_back({t.id, t.pos});
            }
        }
    }

   private:
    glm::vec3 spawnFor(int i) const {
        static const std::array<glm::vec3, 5> kTrainingLanes{
            glm::vec3(-6.4f, 1.0f, -14.0f),
            glm::vec3(-3.2f, 1.0f, -18.0f),
            glm::vec3(0.0f, 1.0f, -22.0f),
            glm::vec3(3.2f, 1.0f, -18.0f),
            glm::vec3(6.4f, 1.0f, -14.0f),
        };
        if (i >= 0 && static_cast<std::size_t>(i) < kTrainingLanes.size()) {
            return kTrainingLanes[static_cast<std::size_t>(i)];
        }
        return glm::vec3(0.0f, 1.0f, -22.0f - static_cast<float>(i));
    }
    std::vector<Target> targets_;
    float time_ = 0.f;
};
}  // namespace stw
