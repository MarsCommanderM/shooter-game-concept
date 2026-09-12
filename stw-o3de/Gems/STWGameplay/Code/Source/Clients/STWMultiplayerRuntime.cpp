#include "STWMultiplayerRuntime.h"

#include <AzNetworking/ConnectionLayer/IConnection.h>
#include <Source/AutoGen/AutoComponentTypes.h>

namespace STWGameplay
{
    STWMultiplayerRuntime::STWMultiplayerRuntime()
        : m_endpointDisconnectedHandler([this](Multiplayer::MultiplayerAgentType agentType)
        {
            OnEndpointDisconnected(agentType);
        })
        , m_serverAcceptanceReceivedHandler([this]()
        {
            OnServerAcceptanceReceived();
        })
    {
    }

    bool STWMultiplayerRuntime::Initialize()
    {
        if (m_multiplayer != nullptr)
        {
            return true;
        }

        m_multiplayer = Multiplayer::GetMultiplayer();
        if (m_multiplayer == nullptr)
        {
            m_state = STWMultiplayerTransportState::Unavailable;
            return false;
        }

        RegisterMultiplayerComponents();
        m_multiplayer->AddEndpointDisconnectedHandler(m_endpointDisconnectedHandler);
        m_multiplayer->AddServerAcceptanceReceivedHandler(m_serverAcceptanceReceivedHandler);
        m_handlersConnected = true;
        m_state = STWMultiplayerTransportState::Idle;
        return true;
    }

    bool STWMultiplayerRuntime::StartHosting(uint16_t port, bool isDedicated)
    {
        if (!Initialize())
        {
            return false;
        }

        const bool started = m_multiplayer->StartHosting(port, isDedicated);
        if (started)
        {
            m_sessionOwned = true;
            m_state = STWMultiplayerTransportState::Hosting;
        }
        else if (m_state == STWMultiplayerTransportState::Idle)
        {
            m_state = STWMultiplayerTransportState::Failed;
        }
        return started;
    }

    bool STWMultiplayerRuntime::Connect(const AZStd::string& remoteAddress, uint16_t port)
    {
        if (!Initialize())
        {
            return false;
        }

        const bool connectionCreated = m_multiplayer->Connect(remoteAddress, port);
        if (connectionCreated)
        {
            m_sessionOwned = true;
            m_state = STWMultiplayerTransportState::Connecting;
        }
        else if (m_state == STWMultiplayerTransportState::Idle)
        {
            m_state = STWMultiplayerTransportState::Failed;
        }
        return connectionCreated;
    }

    void STWMultiplayerRuntime::Shutdown()
    {
        if (m_multiplayer != nullptr && m_sessionOwned &&
            m_multiplayer->GetAgentType() != Multiplayer::MultiplayerAgentType::Uninitialized)
        {
            m_multiplayer->Terminate(AzNetworking::DisconnectReason::TerminatedByUser);
        }

        if (m_handlersConnected)
        {
            m_endpointDisconnectedHandler.Disconnect();
            m_serverAcceptanceReceivedHandler.Disconnect();
        }

        m_multiplayer = nullptr;
        m_handlersConnected = false;
        m_sessionOwned = false;
        m_state = STWMultiplayerTransportState::Unavailable;
    }

    Multiplayer::MultiplayerAgentType STWMultiplayerRuntime::GetAgentType() const
    {
        return m_multiplayer != nullptr
            ? m_multiplayer->GetAgentType()
            : Multiplayer::MultiplayerAgentType::Uninitialized;
    }

    void STWMultiplayerRuntime::OnEndpointDisconnected(Multiplayer::MultiplayerAgentType agentType)
    {
        AZ_UNUSED(agentType);
        if (m_state == STWMultiplayerTransportState::Connecting ||
            m_state == STWMultiplayerTransportState::Connected)
        {
            m_state = STWMultiplayerTransportState::Idle;
        }
    }

    void STWMultiplayerRuntime::OnServerAcceptanceReceived()
    {
        if (m_state == STWMultiplayerTransportState::Connecting)
        {
            m_state = STWMultiplayerTransportState::Connected;
        }
    }
} // namespace STWGameplay
