#include <AzTest/AzTest.h>
#include <STWGameplay/SpawnCheckpointModel.h>
#include <STWGameplay/PlayerSliceModel.h>

#include <cmath>

namespace STWGameplay
{
    TEST(SpawnCheckpointModelTests, DefaultsToArenaSpawn)
    {
        SpawnCheckpointModel model;
        EXPECT_FALSE(model.HasActiveCheckpoint());
        EXPECT_TRUE(model.ResolveRespawnPosition().IsClose(ArenaLayout::PlayerSpawn));
        EXPECT_EQ(model.GetActivationCount(), 0);
    }

    TEST(SpawnCheckpointModelTests, ActivatesValidCheckpoint)
    {
        SpawnCheckpointModel model;
        const AZ::Vector3 checkpoint(0.0f, -5.0f, 0.15f);
        ASSERT_TRUE(model.ActivateCheckpoint(checkpoint));
        EXPECT_TRUE(model.HasActiveCheckpoint());
        EXPECT_TRUE(model.GetActiveCheckpointPosition().IsClose(checkpoint));
        EXPECT_EQ(model.GetActivationCount(), 1);
    }

    TEST(SpawnCheckpointModelTests, RejectsInvalidCheckpoint)
    {
        SpawnCheckpointModel model;
        EXPECT_FALSE(model.ActivateCheckpoint(AZ::Vector3(NAN, 0.0f, 0.0f)));
        EXPECT_FALSE(model.ActivateCheckpoint(AZ::Vector3(100.0f, 0.0f, 0.15f)));
        EXPECT_FALSE(model.HasActiveCheckpoint());
        EXPECT_EQ(model.GetActivationCount(), 0);
    }

    TEST(SpawnCheckpointModelTests, DuplicateActivationIsIdempotent)
    {
        SpawnCheckpointModel model;
        const AZ::Vector3 checkpoint(0.0f, -5.0f, 0.15f);
        ASSERT_TRUE(model.ActivateCheckpoint(checkpoint));
        ASSERT_TRUE(model.ActivateCheckpoint(checkpoint));
        EXPECT_EQ(model.GetActivationCount(), 1);
        EXPECT_TRUE(model.ResolveRespawnPosition().IsClose(checkpoint));
    }

    TEST(SpawnCheckpointModelTests, ActiveCheckpointResolvesDeterministically)
    {
        SpawnCheckpointModel first;
        SpawnCheckpointModel second;
        const AZ::Vector3 checkpoint(1.0f, -4.0f, 0.15f);
        ASSERT_TRUE(first.ActivateCheckpoint(checkpoint));
        ASSERT_TRUE(second.ActivateCheckpoint(checkpoint));
        EXPECT_TRUE(first.ResolveRespawnPosition().IsClose(second.ResolveRespawnPosition()));
    }

    TEST(SpawnCheckpointModelTests, ResetRearmsDefaultSpawn)
    {
        SpawnCheckpointModel model;
        ASSERT_TRUE(model.ActivateCheckpoint(AZ::Vector3(0.0f, -5.0f, 0.15f)));
        model.Reset();
        EXPECT_FALSE(model.HasActiveCheckpoint());
        EXPECT_TRUE(model.ResolveRespawnPosition().IsClose(ArenaLayout::PlayerSpawn));
        EXPECT_EQ(model.GetActivationCount(), 0);
    }

    TEST(SpawnCheckpointModelTests, CheckpointStateIsSeparateFromPlayerAndEnemyAuthority)
    {
        SpawnCheckpointModel checkpoint;
        PlayerSliceModel player;
        const EnemyState enemyBefore = player.GetEnemy().GetState();
        const PlayerState playerBefore = player.GetPlayer();
        ASSERT_TRUE(checkpoint.ActivateCheckpoint(AZ::Vector3(0.0f, -5.0f, 0.15f)));
        EXPECT_TRUE(player.GetPlayer().m_position.IsClose(playerBefore.m_position));
        EXPECT_EQ(player.GetPlayer().m_health, playerBefore.m_health);
        EXPECT_EQ(player.GetEnemy().GetState().m_deathEvents, enemyBefore.m_deathEvents);
        EXPECT_EQ(player.GetEnemy().GetState().m_health, enemyBefore.m_health);
    }
}
