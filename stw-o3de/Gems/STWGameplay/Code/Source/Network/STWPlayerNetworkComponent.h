#pragma once

#include <Source/AutoGen/STWPlayerNetworkComponent.AutoComponent.h>

#include <STWGameplay/PlayerCommand.h>
#include <STWGameplay/PlayerSimulationTypes.h>

namespace STWGameplay
{
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

        bool m_gameplayAuthorityBound = false;
    };

} // namespace STWGameplay
