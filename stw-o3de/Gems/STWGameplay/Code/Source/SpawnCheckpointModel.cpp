#include <STWGameplay/SpawnCheckpointModel.h>

namespace STWGameplay
{
    bool SpawnCheckpointModel::ActivateCheckpoint(const AZ::Vector3& position)
    {
        if (!position.IsFinite() || !ArenaLayout::IsInside(position))
        {
            return false;
        }
        if (m_hasActiveCheckpoint && m_activeCheckpoint.IsClose(position))
        {
            return true;
        }

        m_activeCheckpoint = position;
        m_hasActiveCheckpoint = true;
        ++m_activationCount;
        return true;
    }

    AZ::Vector3 SpawnCheckpointModel::ResolveRespawnPosition() const
    {
        return m_hasActiveCheckpoint ? m_activeCheckpoint : m_defaultSpawn;
    }

    void SpawnCheckpointModel::Reset()
    {
        m_activeCheckpoint = m_defaultSpawn;
        m_hasActiveCheckpoint = false;
        m_activationCount = 0;
    }
}
