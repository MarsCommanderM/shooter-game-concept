#pragma once

#include <Source/AutoGen/STWPlayerNetworkComponent.AutoComponent.h>

#include <STWGameplay/PlayerCommand.h>
#include <STWGameplay/PlayerSimulationTypes.h>

namespace STWGameplay
{
    enum class STWNetworkRoleBinding : AZ::u8
    {
        GameplayAuthority,
        RemoteSnapshot
    };

    inline STWNetworkRoleBinding GetSTWNetworkRoleBinding(Multiplayer::NetEntityRole role)
    {
        return role == Multiplayer::NetEntityRole::Client
            ? STWNetworkRoleBinding::RemoteSnapshot
            : STWNetworkRoleBinding::GameplayAuthority;
    }

    //! O3DE transport carrier for one already-sampled PlayerCommand.
    //! This component owns no gameplay state and does not execute simulation.
    class STWPlayerNetworkComponent final
        : public STWPlayerNetworkComponentBase
    {
    public:
        AZ_MULTIPLAYER_COMPONENT(
            STWGameplay::STWPlayerNetworkComponent,
            s_sTWPlayerNetworkComponentConcreteUuid,
            STWGameplay::STWPlayerNetworkComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        static void WriteCommand(
            STWPlayerNetworkComponentNetworkInput& networkInput,
            const PlayerCommand& command);

        static bool ReadCommand(
            const STWPlayerNetworkComponentNetworkInput& networkInput,
            PlayerCommand& command);

        STWPlayerNetworkComponent();

        //! Writes the complete current gameplay snapshot to Authority->Client properties.
        //! Only an authoritative controller may write these properties.
        void PublishAuthoritativeSnapshot(const AuthoritativePlayerSnapshot& snapshot);

        void OnInit() override;
        void OnActivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;
        void OnDeactivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;
        void OnNetworkActivated() override;

    private:
        void OnAuthoritativeSnapshotSequenceChanged(uint32_t snapshotSequence);
        bool ReadAuthoritativeSnapshot(AuthoritativePlayerSnapshot& snapshot) const;

        AZ::Event<uint32_t>::Handler m_snapshotSequenceChangedHandler;
        bool m_remoteSnapshotBound = false;
        bool m_networkRoleReported = false;
        bool m_authoritativeSnapshotReported = false;
        bool m_remoteSnapshotReported = false;
        bool m_remoteSnapshotDiagnosticReported = false;
    };

    class STWPlayerNetworkComponentController final
        : public STWPlayerNetworkComponentControllerBase
    {
    public:
        explicit STWPlayerNetworkComponentController(STWPlayerNetworkComponent& parent);

        void OnActivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;
        void OnDeactivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;

        void CreateInput(Multiplayer::NetworkInput& input, float deltaTime) override;
        void ProcessInput(Multiplayer::NetworkInput& input, float deltaTime) override;

    private:
        bool TryBindGameplayAuthority();
        void UnbindGameplayAuthority();
        void PopHeldCommand();

        bool m_gameplayAuthorityBound = false;
        static constexpr size_t MaxHeldCommands = 8;
        PlayerCommand m_heldCommands[MaxHeldCommands]{};
        size_t m_heldCount = 0;
        bool m_transportReleaseLogged = false;
        bool m_transportDropLogged = false;
    };

} // namespace STWGameplay
