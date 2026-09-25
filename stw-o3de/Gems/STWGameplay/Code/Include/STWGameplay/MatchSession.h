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
    class MatchRoster
    {
    public:
        static constexpr uint32_t MaxCapacity = 16;

        explicit MatchRoster(uint32_t capacity = MaxCapacity)
            : m_capacity(capacity == 0 || capacity > MaxCapacity ? MaxCapacity : capacity)
        {
        }

        uint32_t Capacity() const { return m_capacity; }
        uint32_t Count() const { return m_count; }

        bool TryAdmit(uint64_t rosterKey, uint32_t& outSlot, MatchTeam& outTeam)
        {
            for (uint32_t index = 0; index < m_count; ++index)
            {
                if (m_userIds[index] == rosterKey)
                {
                    outSlot = index;
                    outTeam = (index % 2u) == 0u ? MatchTeam::A : MatchTeam::B;
                    return true;
                }
            }
            if (m_count >= m_capacity)
            {
                return false;
            }
            outSlot = m_count;
            outTeam = (m_count % 2u) == 0u ? MatchTeam::A : MatchTeam::B;
            m_userIds[m_count++] = rosterKey;
            return true;
        }

        uint64_t UserAt(uint32_t slot) const
        {
            return slot < m_count ? m_userIds[slot] : 0;
        }

        std::string Serialize() const
        {
            std::string text = "version=1\ncapacity=";
            text += std::to_string(m_capacity);
            text += "\nplayers=";
            text += std::to_string(m_count);
            text += "\n";
            for (uint32_t index = 0; index < m_count; ++index)
            {
                text += "user=";
                text += std::to_string(m_userIds[index]);
                text += " slot=";
                text += std::to_string(index);
                text += " team=";
                text += ((index % 2u) == 0u) ? "A\n" : "B\n";
            }
            return text;
        }

        static bool Deserialize(const std::string& text, MatchRoster& outRoster)
        {
            uint32_t capacity = 0;
            uint32_t players = 0;
            if (!ReadU32(text, "capacity=", capacity) || !ReadU32(text, "players=", players))
            {
                return false;
            }
            if (capacity == 0 || capacity > MaxCapacity || players > capacity)
            {
                return false;
            }
            MatchRoster loaded(capacity);
            size_t search = 0;
            for (uint32_t index = 0; index < players; ++index)
            {
                const size_t userPos = text.find("user=", search);
                if (userPos == std::string::npos)
                {
                    return false;
                }
                const size_t valuePos = userPos + 5;
                char* end = nullptr;
                const unsigned long long userId = std::strtoull(text.c_str() + valuePos, &end, 10);
                if (end == text.c_str() + valuePos)
                {
                    return false;
                }
                uint32_t slot = 0;
                MatchTeam team = MatchTeam::A;
                if (!loaded.TryAdmit(static_cast<uint64_t>(userId), slot, team) || slot != index)
                {
                    return false;
                }
                search = static_cast<size_t>(end - text.c_str());
            }
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
        uint32_t m_count = 0;
        uint64_t m_userIds[MaxCapacity] = {};
    };
}
