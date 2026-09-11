#include <AzTest/AzTest.h>

#include <cmath>
#include <limits>

#include <STWGameplay/FixedSimulationClock.h>

namespace STWGameplay
{
    TEST(FixedSimulationClockTests, FixedDeltaProducesOneSimulationStep)
    {
        FixedSimulationClock clock;

        const FixedSimulationAdvanceResult result = clock.Advance(FixedSimulationClock::FixedDeltaTime);

        EXPECT_TRUE(result.m_inputValid);
        EXPECT_EQ(result.m_stepCount, 1u);
        EXPECT_EQ(clock.GetSimulationTick(), 1u);
        EXPECT_FLOAT_EQ(clock.GetInterpolationAlpha(), 0.0f);
    }

    TEST(FixedSimulationClockTests, FractionalTimeProducesBoundedInterpolationAlpha)
    {
        FixedSimulationClock clock;

        EXPECT_EQ(clock.Advance(FixedSimulationClock::FixedDeltaTime * 0.25f).m_stepCount, 0u);
        EXPECT_NEAR(clock.GetInterpolationAlpha(), 0.25f, 0.0001f);

        EXPECT_EQ(clock.Advance(FixedSimulationClock::FixedDeltaTime * 0.25f).m_stepCount, 0u);
        EXPECT_NEAR(clock.GetInterpolationAlpha(), 0.5f, 0.0001f);

        EXPECT_EQ(clock.Advance(FixedSimulationClock::FixedDeltaTime * 0.5f).m_stepCount, 1u);
        EXPECT_FLOAT_EQ(clock.GetInterpolationAlpha(), 0.0f);
        EXPECT_EQ(clock.GetSimulationTick(), 1u);
    }

    TEST(FixedSimulationClockTests, MultipleFixedStepsAdvanceTheSimulationTick)
    {
        FixedSimulationClock clock;

        const FixedSimulationAdvanceResult result = clock.Advance(FixedSimulationClock::FixedDeltaTime * 3.0f);

        EXPECT_TRUE(result.m_inputValid);
        EXPECT_EQ(result.m_stepCount, 3u);
        EXPECT_EQ(clock.GetSimulationTick(), 3u);
        EXPECT_FLOAT_EQ(clock.GetInterpolationAlpha(), 0.0f);
    }

    TEST(FixedSimulationClockTests, CatchUpIsBoundedAndReported)
    {
        FixedSimulationClock clock;

        const FixedSimulationAdvanceResult result =
            clock.Advance(FixedSimulationClock::MaximumFrameDelta * 2.0f);

        EXPECT_TRUE(result.m_inputValid);
        EXPECT_EQ(result.m_stepCount, FixedSimulationClock::MaxSubstepsPerFrame);
        EXPECT_TRUE(result.m_catchUpClamped);
        EXPECT_TRUE(clock.WasCatchUpClamped());
        EXPECT_GE(clock.GetDroppedSimulationTime(), FixedSimulationClock::FixedDeltaTime);
        EXPECT_GE(clock.GetInterpolationAlpha(), 0.0f);
        EXPECT_LE(clock.GetInterpolationAlpha(), 1.0f);
    }

    TEST(FixedSimulationClockTests, InvalidFrameDeltaDoesNotMutateClock)
    {
        FixedSimulationClock clock;
        clock.Advance(FixedSimulationClock::FixedDeltaTime * 0.25f);

        const AZ::u64 tickBefore = clock.GetSimulationTick();
        const float accumulatorBefore = clock.GetAccumulator();
        const float droppedTimeBefore = clock.GetDroppedSimulationTime();

        for (const float invalidDelta : {
                 -1.0f,
                 std::numeric_limits<float>::quiet_NaN(),
                 std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity() })
        {
            const FixedSimulationAdvanceResult result = clock.Advance(invalidDelta);
            EXPECT_FALSE(result.m_inputValid);
            EXPECT_EQ(result.m_stepCount, 0u);
            EXPECT_FALSE(result.m_catchUpClamped);
            EXPECT_EQ(clock.GetSimulationTick(), tickBefore);
            EXPECT_FLOAT_EQ(clock.GetAccumulator(), accumulatorBefore);
            EXPECT_FLOAT_EQ(clock.GetDroppedSimulationTime(), droppedTimeBefore);
        }
    }

    TEST(FixedSimulationClockTests, ResetClearsTimingState)
    {
        FixedSimulationClock clock;
        clock.Advance(FixedSimulationClock::FixedDeltaTime * 2.5f);
        EXPECT_NE(clock.GetSimulationTick(), 0u);

        clock.Reset();

        EXPECT_EQ(clock.GetSimulationTick(), 0u);
        EXPECT_FLOAT_EQ(clock.GetAccumulator(), 0.0f);
        EXPECT_FLOAT_EQ(clock.GetInterpolationAlpha(), 0.0f);
        EXPECT_FLOAT_EQ(clock.GetDroppedSimulationTime(), 0.0f);
        EXPECT_FALSE(clock.WasCatchUpClamped());
    }
}
