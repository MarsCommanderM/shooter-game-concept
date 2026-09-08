#include <STWGameplay/EnemyCombatModel.h>
#include <STWGameplay/ArenaLayout.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        const AZStd::array<EnemyProfile, EnemyProfileCount> s_enemyProfiles = {
            EnemyProfile{ EnemyArchetype::Assault, "ASSAULT", 100.0f, 1.5f, 20.0f, 2.5f, 1.0f, 20.0f, 1.0f },
            EnemyProfile{ EnemyArchetype::Heavy, "HEAVY", 180.0f, 1.0f, 18.0f, 2.8f, 1.4f, 28.0f, 1.20f },
            EnemyProfile{ EnemyArchetype::Skirmisher, "SKIRMISHER", 75.0f, 2.2f, 20.0f, 2.0f, 0.65f, 12.0f, 0.90f }
        };
    }

    const AZ::Vector3 EnemyCombatModel::SpawnPosition = ArenaLayout::EnemySpawn;

    const EnemyProfile& GetEnemyProfile(EnemyArchetype archetype)
    {
        const size_t index = static_cast<size_t>(archetype);
        return s_enemyProfiles[index < EnemyProfileCount ? index : 0];
    }

    EnemyCombatModel::EnemyCombatModel()
    {
        Configure(PrimaryEnemyId, GetEnemyProfile(EnemyArchetype::Assault), SpawnPosition);
    }

    bool EnemyCombatModel::Configure(EnemyId id, const EnemyProfile& profile, const AZ::Vector3& spawnPosition)
    {
        if (id == InvalidEnemyId || !spawnPosition.IsFinite()
            || !std::isfinite(profile.m_maxHealth) || profile.m_maxHealth <= 0.0f
            || !std::isfinite(profile.m_moveSpeed) || profile.m_moveSpeed < 0.0f
            || !std::isfinite(profile.m_detectionRadius) || profile.m_detectionRadius <= 0.0f
            || !std::isfinite(profile.m_attackRange) || profile.m_attackRange <= 0.0f
            || !std::isfinite(profile.m_attackCooldown) || profile.m_attackCooldown < 0.0f
            || !std::isfinite(profile.m_attackDamage) || profile.m_attackDamage <= 0.0f
            || !std::isfinite(profile.m_presentationScale) || profile.m_presentationScale <= 0.0f)
        {
            return false;
        }

        m_profile = profile;
        m_spawnPosition = spawnPosition;
        m_state = {};
        m_state.m_id = id;
        m_state.m_archetype = profile.m_archetype;
        m_state.m_position = spawnPosition;
        m_state.m_maxHealth = profile.m_maxHealth;
        m_state.m_health = profile.m_maxHealth;
        m_state.m_presentationScale = profile.m_presentationScale;
        m_attackCooldownRemaining = 0.0f;
        return true;
    }

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
        if (!playerAlive || distance > m_profile.m_detectionRadius)
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
            EnterState(distance <= m_profile.m_attackRange ? EnemyBehaviorState::Attack : EnemyBehaviorState::Chase);
            break;
        case EnemyBehaviorState::Chase:
            if (distance <= m_profile.m_attackRange)
            {
                EnterState(EnemyBehaviorState::Attack);
            }
            break;
        case EnemyBehaviorState::Attack:
            if (distance > m_profile.m_attackRange)
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
        m_attackCooldownRemaining = m_profile.m_attackCooldown;
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
        const EnemyId id = m_state.m_id;
        const EnemyArchetype archetype = m_state.m_archetype;
        const int damageEvents = m_state.m_damageEvents;
        const int deathEvents = m_state.m_deathEvents;
        const int respawnEvents = m_state.m_respawnEvents + 1;
        const int detectionEvents = m_state.m_detectionEvents;
        const int chaseEvents = m_state.m_chaseEvents;
        const int attackEvents = m_state.m_attackEvents;
        m_state = {};
        m_state.m_id = id;
        m_state.m_archetype = archetype;
        m_state.m_position = m_spawnPosition;
        m_state.m_maxHealth = m_profile.m_maxHealth;
        m_state.m_health = m_profile.m_maxHealth;
        m_state.m_presentationScale = m_profile.m_presentationScale;
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
        if (distance <= m_profile.m_attackRange || distance > m_profile.m_detectionRadius || distance <= 0.001f)
        {
            return AZ::Vector3::CreateZero();
        }
        return offset / distance * m_profile.m_moveSpeed;
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
