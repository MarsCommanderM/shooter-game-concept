#include <STWGameplay/ArenaLayout.h>

namespace STWGameplay
{
    bool ArenaLayout::IsInside(const AZ::Vector3& point)
    {
        return point.IsFinite() && point.GetX() >= BoundsMinimum.GetX() && point.GetX() <= BoundsMaximum.GetX()
            && point.GetY() >= BoundsMinimum.GetY() && point.GetY() <= BoundsMaximum.GetY()
            && point.GetZ() >= BoundsMinimum.GetZ() && point.GetZ() <= BoundsMaximum.GetZ();
    }

    bool ArenaLayout::Validate()
    {
        const AZ::Vector3 lane = EnemySpawn - PlayerSpawn;
        return PlayerSpawn.IsFinite() && EnemySpawn.IsFinite() && BoundsMinimum.IsFinite() && BoundsMaximum.IsFinite()
            && BoundsMaximum.GetX() > BoundsMinimum.GetX() && BoundsMaximum.GetY() > BoundsMinimum.GetY()
            && BoundsMaximum.GetZ() > BoundsMinimum.GetZ() && IsInside(PlayerSpawn) && IsInside(EnemySpawn)
            && lane.GetLength() >= MinimumSpawnSeparation && lane.GetLengthSq() > 0.0f;
    }
}
