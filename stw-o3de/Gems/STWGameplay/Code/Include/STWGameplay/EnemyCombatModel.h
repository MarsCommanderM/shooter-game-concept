#pragma once

#include <AzCore/Math/Vector3.h>

namespace STWGameplay
{
    struct EnemyState
    {
        AZ::Vector3 m_position = AZ::Vector3(0.0f, 6.0f, 1.2f);
        float m_radius = 0.80f;
        float m_maxHealth = 100.0f;
        float m_health = 100.0f;
        bool m_alive = true;
        int m_damageEvents = 0;
        int m_deathEvents = 0;
        int m_respawnEvents = 0;
    };

    //! Authoritative, renderer-independent state for the single Block-7 enemy.
    class EnemyCombatModel
    {
    public:
        static constexpr float DetectionRadius = 20.0f;
        static constexpr float StopDistance = 6.0f;
        static constexpr float MoveSpeed = 1.5f;
        static const AZ::Vector3 SpawnPosition;

        bool Update(float deltaTime, const AZ::Vector3& playerPosition);
        bool ApplyDamage(float damage);
        void Reset();
        void SynchronizePhysicalPosition(const AZ::Vector3& position);
        AZ::Vector3 GetMovementIntent(const AZ::Vector3& playerPosition) const;

        const EnemyState& GetState() const { return m_state; }

    private:
        EnemyState m_state;
    };
}
