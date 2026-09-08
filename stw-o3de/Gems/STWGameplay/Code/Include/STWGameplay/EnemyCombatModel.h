#pragma once

#include <cstddef>
#include <AzCore/base.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>

namespace STWGameplay
{
    using EnemyId = AZ::u32;
    inline constexpr EnemyId InvalidEnemyId = 0;
    inline constexpr EnemyId PrimaryEnemyId = 1;

    enum class EnemyArchetype : AZ::u8
    {
        Assault = 0,
        Heavy,
        Skirmisher
    };

    struct EnemyProfile
    {
        EnemyArchetype m_archetype = EnemyArchetype::Assault;
        const char* m_identifier = "ASSAULT";
        float m_maxHealth = 100.0f;
        float m_moveSpeed = 1.5f;
        float m_detectionRadius = 20.0f;
        float m_attackRange = 2.5f;
        float m_attackCooldown = 1.0f;
        float m_attackDamage = 20.0f;
        float m_presentationScale = 1.0f;
    };

    inline constexpr size_t EnemyProfileCount = 3;

    const EnemyProfile& GetEnemyProfile(EnemyArchetype archetype);

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
        EnemyId m_id = PrimaryEnemyId;
        EnemyArchetype m_archetype = EnemyArchetype::Assault;
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
        float m_presentationScale = 1.0f;
    };

    //! Authoritative, renderer-independent state for one deterministic enemy instance.
    class EnemyCombatModel
    {
    public:
        static constexpr float DetectionRadius = 20.0f;
        static constexpr float AttackRange = 2.5f;
        static constexpr float MoveSpeed = 1.5f;
        static constexpr float AttackDamage = 20.0f;
        static constexpr float AttackCooldown = 1.0f;
        static const AZ::Vector3 SpawnPosition;

        EnemyCombatModel();
        bool Configure(EnemyId id, const EnemyProfile& profile, const AZ::Vector3& spawnPosition);
        bool Update(float deltaTime, const AZ::Vector3& playerPosition, bool playerAlive = true);
        bool TryAttackPlayer();
        bool ApplyDamage(float damage);
        void Reset();
        void SynchronizePhysicalPosition(const AZ::Vector3& position);
        AZ::Vector3 GetMovementIntent(const AZ::Vector3& playerPosition) const;

        const EnemyState& GetState() const { return m_state; }
        const EnemyProfile& GetProfile() const { return m_profile; }
        EnemyId GetId() const { return m_state.m_id; }

    private:
        void EnterState(EnemyBehaviorState state);
        float DistanceToPlayer(const AZ::Vector3& playerPosition) const;

        EnemyState m_state;
        EnemyProfile m_profile;
        AZ::Vector3 m_spawnPosition = SpawnPosition;
        float m_attackCooldownRemaining = 0.0f;
    };
}
