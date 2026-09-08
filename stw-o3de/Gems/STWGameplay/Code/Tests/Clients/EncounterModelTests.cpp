#include <AzTest/AzTest.h>
#include <STWGameplay/EncounterModel.h>

namespace STWGameplay
{
    TEST(EncounterModelTests, StartsActive)
    {
        EncounterModel encounter;
        EXPECT_TRUE(encounter.IsActive());
        EXPECT_EQ(encounter.GetCompletedCount(), 0);
    }

    TEST(EncounterModelTests, FirstEnemyEliminationCompletesOnce)
    {
        EncounterModel encounter;
        EnemyState enemy;
        enemy.m_alive = false;
        enemy.m_deathEvents = 1;
        ASSERT_TRUE(encounter.Update(enemy));
        EXPECT_TRUE(encounter.IsCompleted());
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
        ASSERT_TRUE(encounter.Update(enemy));
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
    }

    TEST(EncounterModelTests, RepeatedCompletedTicksDoNotDoubleComplete)
    {
        EncounterModel encounter;
        EnemyState enemy;
        enemy.m_alive = false;
        enemy.m_deathEvents = 1;
        ASSERT_TRUE(encounter.Update(enemy));
        ASSERT_TRUE(encounter.Update(enemy));
        ASSERT_TRUE(encounter.Update(enemy));
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
    }

    TEST(EncounterModelTests, RearmStartsFreshActiveEncounter)
    {
        EncounterModel encounter;
        EnemyState enemy;
        enemy.m_alive = false;
        enemy.m_deathEvents = 1;
        ASSERT_TRUE(encounter.Update(enemy));
        enemy.m_alive = true;
        enemy.m_respawnEvents = 1;
        ASSERT_TRUE(encounter.Rearm(enemy));
        EXPECT_TRUE(encounter.IsActive());
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
    }

    TEST(EncounterModelTests, RearmDoesNotCountCompletion)
    {
        EncounterModel encounter;
        EnemyState enemy;
        enemy.m_alive = true;
        EXPECT_FALSE(encounter.Rearm(enemy));
        EXPECT_EQ(encounter.GetCompletedCount(), 0);
    }

    TEST(EncounterModelTests, EliminationAfterRearmAddsExactlyOneCompletion)
    {
        EncounterModel encounter;
        EnemyState enemy;
        enemy.m_alive = false;
        enemy.m_deathEvents = 1;
        ASSERT_TRUE(encounter.Update(enemy));
        enemy.m_alive = true;
        enemy.m_respawnEvents = 1;
        ASSERT_TRUE(encounter.Rearm(enemy));
        enemy.m_alive = false;
        enemy.m_deathEvents = 2;
        ASSERT_TRUE(encounter.Update(enemy));
        ASSERT_TRUE(encounter.Update(enemy));
        EXPECT_TRUE(encounter.IsCompleted());
        EXPECT_EQ(encounter.GetCompletedCount(), 2);
    }

    TEST(EncounterModelTests, CompletionDoesNotMutatePlayerOrEnemyAuthority)
    {
        EncounterModel encounter;
        EnemyState enemy;
        enemy.m_alive = false;
        enemy.m_deathEvents = 1;
        const EnemyState before = enemy;
        ASSERT_TRUE(encounter.Update(enemy));
        EXPECT_EQ(enemy.m_alive, before.m_alive);
        EXPECT_EQ(enemy.m_health, before.m_health);
        EXPECT_EQ(enemy.m_deathEvents, before.m_deathEvents);
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
    }
}
