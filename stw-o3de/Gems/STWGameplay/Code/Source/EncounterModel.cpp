#include <STWGameplay/EncounterModel.h>

namespace STWGameplay
{
    bool EncounterModel::Update(const EnemyState& enemyState)
    {
        if (enemyState.m_deathEvents < m_legacyObservedDeathEvents)
        {
            return false;
        }

        const bool newElimination = !enemyState.m_alive && enemyState.m_deathEvents > m_legacyObservedDeathEvents;
        m_legacyObservedDeathEvents = enemyState.m_deathEvents;
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
            || enemyState.m_deathEvents < m_legacyObservedDeathEvents)
        {
            return false;
        }

        m_legacyObservedDeathEvents = enemyState.m_deathEvents;
        m_phase = EncounterPhase::Active;
        return true;
    }

    bool EncounterModel::Update(const EnemyCollectionModel& enemies)
    {
        if (enemies.GetRequiredEnemyCount() == 0 || enemies.GetRequiredEnemyCount() > EnemyCollectionModel::MaxEnemyCount)
        {
            return false;
        }

        bool newElimination = false;
        bool allRequiredEliminated = true;
        size_t requiredIndex = 0;
        for (size_t index = 0; index < enemies.GetEnemyCount(); ++index)
        {
            const EnemyInstance& instance = enemies.GetInstanceByIndex(index);
            if (!instance.m_required)
            {
                continue;
            }
            const EnemyState& state = instance.m_combat.GetState();
            if (state.m_deathEvents < m_observedDeathEvents[requiredIndex])
            {
                return false;
            }
            newElimination = newElimination
                || (!state.m_alive && state.m_deathEvents > m_observedDeathEvents[requiredIndex]);
            m_observedDeathEvents[requiredIndex] = state.m_deathEvents;
            allRequiredEliminated = allRequiredEliminated && !state.m_alive && state.m_deathEvents > 0;
            ++requiredIndex;
        }

        if (requiredIndex != enemies.GetRequiredEnemyCount())
        {
            return false;
        }
        if (m_phase == EncounterPhase::Active && allRequiredEliminated && newElimination)
        {
            m_phase = EncounterPhase::Completed;
            ++m_completedCount;
        }
        return true;
    }

    bool EncounterModel::Rearm(const EnemyCollectionModel& enemies)
    {
        if (m_phase != EncounterPhase::Completed || !enemies.AreRequiredEnemiesAlive())
        {
            return false;
        }

        size_t requiredIndex = 0;
        for (size_t index = 0; index < enemies.GetEnemyCount(); ++index)
        {
            const EnemyInstance& instance = enemies.GetInstanceByIndex(index);
            if (instance.m_required)
            {
                const EnemyState& state = instance.m_combat.GetState();
                if (state.m_deathEvents < m_observedDeathEvents[requiredIndex])
                {
                    return false;
                }
                m_observedDeathEvents[requiredIndex] = state.m_deathEvents;
                ++requiredIndex;
            }
        }
        if (requiredIndex != enemies.GetRequiredEnemyCount())
        {
            return false;
        }
        m_phase = EncounterPhase::Active;
        return true;
    }
}
