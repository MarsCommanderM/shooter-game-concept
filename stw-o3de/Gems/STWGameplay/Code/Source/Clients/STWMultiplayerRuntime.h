#pragma once

#include <cstdint>

#include <AzCore/std/string/string.h>
#include <Multiplayer/IMultiplayerSpawner.h>
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
    //! It owns the production player-spawn callback but does not own gameplay,
    //! simulation, replication, or presentation state.
    class STWMultiplayerRuntime final
        : public Multiplayer::IMultiplayerSpawner
    {
    public:
        STWMultiplayerRuntime();

        bool Initialize();
        bool StartHosting(uint16_t port, bool isDedicated);
        bool Connect(const AZStd::string& remoteAddress, uint16_t port);
        void Shutdown();

        STWMultiplayerTransportState GetState() const { return m_state; }
        Multiplayer::MultiplayerAgentType GetAgentType() const;

        static const char* GetPlayerSpawnablePath();

        Multiplayer::NetworkEntityHandle OnPlayerJoin(
            uint64_t userId, const Multiplayer::MultiplayerAgentDatum& agentDatum) override;
        void OnPlayerLeave(
            Multiplayer::ConstNetworkEntityHandle entityHandle,
            const Multiplayer::ReplicationSet& replicationSet,
            AzNetworking::DisconnectReason reason) override;

    private:
        void OnNetworkInitialized(AzNetworking::INetworkInterface* networkInterface);
        void OnEndpointDisconnected(Multiplayer::MultiplayerAgentType agentType);
        void OnServerAcceptanceReceived();

        Multiplayer::IMultiplayer* m_multiplayer = nullptr;
        Multiplayer::NetworkInitEvent::Handler m_networkInitHandler;
        Multiplayer::EndpointDisconnectedEvent::Handler m_endpointDisconnectedHandler;
        Multiplayer::ServerAcceptanceReceivedEvent::Handler m_serverAcceptanceReceivedHandler;
        STWMultiplayerTransportState m_state = STWMultiplayerTransportState::Unavailable;
        bool m_handlersConnected = false;
        bool m_sessionOwned = false;
        bool m_playerSpawnerRegistered = false;
    };
} // namespace STWGameplay
