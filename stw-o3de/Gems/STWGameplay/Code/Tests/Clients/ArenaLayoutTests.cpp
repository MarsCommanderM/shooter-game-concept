#include <AzTest/AzTest.h>
#include <STWGameplay/ArenaLayout.h>

namespace STWGameplay
{
    TEST(ArenaLayoutTests, SpawnLocationsAreFinite)
    {
        EXPECT_TRUE(ArenaLayout::PlayerSpawn.IsFinite());
        EXPECT_TRUE(ArenaLayout::EnemySpawn.IsFinite());
    }
    TEST(ArenaLayoutTests, SpawnSeparationExceedsMinimum)
    {
        EXPECT_GE((ArenaLayout::EnemySpawn - ArenaLayout::PlayerSpawn).GetLength(), ArenaLayout::MinimumSpawnSeparation);
    }
    TEST(ArenaLayoutTests, BoundsAreValid)
    {
        EXPECT_GT(ArenaLayout::BoundsMaximum.GetX(), ArenaLayout::BoundsMinimum.GetX());
        EXPECT_GT(ArenaLayout::BoundsMaximum.GetY(), ArenaLayout::BoundsMinimum.GetY());
        EXPECT_GT(ArenaLayout::BoundsMaximum.GetZ(), ArenaLayout::BoundsMinimum.GetZ());
    }
    TEST(ArenaLayoutTests, EnemySpawnIsInsideArena) { EXPECT_TRUE(ArenaLayout::IsInside(ArenaLayout::EnemySpawn)); }
    TEST(ArenaLayoutTests, PlayerSpawnIsInsideArena) { EXPECT_TRUE(ArenaLayout::IsInside(ArenaLayout::PlayerSpawn)); }
    TEST(ArenaLayoutTests, CombatLaneIsNonZeroAndLayoutValid)
    {
        EXPECT_GT((ArenaLayout::EnemySpawn - ArenaLayout::PlayerSpawn).GetLengthSq(), 0.0f);
        EXPECT_TRUE(ArenaLayout::Validate());
    }
}
