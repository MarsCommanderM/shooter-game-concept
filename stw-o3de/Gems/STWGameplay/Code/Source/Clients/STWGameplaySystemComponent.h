#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <STWGameplay/CombatFeedbackPresentation.h>
#include <STWGameplay/EnemyPresentation.h>
#include <STWGameplay/ViewmodelPresentation.h>
#include "PhysXPlayerRuntime.h"
#include "PhysXEnemyRuntime.h"

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
        void UpdateSwayAcceptanceMarkers();
        // Attempts to create the PhysX controller once the O3DE default physics scene exists.
        void TryStartPhysics();
        // Attempts to acquire the real Atom viewmodel mesh once the render scene exists.
        void TryStartViewmodelMesh();
        // Drives the Atom mesh from the same first-person basis the presentation computes.
        void UpdateViewmodelMeshTransform(
            const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim, const AZ::Vector3& up);
        void ShutdownViewmodelMesh();
        void TryStartEnemyMesh();
        void UpdateEnemyMeshTransform();
        void ShutdownEnemyMesh();
        void TryStartArenaMesh();
        void ShutdownArenaMesh();
        void UpdateArenaAcceptance();
        void UpdateEnemyCombatAcceptance();
        void UpdateEnemyAiAcceptance(float deltaTime);
        void UpdateEnemyPresentationAcceptance();
        void UpdateCombatFeedbackAcceptance();

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
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_fireFeedbackMeshHandle;
        AZStd::string m_viewmodelMeshAssetPath;
        bool m_viewmodelMeshReported = false;
        ViewmodelMeshStartup m_enemyMeshStartup = ViewmodelMeshStartup::Waiting;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_enemyMeshHandle;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_impactFeedbackMeshHandle;
        AZStd::string m_enemyMeshAssetPath;
        bool m_enemyMeshReported = false;
        ViewmodelMeshStartup m_arenaMeshStartup = ViewmodelMeshStartup::Waiting;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_arenaMeshHandle;
        AZStd::string m_arenaMeshAssetPath;
        bool m_arenaMeshReported = false;
        bool m_arenaAcceptanceReported = false;

        PlayerSliceModel m_model;
        EnemyPresentation m_enemyPresentation;
        ViewmodelPresentation m_viewmodel;
        CombatFeedbackPresentation m_combatFeedback;
        PhysXPlayerRuntime m_physicsPlayer;
        PhysXEnemyRuntime m_physicsEnemy;
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
        bool m_swayAcceptanceBegun = false;
        bool m_swayPositiveReported = false;
        bool m_swayReturnReported = false;
        bool m_swayAcceptanceReported = false;
        float m_acceptanceTime = 0.0f;
        AZ::Vector3 m_acceptanceStartPosition = AZ::Vector3::CreateZero();
        AZ::Vector3 m_enemyAcceptanceStartPosition = AZ::Vector3::CreateZero();
        bool m_enemyPhysicsReady = false;
        bool m_enemyMoved = false;
        bool m_enemyCombatPrepared = false;
        bool m_enemyCombatAcceptanceReported = false;
        bool m_enemyAiAcceptanceReported = false;
        bool m_enemyAiPlayerDeathObserved = false;
        bool m_enemyAiPlayerRespawned = false;
        bool m_enemyAiLoopReactivated = false;
        float m_enemyAiRespawnDelay = 0.0f;
        bool m_enemyPresentationIdleObserved = false;
        bool m_enemyPresentationChaseObserved = false;
        bool m_enemyPresentationAttackObserved = false;
        bool m_enemyPresentationDeadObserved = false;
        bool m_enemyPresentationResetObserved = false;
        bool m_enemyPresentationAuthoritySeparated = true;
        bool m_enemyPresentationAcceptanceReported = false;
        bool m_combatFeedbackAuthoritySeparated = true;
        bool m_combatFeedbackAcceptanceReported = false;
    };
}
