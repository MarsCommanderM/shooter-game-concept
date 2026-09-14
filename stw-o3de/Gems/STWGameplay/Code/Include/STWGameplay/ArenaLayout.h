#pragma once

#include <AzCore/Math/Vector3.h>

namespace STWGameplay
{
    struct ArenaLayout
    {
        static inline const AZ::Vector3 PlayerSpawn = AZ::Vector3(0.0f, -11.0f, 0.15f);
        static inline const AZ::Vector3 EnemySpawn = AZ::Vector3(0.0f, 6.0f, 1.2f);
        static inline const AZ::Vector3 BoundsMinimum = AZ::Vector3(-11.5f, -11.5f, -0.1f);
        static inline const AZ::Vector3 BoundsMaximum = AZ::Vector3(11.5f, 11.5f, 4.0f);
        static constexpr float MinimumSpawnSeparation = 8.0f;

        static bool IsInside(const AZ::Vector3& point);
        static bool Validate();
    };
}
