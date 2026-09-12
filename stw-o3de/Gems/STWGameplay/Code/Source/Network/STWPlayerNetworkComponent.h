#pragma once

#include <Source/AutoGen/STWPlayerNetworkComponent.AutoComponent.h>

#include <STWGameplay/PlayerCommand.h>

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

        void OnInit() override;
        void OnActivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;
        void OnDeactivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;
        void OnNetworkActivated() override;
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
    };
} // namespace STWGameplay
