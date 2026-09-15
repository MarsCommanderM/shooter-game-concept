#pragma once

#include <cstddef>
#include <cmath>

#include <STWGameplay/PlayerSimulationTypes.h>

namespace STWGameplay
{
    enum class ReconciliationDecision
    {
        NoCorrection,
        CorrectionRequired,
        CorrectionAndReplayRequired,
        InvalidAuthoritativeState
    };

    struct ReconciliationResult
    {
        ReconciliationDecision m_decision = ReconciliationDecision::NoCorrection;
        bool m_comparisonValid = true;
    };

    enum class ReconciliationSnapshotStatus
    {
        Accepted,
        IgnoredStale,
        Invalid
    };

    struct ReconciliationEvaluation final
    {
        ReconciliationResult m_comparison;
        ReconciliationSnapshotStatus m_snapshotStatus = ReconciliationSnapshotStatus::Invalid;
        bool m_acknowledgementUsable = false;
        size_t m_discardedCommandCount = 0;
    };

    //! Pure comparison policy. It never writes gameplay, physics, or presentation state.
    class PlayerReconciliationPolicy final
    {
    public:
        static constexpr float PositionEpsilon = 0.05f;
        static constexpr float YawEpsilon = 0.001f;
        static constexpr float PitchEpsilon = 0.001f;
        static constexpr float HealthEpsilon = 0.001f;
        static constexpr float TimerEpsilon = 0.001f;

        static ReconciliationResult Evaluate(
            const AuthoritativePlayerSnapshot& predicted,
            const AuthoritativePlayerSnapshot& authoritative)
        {
            if (!predicted.m_physicalStateSynchronized || !authoritative.m_physicalStateSynchronized
                || !IsComparisonStateFinite(predicted) || !IsComparisonStateFinite(authoritative))
            {
                return { ReconciliationDecision::InvalidAuthoritativeState, false };
            }

            if ((predicted.m_position - authoritative.m_position).GetLengthSq()
                    > PositionEpsilon * PositionEpsilon
                || !IsClose(predicted.m_yaw, authoritative.m_yaw, YawEpsilon)
                || !IsClose(predicted.m_pitch, authoritative.m_pitch, PitchEpsilon)
                || !IsClose(predicted.m_health, authoritative.m_health, HealthEpsilon)
                || predicted.m_grounded != authoritative.m_grounded
                || predicted.m_alive != authoritative.m_alive
                || predicted.m_crouchDesired != authoritative.m_crouchDesired
                || predicted.m_slideActive != authoritative.m_slideActive
                || predicted.m_mantleRequested != authoritative.m_mantleRequested
                || predicted.m_mantleActive != authoritative.m_mantleActive
                || predicted.m_activeEquipmentSlot != authoritative.m_activeEquipmentSlot
                || predicted.m_activeEquipmentProfile != authoritative.m_activeEquipmentProfile
                || predicted.m_magazine != authoritative.m_magazine
                || predicted.m_reserve != authoritative.m_reserve
                || predicted.m_charges != authoritative.m_charges
                || !IsClose(predicted.m_cooldownRemaining, authoritative.m_cooldownRemaining, TimerEpsilon)
                || !IsClose(predicted.m_reloadRemaining, authoritative.m_reloadRemaining, TimerEpsilon)
                || predicted.m_reloading != authoritative.m_reloading
                || predicted.m_deathEvents != authoritative.m_deathEvents
                || predicted.m_respawnEvents != authoritative.m_respawnEvents)
            {
                return { ReconciliationDecision::CorrectionRequired, true };
            }

            return {};
        }

        //! Classifies one externally supplied snapshot without applying correction or replay.
        //! A snapshot sequence is accepted only once, and an acknowledgement may not advance
        //! beyond the locally generated command sequence. Physical timing remains whatever the
        //! snapshot's explicit readback metadata proves; this function does not strengthen it.
        static ReconciliationEvaluation EvaluateIncoming(
            const AuthoritativePlayerSnapshot& predicted,
            const AuthoritativePlayerSnapshot& authoritative,
            PlayerCommandSequence latestLocalCommandSequence,
            PlayerSnapshotSequence lastAcceptedSnapshotSequence)
        {
            ReconciliationEvaluation evaluation;
            evaluation.m_comparison = { ReconciliationDecision::InvalidAuthoritativeState, false };
            if (authoritative.m_snapshotSequence == InvalidPlayerSimulationSequence)
            {
                return evaluation;
            }

            if (lastAcceptedSnapshotSequence != InvalidPlayerSimulationSequence
                && !IsNewerPlayerSimulationSequence(
                    authoritative.m_snapshotSequence, lastAcceptedSnapshotSequence))
            {
                evaluation.m_snapshotStatus = ReconciliationSnapshotStatus::IgnoredStale;
                evaluation.m_comparison = { ReconciliationDecision::NoCorrection, true };
                return evaluation;
            }

            if (authoritative.m_acknowledgedCommandSequence != InvalidPlayerSimulationSequence
                && (latestLocalCommandSequence == InvalidPlayerSimulationSequence
                    || IsNewerPlayerSimulationSequence(
                        authoritative.m_acknowledgedCommandSequence, latestLocalCommandSequence)))
            {
                return evaluation;
            }

            evaluation.m_comparison = Evaluate(predicted, authoritative);
            if (!evaluation.m_comparison.m_comparisonValid)
            {
                return evaluation;
            }

            evaluation.m_snapshotStatus = ReconciliationSnapshotStatus::Accepted;
            evaluation.m_acknowledgementUsable =
                authoritative.m_acknowledgedCommandSequence != InvalidPlayerSimulationSequence;
            return evaluation;
        }

    private:
        static bool IsClose(float first, float second, float epsilon)
        {
            return std::abs(first - second) <= epsilon;
        }

        static bool IsComparisonStateFinite(const AuthoritativePlayerSnapshot& snapshot)
        {
            return snapshot.m_position.IsFinite()
                && std::isfinite(snapshot.m_yaw)
                && std::isfinite(snapshot.m_pitch)
                && std::isfinite(snapshot.m_health)
                && std::isfinite(snapshot.m_cooldownRemaining)
                && std::isfinite(snapshot.m_reloadRemaining);
        }
    };
}
