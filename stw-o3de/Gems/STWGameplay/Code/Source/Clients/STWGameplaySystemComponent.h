#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <STWGameplay/PlayerSliceModel.h>

namespace STWGameplay
{
    class STWGameplaySystemComponent final
        : public AZ::Component
        , public AZ::TickBus::Handler
        , public AzFramework::InputChannelEventListener
    {
    public:
        AZ_COMPONENT_DECL(STWGameplaySystemComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        void Activate() override;
        void Deactivate() override;
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private:
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;
        void UpdateCamera();
        void DrawPresentation();

        PlayerSliceModel m_model;
        PlayerInput m_input;
    };
}
