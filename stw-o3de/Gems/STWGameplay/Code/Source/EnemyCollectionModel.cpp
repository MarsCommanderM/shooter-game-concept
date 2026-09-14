#include <STWGameplay/EnemyCollectionModel.h>

namespace STWGameplay
{
    namespace
    {
        const AZStd::array<AZ::Vector3, EnemyCollectionModel::RequiredEnemyCount> s_defaultSpawnPositions = {
            AZ::Vector3(0.0f, 6.0f, 1.2f),
            AZ::Vector3(-7.0f, 6.0f, 1.2f),
            AZ::Vector3(7.0f, 6.0f, 1.2f)
        };
        const AZStd::array<EnemyArchetype, EnemyCollectionModel::RequiredEnemyCount> s_defaultArchetypes = {
            EnemyArchetype::Assault,
            EnemyArchetype::Heavy,
            EnemyArchetype::Skirmisher
        };
    }

    EnemyCollectionModel::EnemyCollectionModel()
    {
        InitializeDefaultEncounter();
    }

    bool EnemyCollectionModel::InitializeDefaultEncounter()
    {
        for (EnemyInstance& instance : m_instances)
        {
            instance = {};
        }
        m_enemyCount = 0;
        m_requiredEnemyCount = 0;
        for (size_t index = 0; index < RequiredEnemyCount; ++index)
        {
            if (!AddEnemy(static_cast<EnemyId>(index + 1), s_defaultArchetypes[index],
                    s_defaultSpawnPositions[index], true))
            {
                return false;
            }
        }
        return true;
    }

    bool EnemyCollectionModel::AddEnemy(EnemyId id, EnemyArchetype archetype,
        const AZ::Vector3& spawnPosition, bool required)
    {
        if (id == InvalidEnemyId || m_enemyCount >= MaxEnemyCount || !spawnPosition.IsFinite() || Find(id) != nullptr)
        {
            return false;
        }
        EnemyInstance& instance = m_instances[m_enemyCount];
        instance = {};
        instance.m_id = id;
        instance.m_required = required;
        instance.m_spawnPosition = spawnPosition;
        if (!instance.m_combat.Configure(id, GetEnemyProfile(archetype), spawnPosition))
        {
            instance = {};
            return false;
        }
        ++m_enemyCount;
        if (required)
        {
            ++m_requiredEnemyCount;
        }
        return true;
    }

    bool EnemyCollectionModel::Update(float deltaTime, const AZ::Vector3& playerPosition, bool playerAlive)
    {
        bool updated = true;
        for (size_t index = 0; index < m_enemyCount; ++index)
        {
            updated = m_instances[index].m_combat.Update(deltaTime, playerPosition, playerAlive) && updated;
        }
        return updated;
    }

    bool EnemyCollectionModel::ApplyDamage(EnemyId id, float damage)
    {
        EnemyCombatModel* enemy = GetEnemy(id);
        return enemy != nullptr && enemy->ApplyDamage(damage);
    }

    bool EnemyCollectionModel::TryAttackPlayer(EnemyId id)
    {
        EnemyCombatModel* enemy = GetEnemy(id);
        return enemy != nullptr && enemy->TryAttackPlayer();
    }

    bool EnemyCollectionModel::ResetEnemy(EnemyId id)
    {
        EnemyCombatModel* enemy = GetEnemy(id);
        if (enemy == nullptr)
        {
            return false;
        }
        enemy->Reset();
        return true;
    }

    bool EnemyCollectionModel::ResetRequiredEnemies()
    {
        bool reset = true;
        for (size_t index = 0; index < m_enemyCount; ++index)
        {
            if (m_instances[index].m_required)
            {
                m_instances[index].m_combat.Reset();
            }
        }
        return reset;
    }

    bool EnemyCollectionModel::SynchronizePhysicalPosition(EnemyId id, const AZ::Vector3& position)
    {
        EnemyCombatModel* enemy = GetEnemy(id);
        if (enemy == nullptr || !position.IsFinite())
        {
            return false;
        }
        enemy->SynchronizePhysicalPosition(position);
        return true;
    }

    size_t EnemyCollectionModel::GetAliveCount() const
    {
        size_t alive = 0;
        for (size_t index = 0; index < m_enemyCount; ++index)
        {
            alive += m_instances[index].m_combat.GetState().m_alive ? 1 : 0;
        }
        return alive;
    }

    bool EnemyCollectionModel::AreRequiredEnemiesAlive() const
    {
        size_t required = 0;
        for (size_t index = 0; index < m_enemyCount; ++index)
        {
            if (m_instances[index].m_required)
            {
                ++required;
                if (!m_instances[index].m_combat.GetState().m_alive)
                {
                    return false;
                }
            }
        }
        return required == m_requiredEnemyCount && required > 0;
    }

    bool EnemyCollectionModel::AreRequiredEnemiesEliminated() const
    {
        size_t required = 0;
        for (size_t index = 0; index < m_enemyCount; ++index)
        {
            if (m_instances[index].m_required)
            {
                ++required;
                const EnemyState& state = m_instances[index].m_combat.GetState();
                if (state.m_alive || state.m_deathEvents <= 0)
                {
                    return false;
                }
            }
        }
        return required == m_requiredEnemyCount && required > 0;
    }

    bool EnemyCollectionModel::IsRequiredEnemy(EnemyId id) const
    {
        const EnemyInstance* instance = Find(id);
        return instance != nullptr && instance->m_required;
    }

    const EnemyInstance* EnemyCollectionModel::Find(EnemyId id) const
    {
        for (size_t index = 0; index < m_enemyCount; ++index)
        {
            if (m_instances[index].m_id == id)
            {
                return &m_instances[index];
            }
        }
        return nullptr;
    }

    EnemyInstance* EnemyCollectionModel::Find(EnemyId id)
    {
        for (size_t index = 0; index < m_enemyCount; ++index)
        {
            if (m_instances[index].m_id == id)
            {
                return &m_instances[index];
            }
        }
        return nullptr;
    }

    const EnemyCombatModel* EnemyCollectionModel::GetEnemy(EnemyId id) const
    {
        const EnemyInstance* instance = Find(id);
        return instance != nullptr ? &instance->m_combat : nullptr;
    }

    EnemyCombatModel* EnemyCollectionModel::GetEnemy(EnemyId id)
    {
        EnemyInstance* instance = Find(id);
        return instance != nullptr ? &instance->m_combat : nullptr;
    }
}
