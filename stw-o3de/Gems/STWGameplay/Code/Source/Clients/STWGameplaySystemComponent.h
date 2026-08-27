#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <STWGameplay/PlayerSliceModel.h>
#include "PhysXPlayerRuntime.h"

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
        void RecordPerformance(float deltaTime);
        void UpdateAutomatedAcceptance(float deltaTime);
        // Attempts to create the PhysX controller once the O3DE default physics scene exists.
        void TryStartPhysics();

        // The PhysX character controller cannot be created during Activate() because the
        // default physics scene does not exist yet; creation is deferred to OnTick.
        enum class PhysicsStartup
        {
            Waiting,
            Ready,
            Failed
        };
        PhysicsStartup m_physicsStartup = PhysicsStartup::Waiting;

        PlayerSliceModel m_model;
        PhysXPlayerRuntime m_physicsPlayer;
        PlayerInput m_input;
        AZStd::string m_nativeCapturePath;
        float m_nativeCaptureDelay = 0.0f;
        bool m_nativeCaptureAttempted = false;
        AZStd::array<float, 2048> m_frameSamples{};
        size_t m_frameSampleCount = 0;
        float m_performanceDuration = 0.0f;
        bool m_performanceReported = false;
        bool m_automatedAcceptance = false;
        bool m_acceptanceReported = false;
        float m_acceptanceTime = 0.0f;
        AZ::Vector3 m_acceptanceStartPosition = AZ::Vector3::CreateZero();
    };
}
