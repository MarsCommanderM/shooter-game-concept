#pragma once

#include <AzCore/Math/Vector3.h>
#include <STWGameplay/ArenaLayout.h>

namespace STWGameplay
{
    //! Deterministic respawn-position state. It has no ownership of player, enemy, physics,
    //! encounter, or presentation state.
    class SpawnCheckpointModel
    {
    public:
        bool ActivateCheckpoint(const AZ::Vector3& position);
        AZ::Vector3 ResolveRespawnPosition() const;
        void Reset();

        const AZ::Vector3& GetDefaultSpawnPosition() const { return m_defaultSpawn; }
        const AZ::Vector3& GetActiveCheckpointPosition() const { return m_activeCheckpoint; }
        bool HasActiveCheckpoint() const { return m_hasActiveCheckpoint; }
        int GetActivationCount() const { return m_activationCount; }

    private:
        AZ::Vector3 m_defaultSpawn = ArenaLayout::PlayerSpawn;
        AZ::Vector3 m_activeCheckpoint = ArenaLayout::PlayerSpawn;
        bool m_hasActiveCheckpoint = false;
        int m_activationCount = 0;
    };
}
