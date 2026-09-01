#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <STWGameplay/ViewmodelPresentation.h>
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
        void UpdateAdsAcceptanceMarkers();
        void EmitAdsAcceptanceState(const char* phase, bool requested) const;
        // Attempts to create the PhysX controller once the O3DE default physics scene exists.
        void TryStartPhysics();
        // Attempts to acquire the real Atom viewmodel mesh once the render scene exists.
        void TryStartViewmodelMesh();
        // Drives the Atom mesh from the same first-person basis the presentation computes.
        void UpdateViewmodelMeshTransform(
            const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim, const AZ::Vector3& up);
        void ShutdownViewmodelMesh();

        // The PhysX character controller cannot be created during Activate() because the
        // default physics scene does not exist yet; creation is deferred to OnTick.
        enum class PhysicsStartup
        {
            Waiting,
            Ready,
            Failed
        };
        PhysicsStartup m_physicsStartup = PhysicsStartup::Waiting;

        // The Atom mesh feature processor only exists once the render scene is up, so the
        // real viewmodel mesh is acquired lazily in OnTick exactly like the PhysX controller.
        enum class ViewmodelMeshStartup
        {
            Waiting,
            Acquired,
            Failed
        };
        ViewmodelMeshStartup m_viewmodelMeshStartup = ViewmodelMeshStartup::Waiting;
        // Unit-box product scaled to the dimensions the previous procedural body used, so the
        // first-person framing is unchanged by the switch to a real mesh.
        static constexpr float ViewmodelMeshScaleX = 0.20f;
        static constexpr float ViewmodelMeshScaleY = 0.60f;
        static constexpr float ViewmodelMeshScaleZ = 0.16f;

        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_viewmodelMeshHandle;
        AZStd::string m_viewmodelMeshAssetPath;
        bool m_viewmodelMeshReported = false;

        PlayerSliceModel m_model;
        ViewmodelPresentation m_viewmodel;
        PhysXPlayerRuntime m_physicsPlayer;
        PlayerInput m_input;
        bool m_adsHeld = false;
        AZStd::string m_nativeCapturePath;
        float m_nativeCaptureDelay = 0.0f;
        bool m_nativeCaptureAttempted = false;
        AZStd::array<float, 2048> m_frameSamples{};
        size_t m_frameSampleCount = 0;
        float m_performanceDuration = 0.0f;
        bool m_performanceReported = false;
        bool m_automatedAcceptance = false;
        bool m_acceptanceReported = false;
        bool m_viewmodelAcceptanceReported = false;
        bool m_adsAcceptanceBegun = false;
        bool m_adsEnterReported = false;
        bool m_adsEndpointReported = false;
        bool m_adsFireReported = false;
        bool m_adsReloadReported = false;
        bool m_adsExitReported = false;
        bool m_adsReturnReported = false;
        bool m_adsAcceptanceReported = false;
        float m_acceptanceTime = 0.0f;
        AZ::Vector3 m_acceptanceStartPosition = AZ::Vector3::CreateZero();
    };
}
