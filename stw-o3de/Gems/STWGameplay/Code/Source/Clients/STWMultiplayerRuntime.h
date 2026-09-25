#pragma once

#include <cstdint>

#include <AzCore/Component/TickBus.h>
#include <AzCore/std/string/string.h>
#include <Multiplayer/IMultiplayerSpawner.h>
#include <Multiplayer/IMultiplayer.h>
#include <STWGameplay/MatchSession.h>

namespace AzNetworking
{
    class INetworkInterface;
}

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
        , public AZ::TickBus::Handler
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

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private:
        void OnNetworkInitialized(AzNetworking::INetworkInterface* networkInterface);
        void OnEndpointDisconnected(Multiplayer::MultiplayerAgentType agentType);
        void OnServerAcceptanceReceived();
        void PersistMatchRecord();
        uint32_t MatchCapacity() const;
        //! Neither OnPlayerLeave nor the endpoint-disconnected event is
        //! reliably invoked for every disconnect reason in this engine
        //! build (see the comments on both) - polled here every tick
        //! instead, against the network interface's own connection set,
        //! which does not depend on either of those hooks firing.
        void ReconcileRosterAgainstConnections();

        Multiplayer::IMultiplayer* m_multiplayer = nullptr;
        AzNetworking::INetworkInterface* m_networkInterface = nullptr;
        Multiplayer::NetworkInitEvent::Handler m_networkInitHandler;
        Multiplayer::EndpointDisconnectedEvent::Handler m_endpointDisconnectedHandler;
        Multiplayer::ServerAcceptanceReceivedEvent::Handler m_serverAcceptanceReceivedHandler;
        STWMultiplayerTransportState m_state = STWMultiplayerTransportState::Unavailable;
        bool m_handlersConnected = false;
        bool m_sessionOwned = false;
        bool m_playerSpawnerRegistered = false;
        bool m_dedicatedHardenedLogged = false;
        uint32_t m_serverObservedDisconnectCount = 0;
        MatchRoster m_roster;
    };
} // namespace STWGameplay
