#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>

namespace STWGameplay
{
    enum class MatchTeam : uint8_t
    {
        A,
        B
    };

    enum class DedicatedHostDecision : uint8_t
    {
        Accept,
        RejectPort,
        RejectNotDedicated,
        RejectCapacity,
        RejectAlreadyHosting
    };

    struct DedicatedHostRequest
    {
        uint16_t m_port = 0;
        bool m_isDedicated = false;
        uint32_t m_capacity = 0;
        bool m_alreadyHosting = false;
    };

    inline DedicatedHostDecision ValidateDedicatedHost(const DedicatedHostRequest& request)
    {
        if (request.m_alreadyHosting)
        {
            return DedicatedHostDecision::RejectAlreadyHosting;
        }
        if (!request.m_isDedicated)
        {
            return DedicatedHostDecision::RejectNotDedicated;
        }
        if (request.m_port == 0)
        {
            return DedicatedHostDecision::RejectPort;
        }
        if (request.m_capacity == 0 || request.m_capacity > 16u)
        {
            return DedicatedHostDecision::RejectCapacity;
        }
        return DedicatedHostDecision::Accept;
    }

    inline const char* DedicatedHostDecisionName(DedicatedHostDecision decision)
    {
        switch (decision)
        {
        case DedicatedHostDecision::Accept:
            return "accept";
        case DedicatedHostDecision::RejectPort:
            return "port";
        case DedicatedHostDecision::RejectNotDedicated:
            return "not_dedicated";
        case DedicatedHostDecision::RejectCapacity:
            return "capacity";
        case DedicatedHostDecision::RejectAlreadyHosting:
            return "already_hosting";
        }
        return "not_dedicated";
    }

    //! Fixed roster for one dedicated match. Admission is deterministic:
    //! the first accepted user is slot 0 on team A, the next is slot 1 on
    //! team B, and a repeated user id returns the slot it already owns.
    //!
    //! Team is derived from slot index (even = A, odd = B), so a departed
    //! player's slot is tombstoned in place rather than compacted: shifting
    //! later entries down to fill the gap would silently reassign every
    //! shifted player's team mid-match. TryAdmit reuses the lowest freed
    //! slot (and therefore that slot's team) before growing the roster.
    class MatchRoster
    {
    public:
        static constexpr uint32_t MaxCapacity = 16;

        explicit MatchRoster(uint32_t capacity = MaxCapacity)
            : m_capacity(capacity == 0 || capacity > MaxCapacity ? MaxCapacity : capacity)
        {
        }

        static MatchTeam TeamForSlot(uint32_t slot)
        {
            return (slot % 2u) == 0u ? MatchTeam::A : MatchTeam::B;
        }

        uint32_t Capacity() const { return m_capacity; }
        //! Currently-occupied slot count, not the high-water mark of slots
        //! ever used - a freed slot is not counted until it is reused.
        uint32_t Count() const { return m_liveCount; }
        //! One past the highest slot index ever assigned; freed slots below
        //! this are tombstones, not gaps in a compacted list.
        uint32_t Extent() const { return m_extent; }
        bool IsOccupied(uint32_t slot) const { return slot < m_extent && m_occupied[slot]; }

        bool TryAdmit(uint64_t rosterKey, uint32_t& outSlot, MatchTeam& outTeam)
        {
            for (uint32_t index = 0; index < m_extent; ++index)
            {
                if (m_occupied[index] && m_userIds[index] == rosterKey)
                {
                    outSlot = index;
                    outTeam = TeamForSlot(index);
                    return true;
                }
            }
            for (uint32_t index = 0; index < m_extent; ++index)
            {
                if (!m_occupied[index])
                {
                    m_userIds[index] = rosterKey;
                    m_occupied[index] = true;
                    ++m_liveCount;
                    outSlot = index;
                    outTeam = TeamForSlot(index);
                    return true;
                }
            }
            if (m_extent >= m_capacity)
            {
                return false;
            }
            outSlot = m_extent;
            outTeam = TeamForSlot(m_extent);
            m_userIds[m_extent] = rosterKey;
            m_occupied[m_extent] = true;
            ++m_extent;
            ++m_liveCount;
            return true;
        }

        //! Frees rosterKey's slot without touching any other slot's index,
        //! occupant, or team. Returns false (no change) if rosterKey is not
        //! a currently-occupied member.
        bool TryRemove(uint64_t rosterKey, uint32_t& outSlot)
        {
            for (uint32_t index = 0; index < m_extent; ++index)
            {
                if (m_occupied[index] && m_userIds[index] == rosterKey)
                {
                    m_occupied[index] = false;
                    --m_liveCount;
                    outSlot = index;
                    return true;
                }
            }
            return false;
        }

        uint64_t UserAt(uint32_t slot) const
        {
            return (slot < m_extent && m_occupied[slot]) ? m_userIds[slot] : 0;
        }

        std::string Serialize() const
        {
            std::string text = "version=2\ncapacity=";
            text += std::to_string(m_capacity);
            text += "\nextent=";
            text += std::to_string(m_extent);
            text += "\nplayers=";
            text += std::to_string(m_liveCount);
            text += "\n";
            for (uint32_t index = 0; index < m_extent; ++index)
            {
                text += "slot=";
                text += std::to_string(index);
                text += " user=";
                text += std::to_string(m_occupied[index] ? m_userIds[index] : 0);
                text += " team=";
                text += (TeamForSlot(index) == MatchTeam::A) ? "A" : "B";
                text += " occupied=";
                text += m_occupied[index] ? "1\n" : "0\n";
            }
            return text;
        }

        static bool Deserialize(const std::string& text, MatchRoster& outRoster)
        {
            uint32_t capacity = 0;
            uint32_t extent = 0;
            uint32_t players = 0;
            if (!ReadU32(text, "capacity=", capacity) || !ReadU32(text, "extent=", extent) ||
                !ReadU32(text, "players=", players))
            {
                return false;
            }
            if (capacity == 0 || capacity > MaxCapacity || extent > capacity || players > extent)
            {
                return false;
            }
            MatchRoster loaded(capacity);
            size_t search = 0;
            uint32_t liveSeen = 0;
            for (uint32_t index = 0; index < extent; ++index)
            {
                const size_t slotPos = text.find("slot=", search);
                if (slotPos == std::string::npos)
                {
                    return false;
                }
                size_t valuePos = slotPos + 5;
                char* end = nullptr;
                const unsigned long slotValue = std::strtoul(text.c_str() + valuePos, &end, 10);
                if (end == text.c_str() + valuePos || slotValue != index)
                {
                    return false;
                }

                const size_t userPos = text.find("user=", static_cast<size_t>(end - text.c_str()));
                if (userPos == std::string::npos)
                {
                    return false;
                }
                valuePos = userPos + 5;
                const unsigned long long userId = std::strtoull(text.c_str() + valuePos, &end, 10);
                if (end == text.c_str() + valuePos)
                {
                    return false;
                }

                const size_t occupiedPos = text.find("occupied=", static_cast<size_t>(end - text.c_str()));
                if (occupiedPos == std::string::npos)
                {
                    return false;
                }
                valuePos = occupiedPos + 9;
                const unsigned long occupiedValue = std::strtoul(text.c_str() + valuePos, &end, 10);
                if (end == text.c_str() + valuePos)
                {
                    return false;
                }

                loaded.m_userIds[index] = static_cast<uint64_t>(userId);
                loaded.m_occupied[index] = occupiedValue != 0;
                if (loaded.m_occupied[index])
                {
                    ++liveSeen;
                }
                search = static_cast<size_t>(end - text.c_str());
            }
            if (liveSeen != players)
            {
                return false;
            }
            loaded.m_extent = extent;
            loaded.m_liveCount = liveSeen;
            outRoster = loaded;
            return true;
        }

    private:
        static bool ReadU32(const std::string& text, const char* key, uint32_t& outValue)
        {
            const size_t pos = text.find(key);
            if (pos == std::string::npos)
            {
                return false;
            }
            char* end = nullptr;
            const unsigned long value = std::strtoul(text.c_str() + pos + std::char_traits<char>::length(key), &end, 10);
            if (end == text.c_str() + pos)
            {
                return false;
            }
            outValue = static_cast<uint32_t>(value);
            return true;
        }

        uint32_t m_capacity = MaxCapacity;
        uint32_t m_extent = 0;
        uint32_t m_liveCount = 0;
        uint64_t m_userIds[MaxCapacity] = {};
        bool m_occupied[MaxCapacity] = {};
    };
}
