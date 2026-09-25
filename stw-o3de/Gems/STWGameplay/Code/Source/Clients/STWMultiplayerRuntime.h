#pragma once

#include <cstdint>

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

        //! Neither OnPlayerLeave nor the endpoint-disconnected event is
        //! reliably invoked for every disconnect reason in this engine
        //! build (see the comments on both), and a dedicated
        //! AZ::TickBus::Handler on this class was proven - via an
        //! unconditional entry trace, five build/run cycles - to never be
        //! entered either, despite connecting on the same code path as the
        //! (working) spawner registration. Called instead from
        //! STWGameplaySystemComponent::OnTick, a real AZ::Component whose
        //! tick is independently confirmed to run every frame on the
        //! dedicated server (ProbeDedicatedHitValidation), against the
        //! owning STWMultiplayerRuntime's m_multiplayer member directly -
        //! no bus, no interface lookup, no hook that might not fire.
        void ReconcileRosterAgainstConnections();

    private:
        void OnNetworkInitialized(AzNetworking::INetworkInterface* networkInterface);
        void OnEndpointDisconnected(Multiplayer::MultiplayerAgentType agentType);
        void OnServerAcceptanceReceived();
        void PersistMatchRecord();
        uint32_t MatchCapacity() const;

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
        uint32_t m_reconcileDiagnosticTickCounter = 0;
        MatchRoster m_roster;
    };
} // namespace STWGameplay
