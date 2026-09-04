#pragma once

#include <cstddef>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>
#include <STWGameplay/EnemyCombatModel.h>

namespace STWGameplay
{
    struct EnemyInstance
    {
        EnemyId m_id = InvalidEnemyId;
        bool m_required = false;
        AZ::Vector3 m_spawnPosition = AZ::Vector3::CreateZero();
        EnemyCombatModel m_combat;
    };

    //! Deterministic owner/registry for independent EnemyCombatModel instances.
    //! The registry owns identity and membership; combat state remains in each model.
    class EnemyCollectionModel
    {
    public:
        static constexpr size_t RequiredEnemyCount = 3;
        static constexpr size_t MaxEnemyCount = 8;

        EnemyCollectionModel();

        bool InitializeDefaultEncounter();
        bool AddEnemy(EnemyId id, EnemyArchetype archetype, const AZ::Vector3& spawnPosition,
            bool required = false);

        bool Update(float deltaTime, const AZ::Vector3& playerPosition, bool playerAlive = true);
        bool ApplyDamage(EnemyId id, float damage);
        bool TryAttackPlayer(EnemyId id);
        bool ResetEnemy(EnemyId id);
        bool ResetRequiredEnemies();
        bool SynchronizePhysicalPosition(EnemyId id, const AZ::Vector3& position);

        size_t GetEnemyCount() const { return m_enemyCount; }
        size_t GetRequiredEnemyCount() const { return m_requiredEnemyCount; }
        size_t GetAliveCount() const;
        bool AreRequiredEnemiesAlive() const;
        bool AreRequiredEnemiesEliminated() const;
        bool IsRequiredEnemy(EnemyId id) const;

        const EnemyInstance* Find(EnemyId id) const;
        EnemyInstance* Find(EnemyId id);
        const EnemyInstance& GetInstanceByIndex(size_t index) const { return m_instances[index]; }
        EnemyInstance& GetInstanceByIndex(size_t index) { return m_instances[index]; }
        const EnemyCombatModel* GetEnemy(EnemyId id) const;
        EnemyCombatModel* GetEnemy(EnemyId id);

        const AZStd::array<EnemyInstance, MaxEnemyCount>& GetInstances() const { return m_instances; }

    private:
        AZStd::array<EnemyInstance, MaxEnemyCount> m_instances{};
        size_t m_enemyCount = 0;
        size_t m_requiredEnemyCount = 0;
    };
}
