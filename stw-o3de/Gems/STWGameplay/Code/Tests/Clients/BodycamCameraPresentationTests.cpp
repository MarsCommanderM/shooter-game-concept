#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <STWGameplay/BodycamCameraPresentation.h>
#include <STWGameplay/PlayerSliceModel.h>

namespace STWGameplay
{
    namespace
    {
        BodycamPresentationInput MovingInput()
        {
            BodycamPresentationInput input;
            input.m_speed = 7.5f;
            input.m_lateralInput = 1.0f;
            input.m_sprinting = true;
            return input;
        }
    }

    TEST(BodycamCameraPresentationTests, NeutralStateHasNoPresentationDelta)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_TRUE(bodycam.IsNearNeutral());
    }

    TEST(BodycamCameraPresentationTests, LookInertiaRespondsAndRecovers)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_lookX = 12.0f;
        bodycam.Update(1.0f / 60.0f, input);
        const float response = bodycam.GetLookInertiaMagnitude();
        EXPECT_GT(response, 0.0f);
        input.m_lookX = 0.0f;
        for (int index = 0; index < 180; ++index)
        {
            bodycam.Update(1.0f / 60.0f, input);
        }
        EXPECT_LT(bodycam.GetLookInertiaMagnitude(), response);
        EXPECT_TRUE(bodycam.IsNearNeutral());
    }

    TEST(BodycamCameraPresentationTests, LateralRollIsClampedAndReturnsToNeutral)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input = MovingInput();
        for (int index = 0; index < 120; ++index)
        {
            bodycam.Update(1.0f / 60.0f, input);
        }
        const BodycamPresentationTuning tuning = BodycamCameraPresentation::GetStandardTuning();
        EXPECT_GT(std::abs(bodycam.GetRollRadians()), 0.0f);
        EXPECT_LE(std::abs(bodycam.GetRollRadians()), tuning.m_rollLimitRadians);
        input.m_lateralInput = 0.0f;
        input.m_speed = 0.0f;
        input.m_sprinting = false;
        for (int index = 0; index < 180; ++index)
        {
            bodycam.Update(1.0f / 60.0f, input);
        }
        EXPECT_NEAR(bodycam.GetRollRadians(), 0.0f, 0.0001f);
    }

    TEST(BodycamCameraPresentationTests, SprintResponseIsStrongerThanNormalLocomotion)
    {
        BodycamCameraPresentation normal;
        BodycamPresentationInput normalInput;
        normalInput.m_speed = PlayerSliceModel::WalkSpeed;
        for (int index = 0; index < 30; ++index)
        {
            normal.Update(1.0f / 60.0f, normalInput);
        }

        BodycamCameraPresentation sprint;
        BodycamPresentationInput sprintInput = normalInput;
        sprintInput.m_speed = PlayerSliceModel::SprintSpeed;
        sprintInput.m_sprinting = true;
        for (int index = 0; index < 30; ++index)
        {
            sprint.Update(1.0f / 60.0f, sprintInput);
        }
        EXPECT_GT(sprint.GetLocomotionResponse(), normal.GetLocomotionResponse());
    }

    TEST(BodycamCameraPresentationTests, AdsSuppressesCameraMotion)
    {
        BodycamCameraPresentation hip;
        BodycamPresentationInput hipInput = MovingInput();
        hipInput.m_lookX = 12.0f;
        hip.Update(1.0f / 60.0f, hipInput);

        BodycamCameraPresentation ads;
        BodycamPresentationInput adsInput = hipInput;
        adsInput.m_ads = true;
        ads.Update(1.0f / 60.0f, adsInput);
        EXPECT_LT(ads.GetMotionMagnitude(), hip.GetMotionMagnitude());
        EXPECT_TRUE(ads.WasAdsSuppressionObserved());
    }

    TEST(BodycamCameraPresentationTests, ReducedMotionPresetReducesConfiguredAmplitudes)
    {
        const BodycamPresentationTuning standard = BodycamCameraPresentation::GetStandardTuning();
        const BodycamPresentationTuning reduced = BodycamCameraPresentation::GetReducedMotionTuning();
        EXPECT_TRUE(BodycamCameraPresentation::IsReducedMotionProfileEffective());
        EXPECT_LT(reduced.m_rollLimitRadians, standard.m_rollLimitRadians);
        EXPECT_LT(reduced.m_locomotionVerticalMeters, standard.m_locomotionVerticalMeters);
        EXPECT_LT(reduced.m_landingVerticalMeters, standard.m_landingVerticalMeters);
    }

    TEST(BodycamCameraPresentationTests, AccelerationProducesBoundedCameraLag)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_planarAcceleration = AZ::Vector3(0.0f, 34.0f, 0.0f);
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_TRUE(bodycam.WasAccelerationResponseObserved());
        EXPECT_LT(bodycam.GetAccelerationOffset().GetY(), 0.0f);
        const BodycamPresentationTuning tuning = BodycamCameraPresentation::GetStandardTuning();
        EXPECT_LE(
            bodycam.GetAccelerationOffset().GetLength(),
            tuning.m_accelerationLagMeters + tuning.m_accelerationVerticalMeters + 0.001f);
    }

    TEST(BodycamCameraPresentationTests, AccelerationReturnsTowardNeutral)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_planarAcceleration = AZ::Vector3(0.0f, 30.0f, 0.0f);
        for (int index = 0; index < 10; ++index)
        {
            bodycam.Update(1.0f / 60.0f, input);
        }
        const float displaced = bodycam.GetAccelerationOffset().GetLength();
        ASSERT_GT(displaced, 0.0f);
        input.m_planarAcceleration = AZ::Vector3::CreateZero();
        for (int index = 0; index < 180; ++index)
        {
            bodycam.Update(1.0f / 60.0f, input);
        }
        EXPECT_LT(bodycam.GetAccelerationOffset().GetLength(), displaced);
        EXPECT_LT(bodycam.GetAccelerationOffset().GetLength(), 0.001f);
    }

    TEST(BodycamCameraPresentationTests, AcceptedShotSignalProducesPresentationRecoil)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_shotFired = true;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_TRUE(bodycam.WasRecoilResponseObserved());
        EXPECT_GT(bodycam.GetRecoilPitchRadians(), 0.0f);
        EXPECT_GT(bodycam.GetRecoilBackMeters(), 0.0f);
    }

    TEST(BodycamCameraPresentationTests, NoShotSignalProducesNoRecoil)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_shotFired = false;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_FLOAT_EQ(bodycam.GetRecoilPitchRadians(), 0.0f);
        EXPECT_FLOAT_EQ(bodycam.GetRecoilBackMeters(), 0.0f);
    }

    TEST(BodycamCameraPresentationTests, RecoilRecoversWithoutAdditionalShot)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_shotFired = true;
        bodycam.Update(1.0f / 60.0f, input);
        const float kicked = bodycam.GetRecoilPitchRadians();
        ASSERT_GT(kicked, 0.0f);
        input.m_shotFired = false;
        for (int index = 0; index < 180; ++index)
        {
            bodycam.Update(1.0f / 60.0f, input);
        }
        EXPECT_LT(bodycam.GetRecoilPitchRadians(), kicked);
        EXPECT_LT(bodycam.GetRecoilPitchRadians(), 0.001f);
    }

    TEST(BodycamCameraPresentationTests, CameraFovTracksAdsBlend)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput hip;
        hip.m_adsBlend = 0.0f;
        bodycam.Update(0.0f, hip);
        EXPECT_NEAR(bodycam.GetCameraFovDegrees(), 60.0f, 0.001f);

        BodycamPresentationInput ads;
        ads.m_ads = true;
        ads.m_adsBlend = 1.0f;
        bodycam.Update(0.0f, ads);
        EXPECT_NEAR(bodycam.GetCameraFovDegrees(), 52.0f, 0.001f);
    }

    TEST(BodycamCameraPresentationTests, ReducedMotionReducesAccelerationAndRecoil)
    {
        BodycamCameraPresentation standard;
        BodycamCameraPresentation reduced;
        reduced.SetProfile(BodycamPresentationProfile::ReducedMotion);
        BodycamPresentationInput input;
        input.m_planarAcceleration = AZ::Vector3(20.0f, 30.0f, 0.0f);
        input.m_shotFired = true;
        standard.Update(1.0f / 60.0f, input);
        reduced.Update(1.0f / 60.0f, input);
        EXPECT_LT(reduced.GetAccelerationOffset().GetLength(), standard.GetAccelerationOffset().GetLength());
        EXPECT_LT(reduced.GetRecoilPitchRadians(), standard.GetRecoilPitchRadians());
    }

    TEST(BodycamCameraPresentationTests, ResetClearsAccumulatedPresentationState)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input = MovingInput();
        input.m_lookX = 12.0f;
        input.m_ads = true;
        input.m_adsBlend = 1.0f;
        input.m_planarAcceleration = AZ::Vector3(0.0f, 30.0f, 0.0f);
        input.m_shotFired = true;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_FALSE(bodycam.IsNearNeutral());
        bodycam.ResetToNeutral();
        EXPECT_TRUE(bodycam.IsNearNeutral());
        EXPECT_TRUE(bodycam.GetAccelerationOffset().IsZero());
        EXPECT_FLOAT_EQ(bodycam.GetRecoilPitchRadians(), 0.0f);
        EXPECT_FLOAT_EQ(bodycam.GetRecoilBackMeters(), 0.0f);
        EXPECT_FLOAT_EQ(bodycam.GetCameraFovDegrees(), 60.0f);
        EXPECT_TRUE(bodycam.WasResetToNeutralObserved());
    }

    TEST(BodycamCameraPresentationTests, DeathAndRespawnResetStateDeterministically)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input = MovingInput();
        bodycam.Update(1.0f / 60.0f, input);
        input.m_alive = false;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_TRUE(bodycam.IsNearNeutral());
        input.m_alive = true;
        input.m_respawnEvents = 1;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_TRUE(bodycam.IsNearNeutral());
    }

    TEST(BodycamCameraPresentationTests, RepeatedInputsAreDeterministic)
    {
        BodycamCameraPresentation first;
        BodycamCameraPresentation second;
        BodycamPresentationInput input = MovingInput();
        input.m_lookX = 4.0f;
        input.m_mantling = true;
        input.m_mantleProgress = 0.4f;
        for (int index = 0; index < 45; ++index)
        {
            first.Update(1.0f / 60.0f, input);
            second.Update(1.0f / 60.0f, input);
        }
        EXPECT_TRUE(first.GetCameraPositionOffset().IsClose(second.GetCameraPositionOffset()));
        EXPECT_TRUE(first.GetCameraRotationOffset().IsClose(second.GetCameraRotationOffset()));
    }

    TEST(BodycamCameraPresentationTests, InputSnapshotRemainsUnchanged)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input = MovingInput();
        input.m_lookX = 5.0f;
        const BodycamPresentationInput before = input;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_EQ(input.m_lookX, before.m_lookX);
        EXPECT_EQ(input.m_speed, before.m_speed);
        EXPECT_EQ(input.m_sprinting, before.m_sprinting);
        EXPECT_EQ(input.m_ads, before.m_ads);
        EXPECT_EQ(input.m_respawnEvents, before.m_respawnEvents);
    }

    TEST(BodycamCameraPresentationTests, NonFiniteInputCannotPoisonCameraState)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input = MovingInput();
        input.m_lookX = std::numeric_limits<float>::infinity();
        input.m_lookY = std::numeric_limits<float>::quiet_NaN();
        input.m_speed = std::numeric_limits<float>::infinity();
        input.m_lateralInput = std::numeric_limits<float>::quiet_NaN();
        input.m_mantleProgress = std::numeric_limits<float>::quiet_NaN();
        input.m_adsBlend = std::numeric_limits<float>::infinity();
        bodycam.Update(1.0f / 60.0f, input);

        EXPECT_TRUE(bodycam.GetCameraPositionOffset().IsFinite());
        EXPECT_TRUE(bodycam.GetCameraRotationOffset().IsFinite());
        EXPECT_TRUE(std::isfinite(bodycam.GetCameraFovDegrees()));
        EXPECT_TRUE(std::isfinite(bodycam.GetRecoilPitchRadians()));
        EXPECT_TRUE(std::isfinite(bodycam.GetRecoilBackMeters()));
    }

    TEST(BodycamCameraPresentationTests, LandingPulseTriggersOnceAndDoesNotRetriggerWhileGrounded)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_grounded = false;
        bodycam.Update(1.0f / 60.0f, input);
        input.m_grounded = true;
        bodycam.Update(1.0f / 60.0f, input);
        const float landingOffset = bodycam.GetCameraPositionOffset().GetZ();
        ASSERT_LT(landingOffset, 0.0f);

        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_GT(bodycam.GetCameraPositionOffset().GetZ(), landingOffset);
        EXPECT_TRUE(bodycam.GetCameraPositionOffset().IsFinite());
    }

    TEST(BodycamCameraPresentationTests, MantleResponseIsPresentationOnlyAndBounded)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input;
        input.m_mantling = true;
        input.m_mantleProgress = 0.5f;
        bodycam.Update(1.0f / 60.0f, input);

        EXPECT_GT(bodycam.GetCameraPositionOffset().GetZ(), 0.0f);
        EXPECT_LE(
            bodycam.GetCameraPositionOffset().GetZ(),
            BodycamCameraPresentation::GetStandardTuning().m_mantleVerticalMeters);
    }

    TEST(BodycamCameraPresentationTests, ConstantAccelerationResponseIsStableAcrossValidFrameChunking)
    {
        BodycamCameraPresentation coarse;
        BodycamCameraPresentation fine;
        BodycamPresentationInput input;
        input.m_planarAcceleration = AZ::Vector3(8.0f, 16.0f, 0.0f);

        coarse.Update(0.1f, input);
        for (int index = 0; index < 6; ++index)
        {
            fine.Update(1.0f / 60.0f, input);
        }

        EXPECT_TRUE(coarse.GetAccelerationOffset().IsClose(fine.GetAccelerationOffset(), 0.00001f));
    }
}
