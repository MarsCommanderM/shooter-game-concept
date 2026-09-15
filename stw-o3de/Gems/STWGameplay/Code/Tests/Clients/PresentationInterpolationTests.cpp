#include <AzTest/AzTest.h>

#include <cmath>
#include <limits>

#include <STWGameplay/PresentationInterpolation.h>

namespace STWGameplay
{
    namespace
    {
        PresentationFrameState MakeState(float x, float y, float z, float yaw = 0.0f, float pitch = 0.0f)
        {
            PresentationFrameState state;
            state.m_position = AZ::Vector3(x, y, z);
            state.m_yaw = yaw;
            state.m_pitch = pitch;
            return state;
        }
    }

    TEST(PresentationInterpolationTests, InitialEvaluationIsSafeAndEmpty)
    {
        PresentationInterpolation interpolation;

        const PresentationFrameState state = interpolation.Evaluate(0.5f);

        EXPECT_FALSE(interpolation.HasState());
        EXPECT_TRUE(state.m_position.IsClose(AZ::Vector3::CreateZero()));
        EXPECT_FLOAT_EQ(state.m_yaw, 0.0f);
        EXPECT_FLOAT_EQ(state.m_pitch, 0.0f);
    }

    TEST(PresentationInterpolationTests, FirstAdvanceSeedsBothStateSamples)
    {
        PresentationInterpolation interpolation;
        const PresentationFrameState source = MakeState(1.0f, 2.0f, 3.0f, 0.25f, -0.5f);

        ASSERT_TRUE(interpolation.Advance(source));

        EXPECT_TRUE(interpolation.HasState());
        EXPECT_TRUE(interpolation.GetPreviousState().m_position.IsClose(source.m_position));
        EXPECT_TRUE(interpolation.GetCurrentState().m_position.IsClose(source.m_position));
        EXPECT_TRUE(interpolation.Evaluate(0.0f).m_position.IsClose(source.m_position));
        EXPECT_TRUE(interpolation.Evaluate(1.0f).m_position.IsClose(source.m_position));
    }

    TEST(PresentationInterpolationTests, PositionAndAnglesUseClampedInterpolation)
    {
        PresentationInterpolation interpolation;
        ASSERT_TRUE(interpolation.Advance(MakeState(0.0f, 0.0f, 0.0f, 0.0f, -0.5f)));
        ASSERT_TRUE(interpolation.Advance(MakeState(4.0f, 8.0f, 12.0f, 1.0f, 0.5f)));

        const PresentationFrameState atZero = interpolation.Evaluate(0.0f);
        const PresentationFrameState atQuarter = interpolation.Evaluate(0.25f);
        const PresentationFrameState atHalf = interpolation.Evaluate(0.5f);
        const PresentationFrameState atThreeQuarter = interpolation.Evaluate(0.75f);
        const PresentationFrameState atOne = interpolation.Evaluate(1.0f);

        EXPECT_TRUE(atZero.m_position.IsClose(AZ::Vector3::CreateZero()));
        EXPECT_TRUE(atQuarter.m_position.IsClose(AZ::Vector3(1.0f, 2.0f, 3.0f)));
        EXPECT_TRUE(atHalf.m_position.IsClose(AZ::Vector3(2.0f, 4.0f, 6.0f)));
        EXPECT_TRUE(atThreeQuarter.m_position.IsClose(AZ::Vector3(3.0f, 6.0f, 9.0f)));
        EXPECT_TRUE(atOne.m_position.IsClose(AZ::Vector3(4.0f, 8.0f, 12.0f)));
        EXPECT_FLOAT_EQ(atHalf.m_yaw, 0.5f);
        EXPECT_NEAR(atHalf.m_pitch, 0.0f, 1e-6f);

        EXPECT_TRUE(interpolation.Evaluate(-1.0f).m_position.IsClose(atZero.m_position));
        EXPECT_TRUE(interpolation.Evaluate(2.0f).m_position.IsClose(atOne.m_position));
    }

    TEST(PresentationInterpolationTests, YawWrapUsesShortestVisualPath)
    {
        PresentationInterpolation interpolation;
        constexpr float pi = 3.14159265358979323846f;
        ASSERT_TRUE(interpolation.Advance(MakeState(0.0f, 0.0f, 0.0f, 179.0f * pi / 180.0f)));
        ASSERT_TRUE(interpolation.Advance(MakeState(0.0f, 0.0f, 0.0f, -179.0f * pi / 180.0f)));

        const float halfwayYaw = interpolation.Evaluate(0.5f).m_yaw;

        EXPECT_NEAR(std::abs(halfwayYaw), pi, 0.0001f);
    }

    TEST(PresentationInterpolationTests, NonFiniteSourceIsRejectedWithoutMutation)
    {
        PresentationInterpolation interpolation;
        const PresentationFrameState valid = MakeState(1.0f, 2.0f, 3.0f);
        ASSERT_TRUE(interpolation.Advance(valid));

        PresentationFrameState invalid = valid;
        invalid.m_position.SetX(std::numeric_limits<float>::quiet_NaN());
        EXPECT_FALSE(interpolation.Advance(invalid));
        EXPECT_TRUE(interpolation.GetPreviousState().m_position.IsClose(valid.m_position));
        EXPECT_TRUE(interpolation.GetCurrentState().m_position.IsClose(valid.m_position));

        invalid = valid;
        invalid.m_yaw = std::numeric_limits<float>::infinity();
        EXPECT_FALSE(interpolation.Advance(invalid));
        EXPECT_TRUE(interpolation.Evaluate(std::numeric_limits<float>::quiet_NaN()).m_position.IsFinite());
        EXPECT_TRUE(interpolation.Evaluate(std::numeric_limits<float>::infinity()).m_position.IsFinite());
        EXPECT_TRUE(interpolation.Evaluate(-std::numeric_limits<float>::infinity()).m_position.IsFinite());
    }

    TEST(PresentationInterpolationTests, ResetHardSnapsAndIsIdempotent)
    {
        PresentationInterpolation interpolation;
        ASSERT_TRUE(interpolation.Advance(MakeState(1.0f, 0.0f, 0.0f)));
        ASSERT_TRUE(interpolation.Advance(MakeState(5.0f, 0.0f, 0.0f)));
        const PresentationFrameState resetState = MakeState(20.0f, 4.0f, -2.0f, 0.75f, -0.25f);

        interpolation.Reset(resetState);
        interpolation.Reset(resetState);

        const PresentationFrameState evaluated = interpolation.Evaluate(0.5f);
        EXPECT_TRUE(evaluated.m_position.IsClose(resetState.m_position));
        EXPECT_FLOAT_EQ(evaluated.m_yaw, resetState.m_yaw);
        EXPECT_FLOAT_EQ(evaluated.m_pitch, resetState.m_pitch);
        EXPECT_TRUE(interpolation.GetPreviousState().m_position.IsClose(resetState.m_position));
        EXPECT_TRUE(interpolation.GetCurrentState().m_position.IsClose(resetState.m_position));
    }
}
