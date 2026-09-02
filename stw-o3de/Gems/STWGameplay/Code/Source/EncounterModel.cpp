#include <STWGameplay/EncounterModel.h>

namespace STWGameplay
{
    bool EncounterModel::Update(const EnemyState& enemyState)
    {
        if (enemyState.m_deathEvents < m_observedDeathEvents)
        {
            return false;
        }

        const bool newElimination = !enemyState.m_alive && enemyState.m_deathEvents > m_observedDeathEvents;
        m_observedDeathEvents = enemyState.m_deathEvents;
        if (!newElimination || m_phase != EncounterPhase::Active)
        {
            return true;
        }

        m_phase = EncounterPhase::Completed;
        ++m_completedCount;
        return true;
    }

    bool EncounterModel::Rearm(const EnemyState& enemyState)
    {
        if (m_phase != EncounterPhase::Completed || !enemyState.m_alive
            || enemyState.m_deathEvents < m_observedDeathEvents)
        {
            return false;
        }

        m_observedDeathEvents = enemyState.m_deathEvents;
        m_phase = EncounterPhase::Active;
        return true;
    }
}
