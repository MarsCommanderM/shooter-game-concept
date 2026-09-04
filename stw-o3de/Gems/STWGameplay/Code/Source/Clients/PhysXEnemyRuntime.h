#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <STWGameplay/EnemyCombatModel.h>

namespace AZ { class Entity; }

namespace STWGameplay
{
    class PhysXEnemyRuntime final
    {
    public:
        PhysXEnemyRuntime();
        ~PhysXEnemyRuntime();
        static constexpr float CapsuleHeight = 2.10f;
        static constexpr float CapsuleRadius = 0.58f;
        static constexpr float CenterHeight = 1.20f;

        bool Initialize(const AZ::Vector3& centerPosition);
        bool Initialize(EnemyId enemyId, const AZ::Vector3& centerPosition);
        void Shutdown();
        bool QueueVelocity(const AZ::Vector3& velocity);
        bool Synchronize(AZ::Vector3& centerPosition, bool& grounded) const;
        bool ResetPosition(const AZ::Vector3& centerPosition);
        bool IsValid() const;
        EnemyId GetBoundEnemyId() const { return m_enemyId; }

    private:
        AZStd::unique_ptr<AZ::Entity> m_enemyEntity;
        EnemyId m_enemyId = InvalidEnemyId;
    };
}
