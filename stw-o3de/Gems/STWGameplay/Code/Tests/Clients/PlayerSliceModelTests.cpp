#include <AzTest/AzTest.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        void Advance(PlayerSliceModel& model, float seconds, PlayerInput input = {})
        {
            constexpr float step = 1.0f / 120.0f;
            for (float elapsed = 0.0f; elapsed < seconds; elapsed += step)
            {
                ASSERT_TRUE(model.Update(AZStd::min(step, seconds - elapsed), input));
            }
        }
    }

    TEST(PlayerSliceModelTests, PlayerStartsHealthyAliveAndGrounded)
    {
        PlayerSliceModel model;
        EXPECT_FLOAT_EQ(model.GetPlayer().m_health, 100.0f);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_maxHealth, 100.0f);
        EXPECT_TRUE(model.GetPlayer().m_alive);
        EXPECT_TRUE(model.GetPlayer().m_grounded);
    }

    TEST(PlayerSliceModelTests, ForwardMovementChangesAuthoritativePosition)
    {
        PlayerSliceModel model;
        const AZ::Vector3 before = model.GetPlayer().m_position;
        PlayerInput input; input.m_forward = 1.0f;
        Advance(model, 0.5f, input);
        EXPECT_GT(model.GetPlayer().m_position.GetY(), before.GetY());
    }

    TEST(PlayerSliceModelTests, LookChangesAuthoritativeOrientation)
    {
        PlayerSliceModel model;
        PlayerInput input; input.m_lookX = 40.0f; input.m_lookY = -20.0f;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_NE(model.GetPlayer().m_yaw, 0.0f);
        EXPECT_NE(model.GetPlayer().m_pitch, 0.0f);
        EXPECT_LE(std::abs(model.GetPlayer().m_pitch), PlayerSliceModel::PitchLimit);
    }

    TEST(PlayerSliceModelTests, MovementAndLookShareOneUpdate)
    {
        PlayerSliceModel model;
        const AZ::Vector3 before = model.GetPlayer().m_position;
        PlayerInput input; input.m_forward = 1.0f; input.m_lookX = 25.0f;
        ASSERT_TRUE(model.Update(0.25f, input));
        EXPECT_FALSE(model.GetPlayer().m_position.IsClose(before));
        EXPECT_NE(model.GetPlayer().m_yaw, 0.0f);
    }

    TEST(PlayerSliceModelTests, SprintIsFasterButBounded)
    {
        PlayerSliceModel walking;
        PlayerSliceModel sprinting;
        PlayerInput walk; walk.m_forward = 1.0f;
        PlayerInput sprint = walk; sprint.m_sprint = true;
        Advance(walking, 0.5f, walk);
        Advance(sprinting, 0.5f, sprint);
        const float walkDistance = (walking.GetPlayer().m_position - AZ::Vector3(0.0f, -6.0f, 0.0f)).GetLength();
        const float sprintDistance = (sprinting.GetPlayer().m_position - AZ::Vector3(0.0f, -6.0f, 0.0f)).GetLength();
        EXPECT_GT(sprintDistance, walkDistance);
        EXPECT_LE(sprintDistance, PlayerSliceModel::SprintSpeed * 0.51f);
    }

    TEST(PlayerSliceModelTests, CollisionPreventsCoverPenetration)
    {
        PlayerSliceModel model;
        model.SetPlayerPosition(AZ::Vector3(-2.0f, -1.5f, 0.0f));
        PlayerInput input; input.m_forward = 1.0f;
        Advance(model, 1.0f, input);
        EXPECT_LE(model.GetPlayer().m_position.GetY(), -1.34f);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_position.GetZ(), 0.0f);
    }

    TEST(PlayerSliceModelTests, ValidShotConsumesExactlyOneRound)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        EXPECT_EQ(model.GetWeapon().m_magazine, 29);
        EXPECT_TRUE(model.GetPresentation().m_shotFired);
    }

    TEST(PlayerSliceModelTests, ShotDuringCooldownIsRejected)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        EXPECT_FALSE(model.TryFire());
        EXPECT_EQ(model.GetWeapon().m_magazine, 29);
    }

    TEST(PlayerSliceModelTests, ReloadConservesTotalAmmo)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        Advance(model, PlayerSliceModel::FireInterval);
        ASSERT_TRUE(model.StartReload());
        Advance(model, PlayerSliceModel::ReloadDuration + 0.01f);
        EXPECT_EQ(model.GetWeapon().m_magazine, 30);
        EXPECT_EQ(model.GetWeapon().m_reserve, 149);
        EXPECT_EQ(model.GetWeapon().m_magazine + model.GetWeapon().m_reserve, 179);
    }

    TEST(PlayerSliceModelTests, RepeatedReloadCannotDuplicateAmmo)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        ASSERT_TRUE(model.StartReload());
        EXPECT_FALSE(model.StartReload());
        Advance(model, PlayerSliceModel::ReloadDuration + 0.01f);
        EXPECT_FALSE(model.StartReload());
        EXPECT_EQ(model.GetWeapon().m_magazine + model.GetWeapon().m_reserve, 179);
    }

    TEST(PlayerSliceModelTests, FiringWhileReloadingIsRejected)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        Advance(model, PlayerSliceModel::FireInterval);
        ASSERT_TRUE(model.StartReload());
        EXPECT_FALSE(model.TryFire());
        EXPECT_EQ(model.GetWeapon().m_magazine, 29);
    }

    TEST(PlayerSliceModelTests, ValidHitDamagesTarget)
    {
        PlayerSliceModel model;
        ASSERT_TRUE(model.TryFire());
        EXPECT_FLOAT_EQ(model.GetTarget().m_health, 84.0f);
        EXPECT_TRUE(model.GetPresentation().m_hit);
    }

    TEST(PlayerSliceModelTests, MissDoesNotDamageTarget)
    {
        PlayerSliceModel model;
        model.SetTargetPosition(AZ::Vector3(8.0f, 0.0f, 1.2f));
        ASSERT_TRUE(model.TryFire());
        EXPECT_FLOAT_EQ(model.GetTarget().m_health, 100.0f);
        EXPECT_FALSE(model.GetPresentation().m_hit);
    }

    TEST(PlayerSliceModelTests, TargetDeathTriggersExactlyOnce)
    {
        PlayerSliceModel model;
        for (int shot = 0; shot < 8; ++shot)
        {
            if (model.GetWeapon().m_cooldownRemaining > 0.0f)
            {
                Advance(model, PlayerSliceModel::FireInterval);
            }
            ASSERT_TRUE(model.TryFire());
        }
        EXPECT_FALSE(model.GetTarget().m_alive);
        EXPECT_EQ(model.GetTarget().m_deathEvents, 1);
        EXPECT_FLOAT_EQ(model.GetTarget().m_health, 0.0f);
    }

    TEST(PlayerSliceModelTests, PresentationCannotMutateWeaponOrTargetState)
    {
        PlayerSliceModel model;
        const WeaponState weaponBefore = model.GetWeapon();
        const TargetState targetBefore = model.GetTarget();
        const PresentationState presentation = model.GetPresentation();
        (void)presentation;
        EXPECT_EQ(model.GetWeapon().m_magazine, weaponBefore.m_magazine);
        EXPECT_FLOAT_EQ(model.GetTarget().m_health, targetBefore.m_health);
    }

    TEST(PlayerSliceModelTests, InvalidDeltaIsTransactional)
    {
        PlayerSliceModel model;
        const PlayerState before = model.GetPlayer();
        PlayerInput input; input.m_forward = 1.0f;
        EXPECT_FALSE(model.Update(-0.1f, input));
        EXPECT_TRUE(model.GetPlayer().m_position.IsClose(before.m_position));
        EXPECT_FLOAT_EQ(model.GetPlayer().m_yaw, before.m_yaw);
    }
}
