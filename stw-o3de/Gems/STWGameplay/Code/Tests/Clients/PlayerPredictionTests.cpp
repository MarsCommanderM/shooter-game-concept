#include <AzTest/AzTest.h>

#include <limits>

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
}
