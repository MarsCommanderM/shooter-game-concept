#include <AzTest/AzTest.h>
#include <AzCore/Math/MathUtils.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <cmath>

AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

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

    TEST(PlayerSliceModelTests, PlayerStartsHealthyAliveAwaitingPhysicalGroundState)
    {
        PlayerSliceModel model;
        EXPECT_FLOAT_EQ(model.GetPlayer().m_health, 100.0f);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_maxHealth, 100.0f);
        EXPECT_TRUE(model.GetPlayer().m_alive);
        EXPECT_FALSE(model.GetPlayer().m_grounded);
    }

    TEST(PlayerSliceModelTests, ForwardInputProducesCameraRelativeVelocity)
    {
        PlayerSliceModel model;
        PlayerInput input; input.m_forward = 1.0f;
        const AZ::Vector3 velocity = model.GetDesiredVelocity(input);
        EXPECT_NEAR(velocity.GetX(), 0.0f, 0.001f);
        EXPECT_NEAR(velocity.GetY(), PlayerSliceModel::WalkSpeed, 0.001f);
        EXPECT_NEAR(velocity.GetZ(), 0.0f, 0.001f);
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
        PlayerInput input; input.m_forward = 1.0f; input.m_lookX = 25.0f;
        ASSERT_TRUE(model.Update(0.25f, input));
        EXPECT_GT(model.GetDesiredVelocity(input).GetX(), 0.0f);
        EXPECT_NE(model.GetPlayer().m_yaw, 0.0f);
    }

    TEST(PlayerSliceModelTests, SprintIsFasterButBounded)
    {
        PlayerInput walk; walk.m_forward = 1.0f;
        PlayerInput sprint = walk; sprint.m_sprint = true;
        PlayerSliceModel model;
        const float walkSpeed = model.GetDesiredVelocity(walk).GetLength();
        const float sprintSpeed = model.GetDesiredVelocity(sprint).GetLength();
        EXPECT_GT(sprintSpeed, walkSpeed);
        EXPECT_NEAR(sprintSpeed, PlayerSliceModel::SprintSpeed, 0.001f);
    }

    TEST(PlayerSliceModelTests, DiagonalVelocityIsNormalized)
    {
        PlayerSliceModel model;
        PlayerInput input; input.m_forward = 1.0f; input.m_strafe = 1.0f;
        EXPECT_NEAR(model.GetDesiredVelocity(input).GetLength(), PlayerSliceModel::WalkSpeed, 0.001f);
    }

    TEST(PlayerSliceModelTests, PhysicalSynchronizationOwnsPositionGroundingAndHeight)
    {
        PlayerSliceModel model;
        const AZ::Vector3 physicalPosition(3.0f, 4.0f, 0.27f);
        model.SynchronizePhysicalState(physicalPosition, true);
        EXPECT_TRUE(model.GetPlayer().m_position.IsClose(physicalPosition));
        EXPECT_TRUE(model.GetPlayer().m_grounded);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_position.GetZ(), 0.27f);
    }

    TEST(PlayerSliceModelTests, CameraYawRotatesMovementDirection)
    {
        PlayerSliceModel model;
        PlayerInput look; look.m_lookX = (AZ::Constants::HalfPi / PlayerSliceModel::LookSensitivity);
        ASSERT_TRUE(model.Update(0.016f, look));
        PlayerInput move; move.m_forward = 1.0f;
        const AZ::Vector3 velocity = model.GetDesiredVelocity(move);
        EXPECT_GT(velocity.GetX(), PlayerSliceModel::WalkSpeed - 0.01f);
        EXPECT_NEAR(velocity.GetY(), 0.0f, 0.01f);
    }

    TEST(PlayerSliceModelTests, CameraPitchIsBoundedForExtremeInput)
    {
        PlayerSliceModel model;
        PlayerInput input; input.m_lookY = 100000.0f;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FLOAT_EQ(model.GetPlayer().m_pitch, -PlayerSliceModel::PitchLimit);
    }

    TEST(PlayerSliceModelTests, GroundedJumpPressProducesBoundedVerticalImpulse)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FLOAT_EQ(model.GetDesiredVelocity(input).GetZ(), PlayerSliceModel::JumpImpulseSpeed);
        EXPECT_EQ(model.GetPlayer().m_jumpEvents, 1);
    }

    TEST(PlayerSliceModelTests, JumpImpulseLastsExactlyOneModelUpdate)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        ASSERT_GT(model.GetDesiredVelocity(input).GetZ(), 0.0f);
        input.m_jump = false;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FLOAT_EQ(model.GetDesiredVelocity(input).GetZ(), 0.0f);
    }

    TEST(PlayerSliceModelTests, HeldJumpDoesNotRetriggerWhileGrounded)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FLOAT_EQ(model.GetDesiredVelocity(input).GetZ(), 0.0f);
        EXPECT_EQ(model.GetPlayer().m_jumpEvents, 1);
    }

    TEST(PlayerSliceModelTests, AirborneJumpPressIsRejected)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3(0.0f, 0.0f, 1.0f), false);
        PlayerInput input; input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FLOAT_EQ(model.GetDesiredVelocity(input).GetZ(), 0.0f);
        EXPECT_EQ(model.GetPlayer().m_jumpEvents, 0);
    }

    TEST(PlayerSliceModelTests, DeadPlayerJumpPressIsRejected)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        ASSERT_TRUE(model.ApplyDamage(model.GetPlayer().m_maxHealth));
        PlayerInput input; input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_TRUE(model.GetDesiredVelocity(input).IsZero());
        EXPECT_EQ(model.GetPlayer().m_jumpEvents, 0);
    }

    TEST(PlayerSliceModelTests, ReleaseAndRenewedGroundingRearmJumpWithPlanarMovement)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        model.SynchronizePhysicalState(AZ::Vector3(0.0f, 0.0f, 1.0f), false);
        input.m_jump = false;
        ASSERT_TRUE(model.Update(0.016f, input));
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        const AZ::Vector3 velocity = model.GetDesiredVelocity(input);
        EXPECT_NEAR(AZ::Vector3(velocity.GetX(), velocity.GetY(), 0.0f).GetLength(), PlayerSliceModel::WalkSpeed, 0.001f);
        EXPECT_FLOAT_EQ(velocity.GetZ(), PlayerSliceModel::JumpImpulseSpeed);
        EXPECT_EQ(model.GetPlayer().m_jumpEvents, 2);
    }

    TEST(PlayerSliceModelTests, GroundedCrouchRequestIsAccepted)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_TRUE(model.GetPlayer().m_crouchDesired);
    }

    TEST(PlayerSliceModelTests, CrouchReleaseRequestsStanding)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        input.m_crouch = false;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FALSE(model.GetPlayer().m_crouchDesired);
    }

    TEST(PlayerSliceModelTests, DeadPlayerCannotBeginCrouch)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        ASSERT_TRUE(model.ApplyDamage(model.GetPlayer().m_maxHealth));
        PlayerInput input; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FALSE(model.GetPlayer().m_crouchDesired);
    }

    TEST(PlayerSliceModelTests, CrouchPreservesPlanarDesiredVelocity)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        input.m_forward = 1.0f;
        input.m_strafe = 1.0f;
        ASSERT_TRUE(model.Update(0.016f, input));
        const AZ::Vector3 velocity = model.GetDesiredVelocity(input);
        EXPECT_NEAR(AZ::Vector3(velocity.GetX(), velocity.GetY(), 0.0f).GetLength(), PlayerSliceModel::WalkSpeed, 0.001f);
    }

    TEST(PlayerSliceModelTests, HeldCrouchRemainsDeterministic)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_TRUE(model.GetPlayer().m_crouchDesired);
    }

    TEST(PlayerSliceModelTests, CrouchDoesNotChangeGroundedJumpImpulse)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_crouch = true; input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FLOAT_EQ(model.GetDesiredVelocity(input).GetZ(), PlayerSliceModel::JumpImpulseSpeed);
        EXPECT_EQ(model.GetPlayer().m_jumpEvents, 1);
    }

    TEST(PlayerSliceModelTests, GroundedMovingFreshCrouchPressStartsSlide)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.0f, input));
        EXPECT_TRUE(model.GetPlayer().m_slideActive);
        EXPECT_EQ(model.GetPlayer().m_slideEvents, 1);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_slideSpeed, PlayerSliceModel::SlideStartSpeed);
    }

    TEST(PlayerSliceModelTests, StationaryCrouchDoesNotStartSlide)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FALSE(model.GetPlayer().m_slideActive);
        EXPECT_TRUE(model.GetPlayer().m_crouchDesired);
    }

    TEST(PlayerSliceModelTests, AirbornePlayerCannotStartSlide)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateAxisZ(1.0f), false);
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FALSE(model.GetPlayer().m_slideActive);
        EXPECT_EQ(model.GetPlayer().m_slideEvents, 0);
    }

    TEST(PlayerSliceModelTests, DeadPlayerCannotStartSlide)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        ASSERT_TRUE(model.ApplyDamage(model.GetPlayer().m_maxHealth));
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FALSE(model.GetPlayer().m_slideActive);
        EXPECT_EQ(model.GetPlayer().m_slideEvents, 0);
    }

    TEST(PlayerSliceModelTests, SlideSpeedDecaysDeterministically)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.0f, input));
        ASSERT_TRUE(model.Update(PlayerSliceModel::SlideDuration * 0.5f, input));
        EXPECT_NEAR(
            model.GetPlayer().m_slideSpeed,
            0.5f * (PlayerSliceModel::SlideStartSpeed + PlayerSliceModel::SlideEndSpeed),
            0.001f);
    }

    TEST(PlayerSliceModelTests, SlideTerminatesAfterBoundedDuration)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.0f, input));
        ASSERT_TRUE(model.Update(PlayerSliceModel::SlideDuration, input));
        EXPECT_FALSE(model.GetPlayer().m_slideActive);
        EXPECT_FLOAT_EQ(model.GetPlayer().m_slideSpeed, 0.0f);
    }

    TEST(PlayerSliceModelTests, SlidePreservesCapturedPlanarDirection)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_strafe = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.0f, input));
        const AZ::Vector3 initialDirection = model.GetDesiredVelocity(input).GetNormalized();
        input.m_forward = 0.2f;
        input.m_strafe = 1.0f;
        ASSERT_TRUE(model.Update(0.1f, input));
        EXPECT_TRUE(model.GetDesiredVelocity(input).GetNormalized().IsClose(initialDirection, 0.001f));
        EXPECT_NEAR(model.GetDesiredVelocity(input).GetZ(), 0.0f, 0.001f);
    }

    TEST(PlayerSliceModelTests, HeldCrouchDoesNotRetriggerSlide)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.0f, input));
        Advance(model, PlayerSliceModel::SlideDuration + 0.1f, input);
        EXPECT_FALSE(model.GetPlayer().m_slideActive);
        EXPECT_EQ(model.GetPlayer().m_slideEvents, 1);
    }

    TEST(PlayerSliceModelTests, SlideExitCanRemainCrouched)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.0f, input));
        ASSERT_TRUE(model.Update(PlayerSliceModel::SlideDuration, input));
        EXPECT_FALSE(model.GetPlayer().m_slideActive);
        EXPECT_TRUE(model.GetPlayer().m_crouchDesired);
    }

    TEST(PlayerSliceModelTests, GroundedJumpRemainsValidAfterSlideEnds)
    {
        PlayerSliceModel model;
        model.SynchronizePhysicalState(AZ::Vector3::CreateZero(), true);
        PlayerInput input; input.m_forward = 1.0f; input.m_crouch = true;
        ASSERT_TRUE(model.Update(0.0f, input));
        ASSERT_TRUE(model.Update(PlayerSliceModel::SlideDuration, input));
        input.m_crouch = false;
        input.m_jump = true;
        ASSERT_TRUE(model.Update(0.016f, input));
        EXPECT_FLOAT_EQ(model.GetDesiredVelocity(input).GetZ(), PlayerSliceModel::JumpImpulseSpeed);
        EXPECT_EQ(model.GetPlayer().m_jumpEvents, 1);
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
                Advance(model, PlayerSliceModel::FireInterval + 0.001f);
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

    TEST(PlayerSliceModelTests, InvalidPhysicalSynchronizationIsTransactional)
    {
        PlayerSliceModel model;
        const PlayerState before = model.GetPlayer();
        model.SynchronizePhysicalState(AZ::Vector3(NAN, 0.0f, 0.0f), true);
        EXPECT_TRUE(model.GetPlayer().m_position.IsClose(before.m_position));
        EXPECT_EQ(model.GetPlayer().m_grounded, before.m_grounded);
    }
}
