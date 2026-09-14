#include <AzTest/AzTest.h>
#include <STWGameplay/EnemyCombatModel.h>
#include <STWGameplay/PlayerSliceModel.h>

namespace STWGameplay
{
    namespace
    {
        const AZ::Vector3 FarPlayer(0.0f, -6.0f, 0.15f);
        const AZ::Vector3 ClosePlayer(0.0f, 4.0f, 0.15f);

        void EnterAttack(EnemyCombatModel& enemy)
        {
            ASSERT_TRUE(enemy.Update(0.1f, ClosePlayer));
            ASSERT_TRUE(enemy.Update(0.1f, ClosePlayer));
            ASSERT_EQ(enemy.GetState().m_behaviorState, EnemyBehaviorState::Attack);
        }
    }

    TEST(EnemyAiCombatLoopTests, IdleTransitionsToDetectInsideAwareness)
    {
        EnemyCombatModel enemy;
        ASSERT_TRUE(enemy.Update(0.1f, FarPlayer));
        EXPECT_EQ(enemy.GetState().m_behaviorState, EnemyBehaviorState::Detect);
        EXPECT_EQ(enemy.GetState().m_detectionEvents, 1);
    }

    TEST(EnemyAiCombatLoopTests, DetectTransitionsToChaseOutsideAttackRange)
    {
        EnemyCombatModel enemy;
        ASSERT_TRUE(enemy.Update(0.1f, FarPlayer));
        ASSERT_TRUE(enemy.Update(0.1f, FarPlayer));
        EXPECT_EQ(enemy.GetState().m_behaviorState, EnemyBehaviorState::Chase);
        EXPECT_EQ(enemy.GetState().m_chaseEvents, 1);
    }

    TEST(EnemyAiCombatLoopTests, DetectTransitionsToAttackInsideAttackRange)
    {
        EnemyCombatModel enemy;
        EnterAttack(enemy);
        EXPECT_TRUE(enemy.GetMovementIntent(ClosePlayer).IsZero());
    }

    TEST(EnemyAiCombatLoopTests, AttackCooldownPreventsUncontrolledRepeatedDamage)
    {
        EnemyCombatModel enemy;
        EnterAttack(enemy);
        EXPECT_TRUE(enemy.TryAttackPlayer());
        EXPECT_FALSE(enemy.TryAttackPlayer());
        ASSERT_TRUE(enemy.Update(EnemyCombatModel::AttackCooldown * 0.5f, ClosePlayer));
        EXPECT_FALSE(enemy.TryAttackPlayer());
        ASSERT_TRUE(enemy.Update(EnemyCombatModel::AttackCooldown * 0.5f, ClosePlayer));
        EXPECT_TRUE(enemy.TryAttackPlayer());
    }

    TEST(EnemyAiCombatLoopTests, EnemyAttackReducesAuthoritativePlayerHealth)
    {
        PlayerSliceModel model;
        model.SetTargetPosition(model.GetPlayer().m_position + AZ::Vector3(0.0f, 2.0f, 0.0f));
        PlayerInput input;
        ASSERT_TRUE(model.Update(0.1f, input));
        ASSERT_TRUE(model.Update(0.1f, input));
        EXPECT_FLOAT_EQ(model.GetPlayer().m_health, 100.0f - EnemyCombatModel::AttackDamage);
        EXPECT_EQ(model.GetPlayer().m_damageEvents, 1);
    }

    TEST(EnemyAiCombatLoopTests, LethalEnemyDamageKillsPlayerWithoutNegativeHealth)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.ApplyDamage(500.0f));
        EXPECT_FALSE(model.GetPlayer().m_alive);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_health, 0.0f);
        EXPECT_EQ(model.GetPlayer().m_deathEvents, 1);
    }

    TEST(EnemyAiCombatLoopTests, DeadPlayerStateIsStableAndCannotFireOrMove)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.ApplyDamage(100.0f));
        EXPECT_FALSE(model.ApplyDamage(1.0f));
        EXPECT_FALSE(model.TryFire());
        PlayerInput input;
        input.m_forward = 1.0f;
        EXPECT_TRUE(model.GetDesiredVelocity(input).IsZero());
        EXPECT_EQ(model.GetPlayer().m_deathEvents, 1);
    }

    TEST(EnemyAiCombatLoopTests, PlayerResetRestoresHealthAndAliveState)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.ApplyDamage(100.0f));
        model.ResetPlayer();
        EXPECT_TRUE(model.GetPlayer().m_alive);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_health, model.GetPlayer().m_maxHealth);
        EXPECT_EQ(model.GetPlayer().m_respawnEvents, 1);
    }

    TEST(EnemyAiCombatLoopTests, ExistingWeaponAuthorityCanKillEnemy)
    {
        PlayerSliceModel model;
        model.SetTargetPosition(model.GetEyePosition() + model.GetAimDirection() * 8.0f);
        for (int shot = 0; shot < 7; ++shot)
        {
            ASSERT_TRUE(model.TryFire());
            ASSERT_TRUE(model.Update(PlayerSliceModel::FireInterval, PlayerInput{}));
        }
        EXPECT_FALSE(model.GetEnemy().GetState().m_alive);
        EXPECT_EQ(model.GetEnemy().GetState().m_behaviorState, EnemyBehaviorState::Dead);
    }

    TEST(EnemyAiCombatLoopTests, DeadEnemyCannotAttackOrMove)
    {
        EnemyCombatModel enemy;
        EnterAttack(enemy);
        ASSERT_TRUE(enemy.ApplyDamage(100.0f));
        ASSERT_TRUE(enemy.Update(1.0f, ClosePlayer));
        EXPECT_FALSE(enemy.TryAttackPlayer());
        EXPECT_TRUE(enemy.GetMovementIntent(ClosePlayer).IsZero());
    }

    TEST(EnemyAiCombatLoopTests, EnemyResetRestoresValidIdleState)
    {
        EnemyCombatModel enemy;
        ASSERT_TRUE(enemy.ApplyDamage(100.0f));
        enemy.Reset();
        EXPECT_TRUE(enemy.GetState().m_alive);
        EXPECT_EQ(enemy.GetState().m_behaviorState, EnemyBehaviorState::Idle);
        EXPECT_EQ(enemy.GetState().m_respawnEvents, 1);
    }

    TEST(EnemyAiCombatLoopTests, IdenticalInputsProduceDeterministicStateTransitions)
    {
        EnemyCombatModel first;
        EnemyCombatModel second;
        for (int step = 0; step < 3; ++step)
        {
            ASSERT_TRUE(first.Update(0.25f, FarPlayer));
            ASSERT_TRUE(second.Update(0.25f, FarPlayer));
            EXPECT_EQ(first.GetState().m_behaviorState, second.GetState().m_behaviorState);
            EXPECT_TRUE(first.GetMovementIntent(FarPlayer).IsClose(second.GetMovementIntent(FarPlayer)));
        }
    }
}
