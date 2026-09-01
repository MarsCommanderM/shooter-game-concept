#pragma once

#include <AzCore/Math/Vector3.h>

namespace STWGameplay
{
    enum class EnemyBehaviorState
    {
        Idle,
        Detect,
        Chase,
        Attack,
        Dead
    };

    struct EnemyState
    {
        AZ::Vector3 m_position = AZ::Vector3(0.0f, 6.0f, 1.2f);
        float m_radius = 0.80f;
        float m_maxHealth = 100.0f;
        float m_health = 100.0f;
        bool m_alive = true;
        EnemyBehaviorState m_behaviorState = EnemyBehaviorState::Idle;
        int m_damageEvents = 0;
        int m_deathEvents = 0;
        int m_respawnEvents = 0;
        int m_detectionEvents = 0;
        int m_chaseEvents = 0;
        int m_attackEvents = 0;
    };

    //! Authoritative, renderer-independent state for the single Block-7 enemy.
    class EnemyCombatModel
    {
    public:
        static constexpr float DetectionRadius = 20.0f;
        static constexpr float AttackRange = 2.5f;
        static constexpr float MoveSpeed = 1.5f;
        static constexpr float AttackDamage = 20.0f;
        static constexpr float AttackCooldown = 1.0f;
        static const AZ::Vector3 SpawnPosition;

        bool Update(float deltaTime, const AZ::Vector3& playerPosition, bool playerAlive = true);
        bool TryAttackPlayer();
        bool ApplyDamage(float damage);
        void Reset();
        void SynchronizePhysicalPosition(const AZ::Vector3& position);
        AZ::Vector3 GetMovementIntent(const AZ::Vector3& playerPosition) const;

        const EnemyState& GetState() const { return m_state; }

    private:
        void EnterState(EnemyBehaviorState state);
        float DistanceToPlayer(const AZ::Vector3& playerPosition) const;

        EnemyState m_state;
        float m_attackCooldownRemaining = 0.0f;
    };
}
