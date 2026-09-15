#include <AzTest/AzTest.h>

#include <limits>

#include <STWGameplay/PlayerCommandHistory.h>

namespace STWGameplay
{
    namespace
    {
        PlayerCommand MakeCommand(PlayerCommandSequence sequence)
        {
            PlayerInput input;
            input.m_forward = static_cast<float>(sequence);
            return MakePlayerCommand(input, sequence);
        }
    }

    TEST(PlayerCommandHistoryTests, EmptyLookupAndAcknowledgementAreSafe)
    {
        PlayerCommandHistory history;
        PlayerCommand command;
        EXPECT_TRUE(history.Empty());
        EXPECT_EQ(history.Size(), 0u);
        EXPECT_FALSE(history.TryGet(1u, command));
        EXPECT_EQ(history.DiscardThrough(1u), 0u);
        EXPECT_TRUE(history.HasAcknowledgement());
    }

    TEST(PlayerCommandHistoryTests, StoresAndLooksUpCommandsInSequenceOrder)
    {
        PlayerCommandHistory history;
        EXPECT_TRUE(history.Push(MakeCommand(1u)));
        EXPECT_TRUE(history.Push(MakeCommand(2u)));

        PlayerCommand command;
        EXPECT_TRUE(history.TryGet(1u, command));
        EXPECT_FLOAT_EQ(command.m_forward, 1.0f);
        EXPECT_TRUE(history.TryGetAt(1u, command));
        EXPECT_EQ(command.m_sequence, 2u);
        EXPECT_FALSE(history.TryGet(3u, command));
    }

    TEST(PlayerCommandHistoryTests, RejectsInvalidDuplicateAndStaleSequences)
    {
        PlayerCommandHistory history;
        EXPECT_FALSE(history.Push(MakeCommand(0u)));
        PlayerCommand nonFinite = MakeCommand(1u);
        nonFinite.m_lookX = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(history.Push(nonFinite));
        EXPECT_TRUE(history.Push(MakeCommand(3u)));

        PlayerCommand duplicate = MakeCommand(3u);
        duplicate.m_forward = 99.0f;
        EXPECT_FALSE(history.Push(duplicate));
        EXPECT_FALSE(history.Push(MakeCommand(2u)));

        PlayerCommand retained;
        EXPECT_TRUE(history.TryGet(3u, retained));
        EXPECT_FLOAT_EQ(retained.m_forward, 3.0f);
        EXPECT_EQ(history.Size(), 1u);
    }

    TEST(PlayerCommandHistoryTests, FullHistoryEvictsExactlyTheOldestCommand)
    {
        PlayerCommandHistory history;
        for (PlayerCommandSequence sequence = 1u; sequence <= PlayerCommandHistory::Capacity; ++sequence)
        {
            EXPECT_TRUE(history.Push(MakeCommand(sequence)));
        }
        EXPECT_TRUE(history.Push(MakeCommand(PlayerCommandHistory::Capacity + 1u)));
        EXPECT_EQ(history.Size(), PlayerCommandHistory::Capacity);

        PlayerCommand command;
        EXPECT_FALSE(history.TryGet(1u, command));
        EXPECT_TRUE(history.TryGetAt(0u, command));
        EXPECT_EQ(command.m_sequence, 2u);
        EXPECT_TRUE(history.TryGet(PlayerCommandHistory::Capacity + 1u, command));
    }

    TEST(PlayerCommandHistoryTests, AcknowledgementDiscardsOnlyThroughKnownCommand)
    {
        PlayerCommandHistory history;
        for (PlayerCommandSequence sequence = 1u; sequence <= 5u; ++sequence)
        {
            history.Push(MakeCommand(sequence));
        }

        EXPECT_EQ(history.DiscardThrough(3u), 3u);
        EXPECT_EQ(history.Size(), 2u);
        EXPECT_EQ(history.GetLastAcknowledgedSequence(), 3u);
        EXPECT_EQ(history.DiscardThrough(3u), 0u);
        EXPECT_EQ(history.DiscardThrough(2u), 0u);
    }

    TEST(PlayerCommandHistoryTests, FutureAcknowledgementDoesNotPruneLocalHistory)
    {
        PlayerCommandHistory history;
        history.Push(MakeCommand(1u));
        history.Push(MakeCommand(2u));

        EXPECT_EQ(history.DiscardThrough(5u), 0u);
        EXPECT_EQ(history.Size(), 2u);
        EXPECT_EQ(history.DiscardThrough(2u), 2u);
        EXPECT_FALSE(history.HasAcknowledgement() && history.GetLastAcknowledgedSequence() == 5u);
    }

    TEST(PlayerCommandHistoryTests, WraparoundAcknowledgementIsOrderedSafely)
    {
        constexpr PlayerCommandSequence maximum = std::numeric_limits<AZ::u32>::max();
        PlayerCommandHistory history;
        history.Push(MakeCommand(maximum - 1u));
        history.Push(MakeCommand(maximum));
        history.Push(MakeCommand(1u));
        history.Push(MakeCommand(2u));

        EXPECT_EQ(history.DiscardThrough(maximum), 2u);
        EXPECT_EQ(history.Size(), 2u);
        EXPECT_EQ(history.DiscardThrough(1u), 1u);
        EXPECT_EQ(history.Size(), 1u);
        PlayerCommand command;
        EXPECT_TRUE(history.TryGet(2u, command));
    }

    TEST(PlayerCommandHistoryTests, ClearPreservesAcknowledgementAndResetStartsSessionMetadataFresh)
    {
        PlayerCommandHistory history;
        history.Push(MakeCommand(1u));
        history.DiscardThrough(1u);
        history.Clear();
        EXPECT_TRUE(history.Empty());
        EXPECT_TRUE(history.HasAcknowledgement());
        EXPECT_EQ(history.GetLastAcknowledgedSequence(), 1u);

        history.Reset();
        EXPECT_FALSE(history.HasAcknowledgement());
        EXPECT_EQ(history.GetLastAcknowledgedSequence(), InvalidPlayerSimulationSequence);
        EXPECT_TRUE(history.Push(MakeCommand(1u)));
    }
}
