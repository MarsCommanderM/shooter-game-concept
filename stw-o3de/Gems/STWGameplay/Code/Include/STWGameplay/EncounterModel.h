#pragma once

#include <STWGameplay/EnemyCombatModel.h>

namespace STWGameplay
{
    enum class EncounterPhase
    {
        Active,
        Completed
    };

    //! Deterministic objective state driven by authoritative enemy lifecycle counters.
    //! This model never owns or mutates enemy, player, physics, weapon, or presentation state.
    class EncounterModel
    {
    public:
        bool Update(const EnemyState& enemyState);
        bool Rearm(const EnemyState& enemyState);

        EncounterPhase GetPhase() const { return m_phase; }
        bool IsActive() const { return m_phase == EncounterPhase::Active; }
        bool IsCompleted() const { return m_phase == EncounterPhase::Completed; }
        int GetCompletedCount() const { return m_completedCount; }

    private:
        EncounterPhase m_phase = EncounterPhase::Active;
        int m_completedCount = 0;
        int m_observedDeathEvents = 0;
    };
}
