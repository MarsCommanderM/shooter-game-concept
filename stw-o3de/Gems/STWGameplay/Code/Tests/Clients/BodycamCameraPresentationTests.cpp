#include <gtest/gtest.h>

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

    TEST(BodycamCameraPresentationTests, ResetClearsAccumulatedPresentationState)
    {
        BodycamCameraPresentation bodycam;
        BodycamPresentationInput input = MovingInput();
        input.m_lookX = 12.0f;
        bodycam.Update(1.0f / 60.0f, input);
        EXPECT_FALSE(bodycam.IsNearNeutral());
        bodycam.ResetToNeutral();
        EXPECT_TRUE(bodycam.IsNearNeutral());
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
}
