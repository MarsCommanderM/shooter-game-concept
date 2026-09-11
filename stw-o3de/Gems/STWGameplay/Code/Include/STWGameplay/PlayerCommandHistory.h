#pragma once

#include <cstddef>

#include <AzCore/std/containers/array.h>

#include <STWGameplay/PlayerCommand.h>

namespace STWGameplay
{
    //! Fixed-capacity local command history. It stores command data only and never mutates gameplay state.
    class PlayerCommandHistory final
    {
    public:
        static constexpr size_t Capacity = 64;

        bool Push(const PlayerCommand& command)
        {
            if (command.m_sequence == InvalidPlayerSimulationSequence || !command.IsFinite())
            {
                return false;
            }
            if (m_hasAcknowledgement && !IsNewerPlayerSimulationSequence(
                    command.m_sequence, m_lastAcknowledgedSequence))
            {
                return false;
            }
            if (m_size > 0 && !IsNewerPlayerSimulationSequence(command.m_sequence, GetNewest().m_sequence))
            {
                return false;
            }

            const size_t insertIndex = (m_begin + m_size) % Capacity;
            m_commands[insertIndex] = command;
            if (m_size < Capacity)
            {
                ++m_size;
            }
            else
            {
                m_begin = (m_begin + 1) % Capacity;
            }
            return true;
        }

        bool TryGet(PlayerCommandSequence sequence, PlayerCommand& command) const
        {
            for (size_t offset = 0; offset < m_size; ++offset)
            {
                const PlayerCommand& candidate = GetAtUnchecked(offset);
                if (candidate.m_sequence == sequence)
                {
                    command = candidate;
                    return true;
                }
            }
            return false;
        }

        bool TryGetAt(size_t offset, PlayerCommand& command) const
        {
            if (offset >= m_size)
            {
                return false;
            }
            command = GetAtUnchecked(offset);
            return true;
        }

        size_t DiscardThrough(PlayerCommandSequence acknowledgement)
        {
            if (acknowledgement == InvalidPlayerSimulationSequence)
            {
                return 0;
            }

            // A repeated or older acknowledgement cannot remove anything a newer acknowledgement
            // has already established.
            if (m_hasAcknowledgement && !IsNewerPlayerSimulationSequence(
                    acknowledgement, m_lastAcknowledgedSequence))
            {
                return 0;
            }

            // A future/unseen acknowledgement is not proof that any locally retained command was
            // processed. Do not prune or advance the acknowledgement watermark in that case.
            if (m_size > 0 && IsNewerPlayerSimulationSequence(acknowledgement, GetNewest().m_sequence))
            {
                return 0;
            }

            m_hasAcknowledgement = true;
            m_lastAcknowledgedSequence = acknowledgement;
            size_t discarded = 0;
            while (m_size > 0)
            {
                const PlayerCommandSequence oldest = GetAtUnchecked(0).m_sequence;
                if (oldest != acknowledgement
                    && !IsNewerPlayerSimulationSequence(acknowledgement, oldest))
                {
                    break;
                }
                m_begin = (m_begin + 1) % Capacity;
                --m_size;
                ++discarded;
            }
            if (m_size == 0)
            {
                m_begin = 0;
            }
            return discarded;
        }

        //! Clears retained commands while preserving the latest acknowledgement watermark.
        void Clear()
        {
            m_begin = 0;
            m_size = 0;
        }

        //! Resets entries and acknowledgement metadata for a new simulation session.
        void Reset()
        {
            Clear();
            m_hasAcknowledgement = false;
            m_lastAcknowledgedSequence = InvalidPlayerSimulationSequence;
        }

        bool Empty() const { return m_size == 0; }
        size_t Size() const { return m_size; }
        bool HasAcknowledgement() const { return m_hasAcknowledgement; }
        PlayerCommandSequence GetLastAcknowledgedSequence() const { return m_lastAcknowledgedSequence; }

    private:
        const PlayerCommand& GetAtUnchecked(size_t offset) const
        {
            return m_commands[(m_begin + offset) % Capacity];
        }

        const PlayerCommand& GetNewest() const
        {
            return GetAtUnchecked(m_size - 1);
        }

        AZStd::array<PlayerCommand, Capacity> m_commands{};
        size_t m_begin = 0;
        size_t m_size = 0;
        bool m_hasAcknowledgement = false;
        PlayerCommandSequence m_lastAcknowledgedSequence = InvalidPlayerSimulationSequence;
    };
}
