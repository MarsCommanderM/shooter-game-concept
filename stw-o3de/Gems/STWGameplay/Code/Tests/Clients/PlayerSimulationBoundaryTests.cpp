#include <AzTest/AzTest.h>

#include <limits>

#include <STWGameplay/PlayerSimulationTypes.h>

namespace STWGameplay
{
    TEST(PlayerSimulationBoundaryTests, CommandCopyPreservesSampledInputAndAssignsSequence)
    {
        PlayerInput sampledInput;
        sampledInput.m_forward = 1.25f;
        sampledInput.m_strafe = -0.75f;
        sampledInput.m_lookX = 4.0f;
        sampledInput.m_lookY = -2.0f;
        sampledInput.m_sprint = true;
        sampledInput.m_fire = true;
        sampledInput.m_requestedEquipmentSlot = 2;

        const PlayerCommand command = MakePlayerCommand(sampledInput, 17u);
        EXPECT_FLOAT_EQ(command.m_forward, sampledInput.m_forward);
        EXPECT_FLOAT_EQ(command.m_strafe, sampledInput.m_strafe);
        EXPECT_FLOAT_EQ(command.m_lookX, sampledInput.m_lookX);
        EXPECT_FLOAT_EQ(command.m_lookY, sampledInput.m_lookY);
        EXPECT_TRUE(command.m_sprint);
        EXPECT_TRUE(command.m_fire);
        EXPECT_EQ(command.m_requestedEquipmentSlot, 2);
        EXPECT_EQ(command.m_sequence, 17u);
    }

    TEST(PlayerSimulationBoundaryTests, CommandRejectsNonFiniteInput)
    {
        PlayerCommand command;
        EXPECT_TRUE(command.IsFinite());
        command.m_lookX = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(command.IsFinite());
        command.m_lookX = 0.0f;
        command.m_lookY = std::numeric_limits<float>::infinity();
        EXPECT_FALSE(command.IsFinite());
    }

    TEST(PlayerSimulationBoundaryTests, SequenceAdvancementReservesZeroAcrossWrap)
    {
        EXPECT_EQ(AdvancePlayerSimulationSequence(0u), 1u);
        EXPECT_EQ(AdvancePlayerSimulationSequence(std::numeric_limits<AZ::u32>::max()), 1u);
        EXPECT_NE(AdvancePlayerSimulationSequence(std::numeric_limits<AZ::u32>::max()), 0u);
    }

    TEST(PlayerSimulationBoundaryTests, SequenceComparisonHandlesWrapAndEqualValues)
    {
        constexpr AZ::u32 maximum = std::numeric_limits<AZ::u32>::max();
        EXPECT_TRUE(IsNewerPlayerSimulationSequence(maximum - 1u, maximum - 2u));
        EXPECT_TRUE(IsNewerPlayerSimulationSequence(maximum, maximum - 1u));
        EXPECT_TRUE(IsNewerPlayerSimulationSequence(1u, maximum));
        EXPECT_FALSE(IsNewerPlayerSimulationSequence(maximum, 1u));
        EXPECT_FALSE(IsNewerPlayerSimulationSequence(7u, 7u));
        EXPECT_FALSE(IsNewerPlayerSimulationSequence(0u, maximum));
        EXPECT_TRUE(IsNewerPlayerSimulationSequence(1u, 0u));
    }

    TEST(PlayerSimulationBoundaryTests, HalfRangeDistanceIsNotDeclaredNewer)
    {
        constexpr PlayerSimulationSequence reference = 1u;
        constexpr PlayerSimulationSequence halfRangeCandidate = reference + PlayerSimulationSequenceHalfRange;

        EXPECT_FALSE(IsNewerPlayerSimulationSequence(halfRangeCandidate, reference));
        EXPECT_FALSE(IsNewerPlayerSimulationSequence(reference, halfRangeCandidate));
    }

    TEST(PlayerSimulationBoundaryTests, SnapshotKeepsCommandAckAndPhysicalReadbackIdentitySeparate)
    {
        AuthoritativePlayerSnapshot snapshot;
        snapshot.m_acknowledgedCommandSequence = 17u;
        snapshot.m_physicalReadbackSequence = 4u;
        snapshot.m_physicalStateSynchronized = true;
        snapshot.m_position = AZ::Vector3(1.0f, 2.0f, 3.0f);
        snapshot.m_requestedSimulationVelocity = AZ::Vector3(4.0f, 5.0f, 0.0f);

        EXPECT_TRUE(snapshot.m_physicalStateSynchronized);
        EXPECT_NE(snapshot.m_acknowledgedCommandSequence, snapshot.m_physicalReadbackSequence);
        EXPECT_TRUE(snapshot.m_position.IsFinite());
        EXPECT_TRUE(snapshot.m_requestedSimulationVelocity.IsFinite());
    }
}
