#pragma once

#include <AzCore/base.h>

#include <cmath>

namespace STWGameplay
{
    using PlayerSimulationSequence = AZ::u32;
    using PlayerCommandSequence = PlayerSimulationSequence;
    using PlayerSnapshotSequence = PlayerSimulationSequence;

    inline constexpr PlayerSimulationSequence InvalidPlayerSimulationSequence = 0;
    inline constexpr PlayerSimulationSequence PlayerSimulationSequenceHalfRange = 0x80000000u;

    //! Advances a local simulation serial without ever emitting the reserved zero value.
    inline constexpr PlayerSimulationSequence AdvancePlayerSimulationSequence(
        PlayerSimulationSequence previous)
    {
        return previous == 0xffffffffu ? 1u : previous + 1u;
    }

    //! Serial-number comparison. It is valid only when the compared distance is below half the range.
    inline constexpr bool IsNewerPlayerSimulationSequence(
        PlayerSimulationSequence candidate, PlayerSimulationSequence reference)
    {
        if (candidate == InvalidPlayerSimulationSequence || candidate == reference)
        {
            return false;
        }
        if (reference == InvalidPlayerSimulationSequence)
        {
            return true;
        }
        return static_cast<PlayerSimulationSequence>(candidate - reference)
            < PlayerSimulationSequenceHalfRange;
    }

    struct PlayerInput
    {
        float m_forward = 0.0f;
        float m_strafe = 0.0f;
        float m_lookX = 0.0f;
        float m_lookY = 0.0f;
        bool m_sprint = false;
        bool m_jump = false;
        bool m_crouch = false;
        bool m_mantle = false;
        bool m_fire = false;
        bool m_reload = false;
        bool m_switchWeapon = false;
        // -1 means no direct slot request. The gameplay model validates the request.
        int m_requestedEquipmentSlot = -1;
    };

    //! Sampled player input plus a transport-neutral local command identity.
    //! The command is data only. Gameplay state, movement, PhysX, and presentation remain owned elsewhere.
    struct PlayerCommand final : PlayerInput
    {
        PlayerCommandSequence m_sequence = InvalidPlayerSimulationSequence;

        PlayerCommand() = default;
        PlayerCommand(const PlayerInput& sampledInput, PlayerCommandSequence sequence)
            : PlayerInput(sampledInput)
            , m_sequence(sequence)
        {
        }

        bool IsFinite() const
        {
            return std::isfinite(m_forward) && std::isfinite(m_strafe)
                && std::isfinite(m_lookX) && std::isfinite(m_lookY);
        }
    };

    inline PlayerCommand MakePlayerCommand(const PlayerInput& sampledInput, PlayerCommandSequence sequence)
    {
        return PlayerCommand(sampledInput, sequence);
    }

    struct CommandTransportDecision
    {
        bool m_send = false;
        bool m_dropped = false;
    };

    //! Delay is a count of already queued samples that must exist before the
    //! oldest sample is released. Loss drops exact multiples of lossEvery.
    //! delaySteps 0 and lossEvery 0 release the only queued sample immediately.
    inline CommandTransportDecision DecideCommandTransport(
        AZ::u32 queuedCount,
        AZ::u32 delaySteps,
        PlayerCommandSequence oldestSequence,
        AZ::u32 lossEvery)
    {
        CommandTransportDecision decision;
        const AZ::u32 delay = delaySteps > 8u ? 8u : delaySteps;
        if (queuedCount <= delay)
        {
            return decision;
        }
        if (lossEvery > 0u && oldestSequence != InvalidPlayerSimulationSequence
            && (oldestSequence % lossEvery) == 0u)
        {
            decision.m_dropped = true;
            return decision;
        }
        decision.m_send = true;
        return decision;
    }
}
