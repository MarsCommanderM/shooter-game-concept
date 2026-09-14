#include <AzTest/AzTest.h>
#include <STWGameplay/EnemyPresentation.h>

namespace STWGameplay
{
    TEST(EnemyPresentationTests, IdlePresentationIsDeterministic)
    {
        EnemyPresentation first;
        EnemyPresentation second;
        EnemyState state;
        ASSERT_TRUE(first.Update(0.25f, state));
        ASSERT_TRUE(second.Update(0.25f, state));
        EXPECT_TRUE(first.GetLocalTransform().IsClose(second.GetLocalTransform()));
        EXPECT_TRUE(first.GetScale().IsClose(second.GetScale()));
    }

    TEST(EnemyPresentationTests, ChasePresentationIsDeterministic)
    {
        EnemyPresentation first;
        EnemyPresentation second;
        EnemyState state;
        state.m_behaviorState = EnemyBehaviorState::Chase;
        ASSERT_TRUE(first.Update(0.2f, state));
        ASSERT_TRUE(second.Update(0.2f, state));
        EXPECT_TRUE(first.GetLocalTransform().IsClose(second.GetLocalTransform()));
        EXPECT_EQ(first.GetState(), EnemyBehaviorState::Chase);
    }

    TEST(EnemyPresentationTests, AttackPresentationConsumesAuthoritativeEvent)
    {
        EnemyPresentation presentation;
        EnemyState state;
        state.m_behaviorState = EnemyBehaviorState::Attack;
        state.m_attackEvents = 1;
        ASSERT_TRUE(presentation.Update(0.0f, state));
        EXPECT_EQ(presentation.GetAttackReactionCount(), 1);
        EXPECT_FALSE(presentation.GetLocalTransform().IsClose(AZ::Transform::CreateIdentity()));
    }

    TEST(EnemyPresentationTests, DeadPresentationIsDeterministic)
    {
        EnemyPresentation presentation;
        EnemyState state;
        state.m_alive = false;
        state.m_behaviorState = EnemyBehaviorState::Dead;
        state.m_deathEvents = 1;
        ASSERT_TRUE(presentation.Update(0.4f, state));
        EXPECT_EQ(presentation.GetDeathReactionCount(), 1);
        EXPECT_LT(presentation.GetScale().GetZ(), 1.0f);
    }

    TEST(EnemyPresentationTests, RespawnRestoresBasePresentation)
    {
        EnemyPresentation presentation;
        EnemyState dead;
        dead.m_alive = false;
        dead.m_behaviorState = EnemyBehaviorState::Dead;
        dead.m_deathEvents = 1;
        ASSERT_TRUE(presentation.Update(1.0f, dead));
        EnemyState reset;
        reset.m_deathEvents = 1;
        reset.m_respawnEvents = 1;
        ASSERT_TRUE(presentation.Update(0.0f, reset));
        EXPECT_TRUE(presentation.GetLocalTransform().IsClose(AZ::Transform::CreateIdentity()));
        EXPECT_TRUE(presentation.GetScale().IsClose(AZ::Vector3::CreateOne()));
        EXPECT_EQ(presentation.GetResetReactionCount(), 1);
    }

    TEST(EnemyPresentationTests, PresentationCannotMutateEnemyCombatModel)
    {
        EnemyCombatModel model;
        ASSERT_TRUE(model.ApplyDamage(16.0f));
        const EnemyState before = model.GetState();
        EnemyPresentation presentation;
        ASSERT_TRUE(presentation.Update(0.25f, model.GetState()));
        const EnemyState& after = model.GetState();
        EXPECT_TRUE(after.m_position.IsClose(before.m_position));
        EXPECT_FLOAT_EQ(after.m_health, before.m_health);
        EXPECT_EQ(after.m_damageEvents, before.m_damageEvents);
        EXPECT_EQ(after.m_behaviorState, before.m_behaviorState);
    }
}
