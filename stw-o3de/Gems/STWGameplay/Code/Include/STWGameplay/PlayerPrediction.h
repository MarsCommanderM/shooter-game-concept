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

        //! First field in Evaluate order that requires correction. "none" when the
        //! snapshots already agree or the comparison itself is invalid.
        static const char* FirstMismatch(
            const AuthoritativePlayerSnapshot& predicted,
            const AuthoritativePlayerSnapshot& authoritative)
        {
            if (!predicted.m_physicalStateSynchronized || !authoritative.m_physicalStateSynchronized
                || !IsComparisonStateFinite(predicted) || !IsComparisonStateFinite(authoritative))
            {
                return "invalid";
            }
            if ((predicted.m_position - authoritative.m_position).GetLengthSq()
                    > PositionEpsilon * PositionEpsilon)
            {
                return "position";
            }
            if (!IsClose(predicted.m_yaw, authoritative.m_yaw, YawEpsilon))
            {
                return "yaw";
            }
            if (!IsClose(predicted.m_pitch, authoritative.m_pitch, PitchEpsilon))
            {
                return "pitch";
            }
            if (!IsClose(predicted.m_health, authoritative.m_health, HealthEpsilon))
            {
                return "health";
            }
            if (predicted.m_grounded != authoritative.m_grounded)
            {
                return "grounded";
            }
            if (predicted.m_alive != authoritative.m_alive)
            {
                return "alive";
            }
            if (predicted.m_crouchDesired != authoritative.m_crouchDesired)
            {
                return "crouch";
            }
            if (predicted.m_slideActive != authoritative.m_slideActive)
            {
                return "slide";
            }
            if (predicted.m_mantleRequested != authoritative.m_mantleRequested)
            {
                return "mantleRequested";
            }
            if (predicted.m_mantleActive != authoritative.m_mantleActive)
            {
                return "mantle";
            }
            if (predicted.m_activeEquipmentSlot != authoritative.m_activeEquipmentSlot)
            {
                return "equipmentSlot";
            }
            if (predicted.m_activeEquipmentProfile != authoritative.m_activeEquipmentProfile)
            {
                return "equipmentProfile";
            }
            if (predicted.m_magazine != authoritative.m_magazine)
            {
                return "magazine";
            }
            if (predicted.m_reserve != authoritative.m_reserve)
            {
                return "reserve";
            }
            if (predicted.m_charges != authoritative.m_charges)
            {
                return "charges";
            }
            if (!IsClose(predicted.m_cooldownRemaining, authoritative.m_cooldownRemaining, TimerEpsilon))
            {
                return "cooldown";
            }
            if (!IsClose(predicted.m_reloadRemaining, authoritative.m_reloadRemaining, TimerEpsilon))
            {
                return "reload";
            }
            if (predicted.m_reloading != authoritative.m_reloading)
            {
                return "reloading";
            }
            if (predicted.m_deathEvents != authoritative.m_deathEvents)
            {
                return "deathEvents";
            }
            if (predicted.m_respawnEvents != authoritative.m_respawnEvents)
            {
                return "respawnEvents";
            }
            return "none";
        }

        //! Copies the fields Evaluate compares. Sequence identities stay with the caller.
        //! This does not replay commands and does not touch presentation state.
        static void CopyComparedFields(
            AuthoritativePlayerSnapshot& destination,
            const AuthoritativePlayerSnapshot& authoritative)
        {
            destination.m_position = authoritative.m_position;
            destination.m_grounded = authoritative.m_grounded;
            destination.m_yaw = authoritative.m_yaw;
            destination.m_pitch = authoritative.m_pitch;
            destination.m_health = authoritative.m_health;
            destination.m_alive = authoritative.m_alive;
            destination.m_crouchDesired = authoritative.m_crouchDesired;
            destination.m_slideActive = authoritative.m_slideActive;
            destination.m_mantleRequested = authoritative.m_mantleRequested;
            destination.m_mantleActive = authoritative.m_mantleActive;
            destination.m_activeEquipmentSlot = authoritative.m_activeEquipmentSlot;
            destination.m_activeEquipmentProfile = authoritative.m_activeEquipmentProfile;
            destination.m_magazine = authoritative.m_magazine;
            destination.m_reserve = authoritative.m_reserve;
            destination.m_charges = authoritative.m_charges;
            destination.m_cooldownRemaining = authoritative.m_cooldownRemaining;
            destination.m_reloadRemaining = authoritative.m_reloadRemaining;
            destination.m_reloading = authoritative.m_reloading;
            destination.m_deathEvents = authoritative.m_deathEvents;
            destination.m_respawnEvents = authoritative.m_respawnEvents;
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

        //! Remote proxies do not own a local command stream. Their authoritative snapshot
        //! acknowledgement is transport metadata for the owning client and must not be
        //! compared against a nonexistent local command sequence.
        static ReconciliationEvaluation EvaluateRemoteIncoming(
            const AuthoritativePlayerSnapshot& authoritative,
            PlayerSnapshotSequence lastAcceptedSnapshotSequence)
        {
            return EvaluateIncoming(
                authoritative,
                authoritative,
                authoritative.m_acknowledgedCommandSequence,
                lastAcceptedSnapshotSequence);
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
