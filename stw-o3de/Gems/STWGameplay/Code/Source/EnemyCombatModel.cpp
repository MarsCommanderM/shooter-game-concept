#include <STWGameplay/EnemyCombatModel.h>
#include <STWGameplay/ArenaLayout.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    const AZ::Vector3 EnemyCombatModel::SpawnPosition = ArenaLayout::EnemySpawn;

    bool EnemyCombatModel::Update(float deltaTime, const AZ::Vector3& playerPosition, bool playerAlive)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f || !playerPosition.IsFinite())
        {
            return false;
        }
        m_attackCooldownRemaining = AZStd::max(0.0f, m_attackCooldownRemaining - deltaTime);
        if (!m_state.m_alive)
        {
            EnterState(EnemyBehaviorState::Dead);
            return true;
        }

        const float distance = DistanceToPlayer(playerPosition);
        if (!playerAlive || distance > DetectionRadius)
        {
            EnterState(EnemyBehaviorState::Idle);
            return true;
        }

        switch (m_state.m_behaviorState)
        {
        case EnemyBehaviorState::Idle:
            EnterState(EnemyBehaviorState::Detect);
            break;
        case EnemyBehaviorState::Detect:
            EnterState(distance <= AttackRange ? EnemyBehaviorState::Attack : EnemyBehaviorState::Chase);
            break;
        case EnemyBehaviorState::Chase:
            if (distance <= AttackRange)
            {
                EnterState(EnemyBehaviorState::Attack);
            }
            break;
        case EnemyBehaviorState::Attack:
            if (distance > AttackRange)
            {
                EnterState(EnemyBehaviorState::Chase);
            }
            break;
        case EnemyBehaviorState::Dead:
            EnterState(EnemyBehaviorState::Idle);
            break;
        }
        return true;
    }

    bool EnemyCombatModel::TryAttackPlayer()
    {
        if (!m_state.m_alive || m_state.m_behaviorState != EnemyBehaviorState::Attack || m_attackCooldownRemaining > 0.0f)
        {
            return false;
        }
        m_attackCooldownRemaining = AttackCooldown;
        ++m_state.m_attackEvents;
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
            EnterState(EnemyBehaviorState::Dead);
            ++m_state.m_deathEvents;
        }
        return true;
    }

    void EnemyCombatModel::Reset()
    {
        const int damageEvents = m_state.m_damageEvents;
        const int deathEvents = m_state.m_deathEvents;
        const int respawnEvents = m_state.m_respawnEvents + 1;
        const int detectionEvents = m_state.m_detectionEvents;
        const int chaseEvents = m_state.m_chaseEvents;
        const int attackEvents = m_state.m_attackEvents;
        m_state = {};
        m_state.m_position = SpawnPosition;
        m_state.m_damageEvents = damageEvents;
        m_state.m_deathEvents = deathEvents;
        m_state.m_respawnEvents = respawnEvents;
        m_state.m_detectionEvents = detectionEvents;
        m_state.m_chaseEvents = chaseEvents;
        m_state.m_attackEvents = attackEvents;
        m_attackCooldownRemaining = 0.0f;
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
        if (!m_state.m_alive || m_state.m_behaviorState != EnemyBehaviorState::Chase || !playerPosition.IsFinite())
        {
            return AZ::Vector3::CreateZero();
        }
        AZ::Vector3 offset = playerPosition - m_state.m_position;
        offset.SetZ(0.0f);
        const float distance = offset.GetLength();
        if (distance <= AttackRange || distance > DetectionRadius || distance <= 0.001f)
        {
            return AZ::Vector3::CreateZero();
        }
        return offset / distance * MoveSpeed;
    }

    void EnemyCombatModel::EnterState(EnemyBehaviorState state)
    {
        if (m_state.m_behaviorState == state)
        {
            return;
        }
        m_state.m_behaviorState = state;
        if (state == EnemyBehaviorState::Detect)
        {
            ++m_state.m_detectionEvents;
        }
        else if (state == EnemyBehaviorState::Chase)
        {
            ++m_state.m_chaseEvents;
        }
    }

    float EnemyCombatModel::DistanceToPlayer(const AZ::Vector3& playerPosition) const
    {
        AZ::Vector3 offset = playerPosition - m_state.m_position;
        offset.SetZ(0.0f);
        return offset.GetLength();
    }
}
