#include <AzTest/AzTest.h>
#include <STWGameplay/EnemyCombatModel.h>

namespace STWGameplay
{
    TEST(EnemyCombatModelTests, InitialStateIsAliveAtDeterministicSpawn)
    {
        EnemyCombatModel enemy;
        EXPECT_TRUE(enemy.GetState().m_alive);
        EXPECT_FLOAT_EQ(enemy.GetState().m_health, 100.0f);
        EXPECT_TRUE(enemy.GetState().m_position.IsClose(EnemyCombatModel::SpawnPosition));
    }

    TEST(EnemyCombatModelTests, DamageReducesHealth)
    {
        EnemyCombatModel enemy;
        ASSERT_TRUE(enemy.ApplyDamage(16.0f));
        EXPECT_FLOAT_EQ(enemy.GetState().m_health, 84.0f);
        EXPECT_EQ(enemy.GetState().m_damageEvents, 1);
    }

    TEST(EnemyCombatModelTests, LethalDamageEntersDeadState)
    {
        EnemyCombatModel enemy;
        ASSERT_TRUE(enemy.ApplyDamage(100.0f));
        EXPECT_FALSE(enemy.GetState().m_alive);
        EXPECT_EQ(enemy.GetState().m_deathEvents, 1);
    }

    TEST(EnemyCombatModelTests, DeadEnemyCannotMoveOrReceiveDamage)
    {
        EnemyCombatModel enemy;
        ASSERT_TRUE(enemy.ApplyDamage(100.0f));
        const AZ::Vector3 position = enemy.GetState().m_position;
        EXPECT_TRUE(enemy.Update(1.0f, AZ::Vector3(0.0f, -6.0f, 0.0f)));
        EXPECT_TRUE(enemy.GetState().m_position.IsClose(position));
        EXPECT_FALSE(enemy.ApplyDamage(10.0f));
    }

    TEST(EnemyCombatModelTests, ResetRestoresSpawnAndHealth)
    {
        EnemyCombatModel enemy;
        ASSERT_TRUE(enemy.ApplyDamage(100.0f));
        enemy.Reset();
        EXPECT_TRUE(enemy.GetState().m_alive);
        EXPECT_FLOAT_EQ(enemy.GetState().m_health, enemy.GetState().m_maxHealth);
        EXPECT_TRUE(enemy.GetState().m_position.IsClose(EnemyCombatModel::SpawnPosition));
        EXPECT_EQ(enemy.GetState().m_respawnEvents, 1);
    }

    TEST(EnemyCombatModelTests, MovementIntentTowardPlayerIsDeterministic)
    {
        EnemyCombatModel first;
        EnemyCombatModel second;
        const AZ::Vector3 player(0.0f, -6.0f, 0.0f);
        ASSERT_TRUE(first.Update(0.1f, player));
        ASSERT_TRUE(first.Update(0.1f, player));
        ASSERT_TRUE(second.Update(0.1f, player));
        ASSERT_TRUE(second.Update(0.1f, player));
        const AZ::Vector3 intent = first.GetMovementIntent(player);
        EXPECT_TRUE(intent.IsClose(second.GetMovementIntent(player)));
        EXPECT_NEAR(intent.GetLength(), EnemyCombatModel::MoveSpeed, 0.0001f);
        EXPECT_LT(intent.GetY(), 0.0f);
    }
}
