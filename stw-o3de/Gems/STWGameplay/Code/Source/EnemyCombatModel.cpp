#include <STWGameplay/EnemyCombatModel.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    const AZ::Vector3 EnemyCombatModel::SpawnPosition(0.0f, 6.0f, 1.2f);

    bool EnemyCombatModel::Update(float deltaTime, const AZ::Vector3& playerPosition)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f || !playerPosition.IsFinite())
        {
            return false;
        }
        if (m_state.m_alive)
        {
            m_state.m_position += GetMovementIntent(playerPosition) * deltaTime;
        }
        return true;
    }

    bool EnemyCombatModel::ApplyDamage(float damage)
    {
        if (!m_state.m_alive || !std::isfinite(damage) || damage <= 0.0f)
        {
            return false;
        }
        m_state.m_health = AZStd::max(0.0f, m_state.m_health - damage);
        ++m_state.m_damageEvents;
        if (m_state.m_health <= 0.0f)
        {
            m_state.m_alive = false;
            ++m_state.m_deathEvents;
        }
        return true;
    }

    void EnemyCombatModel::Reset()
    {
        const int damageEvents = m_state.m_damageEvents;
        const int deathEvents = m_state.m_deathEvents;
        const int respawnEvents = m_state.m_respawnEvents + 1;
        m_state = {};
        m_state.m_position = SpawnPosition;
        m_state.m_damageEvents = damageEvents;
        m_state.m_deathEvents = deathEvents;
        m_state.m_respawnEvents = respawnEvents;
    }

    void EnemyCombatModel::SynchronizePhysicalPosition(const AZ::Vector3& position)
    {
        if (position.IsFinite())
        {
            m_state.m_position = position;
        }
    }

    AZ::Vector3 EnemyCombatModel::GetMovementIntent(const AZ::Vector3& playerPosition) const
    {
        if (!m_state.m_alive || !playerPosition.IsFinite())
        {
            return AZ::Vector3::CreateZero();
        }
        AZ::Vector3 offset = playerPosition - m_state.m_position;
        offset.SetZ(0.0f);
        const float distance = offset.GetLength();
        if (distance <= StopDistance || distance > DetectionRadius || distance <= 0.001f)
        {
            return AZ::Vector3::CreateZero();
        }
        return offset / distance * MoveSpeed;
    }
}
