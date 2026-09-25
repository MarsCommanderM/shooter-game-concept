#include <AzTest/AzTest.h>

#include <limits>

#include <STWGameplay/MatchSession.h>
#include <STWGameplay/PlayerCommand.h>
#include <STWGameplay/PlayerPrediction.h>

namespace STWGameplay
{
    namespace
    {
        AuthoritativePlayerSnapshot MakeSnapshot()
        {
            AuthoritativePlayerSnapshot snapshot;
            snapshot.m_physicalStateSynchronized = true;
            snapshot.m_position = AZ::Vector3(1.0f, 2.0f, 3.0f);
            snapshot.m_grounded = true;
            snapshot.m_yaw = 0.25f;
            snapshot.m_pitch = -0.10f;
            snapshot.m_health = 80.0f;
            snapshot.m_alive = true;
            snapshot.m_crouchDesired = false;
            snapshot.m_slideActive = false;
            snapshot.m_mantleRequested = false;
            snapshot.m_mantleActive = false;
            snapshot.m_magazine = 12;
            snapshot.m_reserve = 48;
            snapshot.m_charges = 2;
            snapshot.m_cooldownRemaining = 0.02f;
            snapshot.m_reloadRemaining = 0.0f;
            snapshot.m_reloading = false;
            snapshot.m_deathEvents = 1;
            snapshot.m_respawnEvents = 0;
            return snapshot;
        }
    }

    TEST(PlayerPredictionTests, IdenticalSnapshotsRequireNoCorrection)
    {
        const AuthoritativePlayerSnapshot snapshot = MakeSnapshot();
        const ReconciliationResult result = PlayerReconciliationPolicy::Evaluate(snapshot, snapshot);
        EXPECT_EQ(result.m_decision, ReconciliationDecision::NoCorrection);
        EXPECT_TRUE(result.m_comparisonValid);
    }

    TEST(PlayerPredictionTests, PositionBelowEpsilonRequiresNoCorrection)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_position.SetX(predicted.m_position.GetX() + 0.04f);
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::NoCorrection);
    }

    TEST(PlayerPredictionTests, PositionAboveEpsilonRequiresCorrectionWithoutReplay)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_position.SetX(predicted.m_position.GetX() + 0.10f);
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::CorrectionRequired);
    }

    TEST(PlayerPredictionTests, GameplayStateMismatchesRequireCorrection)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_grounded = false;
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::CorrectionRequired);

        authoritative = predicted;
        authoritative.m_alive = false;
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::CorrectionRequired);

        authoritative = predicted;
        authoritative.m_activeEquipmentSlot = EquipmentSlot::Secondary;
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::CorrectionRequired);
    }

    TEST(PlayerPredictionTests, WeaponAndTraversalStateMismatchesRequireCorrection)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_magazine -= 1;
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::CorrectionRequired);

        authoritative = predicted;
        authoritative.m_slideActive = true;
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::CorrectionRequired);
    }

    TEST(PlayerPredictionTests, NonFiniteComparisonStateIsRejectedExplicitly)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_position.SetX(std::numeric_limits<float>::quiet_NaN());
        ReconciliationResult result = PlayerReconciliationPolicy::Evaluate(predicted, authoritative);
        EXPECT_EQ(result.m_decision, ReconciliationDecision::InvalidAuthoritativeState);
        EXPECT_FALSE(result.m_comparisonValid);

        authoritative = predicted;
        authoritative.m_yaw = std::numeric_limits<float>::infinity();
        result = PlayerReconciliationPolicy::Evaluate(predicted, authoritative);
        EXPECT_EQ(result.m_decision, ReconciliationDecision::InvalidAuthoritativeState);
        EXPECT_FALSE(result.m_comparisonValid);

        authoritative = predicted;
        authoritative.m_reloadRemaining = -std::numeric_limits<float>::infinity();
        result = PlayerReconciliationPolicy::Evaluate(predicted, authoritative);
        EXPECT_EQ(result.m_decision, ReconciliationDecision::InvalidAuthoritativeState);
        EXPECT_FALSE(result.m_comparisonValid);
    }

    TEST(PlayerPredictionTests, MetadataAndEventIdentityAreNotCorrectionState)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_snapshotSequence = 42u;
        authoritative.m_acknowledgedCommandSequence = 41u;
        authoritative.m_physicalReadbackSequence = 40u;
        authoritative.m_requestedSimulationVelocity = AZ::Vector3(99.0f, 0.0f, 0.0f);
        authoritative.m_lastAcceptedUseEventId = 100u;

        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(predicted, authoritative).m_decision,
            ReconciliationDecision::NoCorrection);
    }

    TEST(PlayerPredictionTests, UnsynchronizedPhysicalStateIsRejectedExplicitly)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = MakeSnapshot();
        authoritative.m_physicalStateSynchronized = false;
        const ReconciliationResult result = PlayerReconciliationPolicy::Evaluate(predicted, authoritative);
        EXPECT_EQ(result.m_decision, ReconciliationDecision::InvalidAuthoritativeState);
        EXPECT_FALSE(result.m_comparisonValid);
    }

    TEST(PlayerPredictionTests, EvaluationDoesNotMutateEitherSnapshot)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        const AuthoritativePlayerSnapshot authoritative = MakeSnapshot();
        const ReconciliationResult result = PlayerReconciliationPolicy::Evaluate(predicted, authoritative);
        EXPECT_EQ(result.m_decision, ReconciliationDecision::NoCorrection);
        EXPECT_FLOAT_EQ(predicted.m_position.GetX(), 1.0f);
        EXPECT_FLOAT_EQ(authoritative.m_position.GetX(), 1.0f);
        EXPECT_EQ(predicted.m_magazine, 12);
        EXPECT_EQ(authoritative.m_magazine, 12);
    }

    TEST(PlayerPredictionTests, IncomingSnapshotAcceptsNewSequenceAndAcknowledgement)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_snapshotSequence = 10u;
        authoritative.m_acknowledgedCommandSequence = 7u;

        const ReconciliationEvaluation evaluation = PlayerReconciliationPolicy::EvaluateIncoming(
            predicted, authoritative, 8u, InvalidPlayerSimulationSequence);
        EXPECT_EQ(evaluation.m_snapshotStatus, ReconciliationSnapshotStatus::Accepted);
        EXPECT_TRUE(evaluation.m_acknowledgementUsable);
        EXPECT_EQ(evaluation.m_comparison.m_decision, ReconciliationDecision::NoCorrection);
    }

    TEST(PlayerPredictionTests, IncomingSnapshotIgnoresEqualAndOlderSequences)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_snapshotSequence = 10u;
        authoritative.m_acknowledgedCommandSequence = 7u;

        ReconciliationEvaluation evaluation = PlayerReconciliationPolicy::EvaluateIncoming(
            predicted, authoritative, 8u, 10u);
        EXPECT_EQ(evaluation.m_snapshotStatus, ReconciliationSnapshotStatus::IgnoredStale);
        EXPECT_FALSE(evaluation.m_acknowledgementUsable);

        authoritative.m_snapshotSequence = 9u;
        evaluation = PlayerReconciliationPolicy::EvaluateIncoming(predicted, authoritative, 8u, 10u);
        EXPECT_EQ(evaluation.m_snapshotStatus, ReconciliationSnapshotStatus::IgnoredStale);
        EXPECT_FALSE(evaluation.m_acknowledgementUsable);
    }

    TEST(PlayerPredictionTests, IncomingSnapshotRejectsFutureAcknowledgement)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_snapshotSequence = 1u;
        authoritative.m_acknowledgedCommandSequence = 3u;

        const ReconciliationEvaluation evaluation = PlayerReconciliationPolicy::EvaluateIncoming(
            predicted, authoritative, 2u, InvalidPlayerSimulationSequence);
        EXPECT_EQ(evaluation.m_snapshotStatus, ReconciliationSnapshotStatus::Invalid);
        EXPECT_EQ(evaluation.m_comparison.m_decision, ReconciliationDecision::InvalidAuthoritativeState);
        EXPECT_FALSE(evaluation.m_comparison.m_comparisonValid);
    }

    TEST(PlayerPredictionTests, IncomingSnapshotRejectsUnsequencedSnapshot)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;

        const ReconciliationEvaluation evaluation = PlayerReconciliationPolicy::EvaluateIncoming(
            predicted, authoritative, 1u, InvalidPlayerSimulationSequence);
        EXPECT_EQ(evaluation.m_snapshotStatus, ReconciliationSnapshotStatus::Invalid);
        EXPECT_FALSE(evaluation.m_acknowledgementUsable);
    }

    TEST(PlayerPredictionTests, IncomingSnapshotSequenceAcceptsValidWraparound)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_snapshotSequence = 1u;

        const ReconciliationEvaluation evaluation = PlayerReconciliationPolicy::EvaluateIncoming(
            predicted, authoritative, 2u, std::numeric_limits<AZ::u32>::max());
        EXPECT_EQ(evaluation.m_snapshotStatus, ReconciliationSnapshotStatus::Accepted);
    }

    TEST(PlayerPredictionTests, CopyComparedFieldsWritesGameplayAndKeepsSequence)
    {
        AuthoritativePlayerSnapshot destination = MakeSnapshot();
        destination.m_snapshotSequence = 4u;
        destination.m_acknowledgedCommandSequence = 9u;
        AuthoritativePlayerSnapshot authoritative = MakeSnapshot();
        authoritative.m_snapshotSequence = 80u;
        authoritative.m_acknowledgedCommandSequence = 3u;
        authoritative.m_position.SetX(destination.m_position.GetX() + 0.10f);
        authoritative.m_magazine = destination.m_magazine - 1;

        PlayerReconciliationPolicy::CopyComparedFields(destination, authoritative);

        EXPECT_EQ(destination.m_snapshotSequence, 4u);
        EXPECT_EQ(destination.m_acknowledgedCommandSequence, 9u);
        EXPECT_FLOAT_EQ(destination.m_position.GetX(), authoritative.m_position.GetX());
        EXPECT_EQ(destination.m_magazine, authoritative.m_magazine);
        EXPECT_EQ(
            PlayerReconciliationPolicy::Evaluate(destination, authoritative).m_decision,
            ReconciliationDecision::NoCorrection);
    }

    TEST(PlayerPredictionTests, FirstMismatchNamesPositionBeforeLaterFields)
    {
        const AuthoritativePlayerSnapshot predicted = MakeSnapshot();
        AuthoritativePlayerSnapshot authoritative = predicted;
        authoritative.m_position.SetX(predicted.m_position.GetX() + 0.10f);
        authoritative.m_grounded = !predicted.m_grounded;
        EXPECT_STREQ(PlayerReconciliationPolicy::FirstMismatch(predicted, authoritative), "position");

        authoritative = predicted;
        authoritative.m_grounded = !predicted.m_grounded;
        EXPECT_STREQ(PlayerReconciliationPolicy::FirstMismatch(predicted, authoritative), "grounded");
        EXPECT_STREQ(PlayerReconciliationPolicy::FirstMismatch(predicted, predicted), "none");
    }

    TEST(PlayerPredictionTests, CommandTransportHoldsDelaysAndDropsExactMultiples)
    {
        const CommandTransportDecision immediate = DecideCommandTransport(1u, 0u, 5u, 0u);
        EXPECT_TRUE(immediate.m_send);
        EXPECT_FALSE(immediate.m_dropped);

        const CommandTransportDecision held = DecideCommandTransport(2u, 2u, 5u, 0u);
        EXPECT_FALSE(held.m_send);
        EXPECT_FALSE(held.m_dropped);

        const CommandTransportDecision released = DecideCommandTransport(3u, 2u, 5u, 0u);
        EXPECT_TRUE(released.m_send);
        EXPECT_FALSE(released.m_dropped);

        const CommandTransportDecision dropped = DecideCommandTransport(1u, 0u, 4u, 4u);
        EXPECT_FALSE(dropped.m_send);
        EXPECT_TRUE(dropped.m_dropped);

        const CommandTransportDecision kept = DecideCommandTransport(1u, 0u, 5u, 4u);
        EXPECT_TRUE(kept.m_send);
        EXPECT_FALSE(kept.m_dropped);
    }

    TEST(PlayerPredictionTests, MatchRosterAssignsTeamsAndRejectsAFullServer)
    {
        MatchRoster roster(2);
        uint32_t slot = 99;
        MatchTeam team = MatchTeam::B;
        ASSERT_TRUE(roster.TryAdmit(0, slot, team));
        EXPECT_EQ(slot, 0u);
        EXPECT_EQ(team, MatchTeam::A);
        ASSERT_TRUE(roster.TryAdmit(7, slot, team));
        EXPECT_EQ(slot, 1u);
        EXPECT_EQ(team, MatchTeam::B);
        EXPECT_FALSE(roster.TryAdmit(8, slot, team));
        ASSERT_TRUE(roster.TryAdmit(0, slot, team));
        EXPECT_EQ(slot, 0u);

        MatchRoster loaded;
        ASSERT_TRUE(MatchRoster::Deserialize(roster.Serialize(), loaded));
        EXPECT_EQ(loaded.Count(), 2u);
        EXPECT_EQ(loaded.UserAt(0), 0u);
        EXPECT_EQ(loaded.UserAt(1), 7u);
        EXPECT_FALSE(MatchRoster::Deserialize("version=1\n", loaded));
    }

    TEST(PlayerPredictionTests, MatchRosterFreesSlotWithoutReassigningOtherTeams)
    {
        MatchRoster roster(4);
        uint32_t slot = 99;
        MatchTeam team = MatchTeam::B;
        ASSERT_TRUE(roster.TryAdmit(1, slot, team));
        EXPECT_EQ(slot, 0u);
        EXPECT_EQ(team, MatchTeam::A);
        ASSERT_TRUE(roster.TryAdmit(2, slot, team));
        EXPECT_EQ(slot, 1u);
        EXPECT_EQ(team, MatchTeam::B);
        ASSERT_TRUE(roster.TryAdmit(3, slot, team));
        EXPECT_EQ(slot, 2u);
        EXPECT_EQ(team, MatchTeam::A);
        EXPECT_EQ(roster.Count(), 3u);

        uint32_t removedSlot = 99;
        EXPECT_FALSE(roster.TryRemove(999, removedSlot));
        ASSERT_TRUE(roster.TryRemove(2, removedSlot));
        EXPECT_EQ(removedSlot, 1u);
        EXPECT_EQ(roster.Count(), 2u);
        EXPECT_EQ(roster.Extent(), 3u);
        EXPECT_FALSE(roster.IsOccupied(1));

        // Neither remaining player's slot or team moved.
        EXPECT_TRUE(roster.IsOccupied(0));
        EXPECT_EQ(roster.UserAt(0), 1u);
        EXPECT_TRUE(roster.IsOccupied(2));
        EXPECT_EQ(roster.UserAt(2), 3u);

        // A fresh admit reuses the freed slot - and its team - instead of
        // growing the roster past its previous extent.
        ASSERT_TRUE(roster.TryAdmit(4, slot, team));
        EXPECT_EQ(slot, 1u);
        EXPECT_EQ(team, MatchTeam::B);
        EXPECT_EQ(roster.Count(), 3u);
        EXPECT_EQ(roster.Extent(), 3u);

        MatchRoster loaded;
        ASSERT_TRUE(MatchRoster::Deserialize(roster.Serialize(), loaded));
        EXPECT_EQ(loaded.Count(), 3u);
        EXPECT_EQ(loaded.Extent(), 3u);
        EXPECT_EQ(loaded.UserAt(0), 1u);
        EXPECT_EQ(loaded.UserAt(1), 4u);
        EXPECT_EQ(loaded.UserAt(2), 3u);
    }

    TEST(PlayerPredictionTests, DedicatedHostRejectsPortZeroAndASecondHost)
    {
        DedicatedHostRequest request;
        request.m_port = 0;
        request.m_isDedicated = true;
        request.m_capacity = 16;
        EXPECT_EQ(ValidateDedicatedHost(request), DedicatedHostDecision::RejectPort);

        request.m_port = 33450;
        request.m_isDedicated = false;
        EXPECT_EQ(ValidateDedicatedHost(request), DedicatedHostDecision::RejectNotDedicated);

        request.m_isDedicated = true;
        request.m_capacity = 0;
        EXPECT_EQ(ValidateDedicatedHost(request), DedicatedHostDecision::RejectCapacity);

        request.m_capacity = 16;
        request.m_alreadyHosting = true;
        EXPECT_EQ(ValidateDedicatedHost(request), DedicatedHostDecision::RejectAlreadyHosting);

        request.m_alreadyHosting = false;
        EXPECT_EQ(ValidateDedicatedHost(request), DedicatedHostDecision::Accept);
    }
}
