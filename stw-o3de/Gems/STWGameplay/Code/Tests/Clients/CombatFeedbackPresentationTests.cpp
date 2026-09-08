#include <AzTest/AzTest.h>
#include <STWGameplay/CombatFeedbackPresentation.h>
#include <STWGameplay/PlayerSliceModel.h>

namespace STWGameplay
{
    TEST(CombatFeedbackPresentationTests, FireFeedbackIdle)
    {
        CombatFeedbackPresentation feedback;
        ASSERT_TRUE(feedback.Update(0.016f, {}));
        EXPECT_FALSE(feedback.IsFireFlashVisible());
        EXPECT_EQ(feedback.GetFireFeedbackCount(), 0u);
    }

    TEST(CombatFeedbackPresentationTests, FireFeedbackTrigger)
    {
        CombatFeedbackPresentation feedback;
        CombatFeedbackInput input;
        input.m_shotFired = true;
        ASSERT_TRUE(feedback.Update(0.0f, input));
        EXPECT_TRUE(feedback.IsFireFlashVisible());
        EXPECT_FLOAT_EQ(feedback.GetFireIntensity(), 1.0f);
        EXPECT_EQ(feedback.GetFireFeedbackCount(), 1u);
    }

    TEST(CombatFeedbackPresentationTests, FireFeedbackDecay)
    {
        CombatFeedbackPresentation feedback;
        CombatFeedbackInput input;
        input.m_shotFired = true;
        ASSERT_TRUE(feedback.Update(0.0f, input));
        ASSERT_TRUE(feedback.Update(CombatFeedbackPresentation::FireFlashDuration, {}));
        EXPECT_FALSE(feedback.IsFireFlashVisible());
        EXPECT_FLOAT_EQ(feedback.GetFireIntensity(), 0.0f);
    }

    TEST(CombatFeedbackPresentationTests, EnemyHitFeedbackTrigger)
    {
        CombatFeedbackPresentation feedback;
        CombatFeedbackInput input;
        input.m_hitConfirmed = true;
        input.m_impactPosition = AZ::Vector3(1.0f, 2.0f, 3.0f);
        ASSERT_TRUE(feedback.Update(0.0f, input));
        EXPECT_TRUE(feedback.IsEnemyHitVisible());
        EXPECT_GT(feedback.GetEnemyHitScale(), 1.0f);
        EXPECT_EQ(feedback.GetHitFeedbackCount(), 1u);
    }

    TEST(CombatFeedbackPresentationTests, EnemyHitFeedbackDecay)
    {
        CombatFeedbackPresentation feedback;
        CombatFeedbackInput input;
        input.m_hitConfirmed = true;
        ASSERT_TRUE(feedback.Update(0.0f, input));
        ASSERT_TRUE(feedback.Update(CombatFeedbackPresentation::EnemyHitDuration, {}));
        EXPECT_FALSE(feedback.IsEnemyHitVisible());
        EXPECT_FLOAT_EQ(feedback.GetEnemyHitScale(), 1.0f);
    }

    TEST(CombatFeedbackPresentationTests, ImpactFeedbackTrigger)
    {
        CombatFeedbackPresentation feedback;
        CombatFeedbackInput input;
        input.m_hitConfirmed = true;
        input.m_impactPosition = AZ::Vector3(4.0f, 5.0f, 1.2f);
        ASSERT_TRUE(feedback.Update(0.0f, input));
        EXPECT_TRUE(feedback.IsImpactVisible());
        EXPECT_TRUE(feedback.GetImpactPosition().IsClose(input.m_impactPosition));
        EXPECT_GT(feedback.GetImpactScale(), 0.0f);
        EXPECT_EQ(feedback.GetImpactFeedbackCount(), 1u);
    }

    TEST(CombatFeedbackPresentationTests, ImpactFeedbackExpire)
    {
        CombatFeedbackPresentation feedback;
        CombatFeedbackInput input;
        input.m_hitConfirmed = true;
        ASSERT_TRUE(feedback.Update(0.0f, input));
        ASSERT_TRUE(feedback.Update(CombatFeedbackPresentation::ImpactDuration, {}));
        EXPECT_FALSE(feedback.IsImpactVisible());
        EXPECT_FLOAT_EQ(feedback.GetImpactScale(), 0.0f);
    }

    TEST(CombatFeedbackPresentationTests, AuthoritySeparation)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        const WeaponState weapon = model.GetWeapon();
        const EnemyState enemy = model.GetEnemy().GetState();
        CombatFeedbackPresentation feedback;
        CombatFeedbackInput input;
        input.m_shotFired = model.GetPresentation().m_shotFired;
        input.m_hitConfirmed = model.GetPresentation().m_hit;
        input.m_impactPosition = enemy.m_position;
        ASSERT_TRUE(feedback.Update(0.016f, input));
        EXPECT_EQ(model.GetWeapon().m_magazine, weapon.m_magazine);
        EXPECT_FLOAT_EQ(model.GetWeapon().m_cooldownRemaining, weapon.m_cooldownRemaining);
        EXPECT_FLOAT_EQ(model.GetEnemy().GetState().m_health, enemy.m_health);
        EXPECT_EQ(model.GetEnemy().GetState().m_damageEvents, enemy.m_damageEvents);
    }
}
