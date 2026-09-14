#include <AzTest/AzTest.h>
#include <STWGameplay/AudioFeedbackPresentation.h>
#include <STWGameplay/PlayerSliceModel.h>

namespace STWGameplay
{
    TEST(AudioFeedbackPresentationTests, FireTransitionProducesOneEvent)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_shotFired = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Fire), 1u);
        input.m_shotFired = false;
        ASSERT_TRUE(audio.Update(0.016f, input));
        input.m_shotFired = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Fire), 2u);
    }

    TEST(AudioFeedbackPresentationTests, ReloadRisingEdgeProducesOneEvent)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_reloading = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Reload), 1u);
        input.m_reloading = false;
        ASSERT_TRUE(audio.Update(0.016f, input));
        input.m_reloading = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Reload), 2u);
    }

    TEST(AudioFeedbackPresentationTests, ConfirmedHitProducesHitFeedback)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_hitConfirmed = true;
        input.m_impactEvent = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Hit), 1u);
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Impact), 1u);
    }

    TEST(AudioFeedbackPresentationTests, ImpactEventMapsIndependently)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_impactEvent = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Impact), 1u);
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Hit), 0u);
    }

    TEST(AudioFeedbackPresentationTests, EnemyStateAndDeathMapToOneFeedbackEvent)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_enemyStateChanged = true;
        input.m_enemyDeathEvent = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::EnemyState), 1u);
    }

    TEST(AudioFeedbackPresentationTests, ResetClearsTransientDedupState)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_reloading = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        input = {};
        input.m_reset = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        input = {};
        input.m_reloading = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetResetCount(), 1u);
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Reload), 2u);
    }

    TEST(AudioFeedbackPresentationTests, MissingBackendDoesNotAffectPresentationState)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_shotFired = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_FALSE(audio.IsBackendReady());
        EXPECT_TRUE(audio.IsPresentationActive());
        EXPECT_TRUE(audio.IsVisualOnly());
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Fire), 1u);
    }

    TEST(AudioFeedbackPresentationTests, IdenticalInputsProduceIdenticalSequence)
    {
        const AZStd::array<AudioFeedbackInput, 4> inputs = {
            AudioFeedbackInput{ true },
            AudioFeedbackInput{ false, true },
            AudioFeedbackInput{ false, false, true, true },
            AudioFeedbackInput{ false, false, false, false, true }
        };
        AudioFeedbackPresentation first;
        AudioFeedbackPresentation second;
        for (const AudioFeedbackInput& input : inputs)
        {
            ASSERT_TRUE(first.Update(0.016f, input));
            ASSERT_TRUE(second.Update(0.016f, input));
        }
        EXPECT_EQ(first.GetEventSequence(), second.GetEventSequence());
    }

    TEST(AudioFeedbackPresentationTests, MovementFeedbackIsRateLimited)
    {
        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_movementActive = true;
        ASSERT_TRUE(audio.Update(0.016f, input));
        ASSERT_TRUE(audio.Update(0.016f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Movement), 1u);
        ASSERT_TRUE(audio.Update(0.36f, input));
        EXPECT_EQ(audio.GetEventCount(AudioFeedbackEventType::Movement), 2u);
    }

    TEST(AudioFeedbackPresentationTests, AuthorityRemainsUnchanged)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        const PlayerState playerBefore = model.GetPlayer();
        const WeaponState weaponBefore = model.GetWeapon();
        const EnemyState enemyBefore = model.GetEnemy().GetState();

        AudioFeedbackPresentation audio;
        AudioFeedbackInput input;
        input.m_shotFired = model.GetPresentation().m_shotFired;
        input.m_hitConfirmed = model.GetPresentation().m_hit;
        input.m_impactEvent = model.GetPresentation().m_hit;
        ASSERT_TRUE(audio.Update(0.016f, input));

        const PlayerState playerAfter = model.GetPlayer();
        const WeaponState weaponAfter = model.GetWeapon();
        const EnemyState enemyAfter = model.GetEnemy().GetState();
        EXPECT_EQ(playerAfter.m_damageEvents, playerBefore.m_damageEvents);
        EXPECT_EQ(playerAfter.m_deathEvents, playerBefore.m_deathEvents);
        EXPECT_EQ(playerAfter.m_respawnEvents, playerBefore.m_respawnEvents);
        EXPECT_EQ(weaponAfter.m_magazine, weaponBefore.m_magazine);
        EXPECT_EQ(weaponAfter.m_reserve, weaponBefore.m_reserve);
        EXPECT_FLOAT_EQ(weaponAfter.m_cooldownRemaining, weaponBefore.m_cooldownRemaining);
        EXPECT_EQ(enemyAfter.m_health, enemyBefore.m_health);
        EXPECT_EQ(enemyAfter.m_damageEvents, enemyBefore.m_damageEvents);
        EXPECT_EQ(enemyAfter.m_deathEvents, enemyBefore.m_deathEvents);
    }
}
