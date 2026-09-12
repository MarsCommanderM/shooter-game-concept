#pragma once

#include <cstdint>

#include <AzCore/std/string/string.h>
#include <Multiplayer/IMultiplayer.h>

namespace STWGameplay
{
    enum class STWMultiplayerTransportState : uint8_t
    {
        Unavailable,
        Idle,
        Hosting,
        Connecting,
        Connected,
        Failed
    };

    //! Owns only the STW lifecycle boundary to O3DE's real Multiplayer transport.
    //! It does not own gameplay, simulation, replication, or presentation state.
    class STWMultiplayerRuntime final
    {
    public:
        STWMultiplayerRuntime();

        bool Initialize();
        bool StartHosting(uint16_t port, bool isDedicated);
        bool Connect(const AZStd::string& remoteAddress, uint16_t port);
        void Shutdown();

        STWMultiplayerTransportState GetState() const { return m_state; }
        Multiplayer::MultiplayerAgentType GetAgentType() const;

    private:
        void OnEndpointDisconnected(Multiplayer::MultiplayerAgentType agentType);
        void OnServerAcceptanceReceived();

        Multiplayer::IMultiplayer* m_multiplayer = nullptr;
        Multiplayer::EndpointDisconnectedEvent::Handler m_endpointDisconnectedHandler;
        Multiplayer::ServerAcceptanceReceivedEvent::Handler m_serverAcceptanceReceivedHandler;
        STWMultiplayerTransportState m_state = STWMultiplayerTransportState::Unavailable;
        bool m_handlersConnected = false;
        bool m_sessionOwned = false;
    };
} // namespace STWGameplay
