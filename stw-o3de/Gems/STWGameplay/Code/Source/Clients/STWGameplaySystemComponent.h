#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <STWGameplay/CombatFeedbackPresentation.h>
#include <STWGameplay/EncounterModel.h>
#include <STWGameplay/EnemyPresentation.h>
#include <STWGameplay/SpawnCheckpointModel.h>
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
        void UpdateEncounterAcceptance();
        void UpdateSpawnCheckpointAcceptance();
        void UpdateWeaponSwitchAcceptance();
        void UpdateJumpAcceptance(bool physicalStateSynchronized);
        void UpdateCrouchAcceptance(bool physicalStateSynchronized);
        void UpdateSlideAcceptance(bool physicalStateSynchronized);
        void UpdateMantleAcceptance(bool physicalStateSynchronized);

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
        AZStd::array<AZ::Render::MeshFeatureProcessorInterface::MeshHandle, PlayerSliceModel::WeaponCount>
            m_viewmodelMeshHandles;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_fireFeedbackMeshHandle;
        AZStd::array<AZStd::string, PlayerSliceModel::WeaponCount> m_viewmodelMeshAssetPaths;
        size_t m_visibleViewmodelSlot = PlayerSliceModel::WeaponCount;
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
        EncounterModel m_encounter;
        SpawnCheckpointModel m_spawnCheckpoint;
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
        bool m_encounterAcceptanceReported = false;
        bool m_encounterAcceptanceFirstCompletion = false;
        bool m_encounterAcceptanceDuplicateBlocked = false;
        bool m_encounterAcceptanceRearmObserved = false;
        bool m_encounterAcceptancePostRearmActive = false;
        bool m_encounterAcceptanceSecondCompletion = false;
        bool m_encounterAcceptanceSecondEliminationTriggered = false;
        int m_encounterAcceptanceLastRespawnEvents = 0;
        bool m_weaponSwitchAcceptanceStarted = false;
        bool m_weaponSwitchFirstSwitchObserved = false;
        bool m_weaponSwitchFirstWeaponVisible = false;
        bool m_weaponSwitchHeldStable = false;
        bool m_weaponSwitchBAmmoChangedOnFire = false;
        bool m_weaponSwitchInactiveAUnchanged = true;
        bool m_weaponSwitchSecondSwitchObserved = false;
        bool m_weaponSwitchAAmmoPreserved = false;
        bool m_weaponSwitchAcceptanceReported = false;
        int m_weaponSwitchInitialSlot = -1;
        int m_weaponSwitchInitialAMagazine = 0;
        int m_weaponSwitchInitialAReserve = 0;
        int m_weaponSwitchInitialBMagazine = 0;
        int m_weaponSwitchInitialBReserve = 0;
        bool m_spawnCheckpointAcceptanceStarted = false;
        bool m_spawnCheckpointDefaultRespawnObserved = false;
        bool m_spawnCheckpointActivationObserved = false;
        bool m_spawnCheckpointDuplicateChecked = false;
        bool m_spawnCheckpointDuplicateBlocked = false;
        bool m_spawnCheckpointActivePersisted = false;
        bool m_spawnCheckpointPlayerDeathObserved = false;
        bool m_spawnCheckpointRespawnObserved = false;
        bool m_spawnCheckpointPhysicalPositionConfirmed = false;
        bool m_spawnCheckpointAcceptanceReported = false;
        AZ::Vector3 m_spawnCheckpointAcceptancePosition = AZ::Vector3::CreateZero();
        AZ::Vector3 m_spawnCheckpointLastPhysicalPosition = AZ::Vector3::CreateZero();
        int m_spawnCheckpointInitialActivationCount = 0;
        int m_spawnCheckpointInitialDeathEvents = 0;
        bool m_jumpAcceptanceStarted = false;
        bool m_jumpAcceptanceAirborne = false;
        bool m_jumpAcceptanceRose = false;
        bool m_jumpAcceptanceLanded = false;
        bool m_jumpAcceptanceReported = false;
        float m_jumpAcceptanceStartTime = 0.0f;
        float m_jumpAcceptanceStartHeight = 0.0f;
        int m_jumpAcceptanceInitialEvents = 0;
        bool m_crouchAcceptanceStarted = false;
        bool m_crouchAcceptanceCrouched = false;
        bool m_crouchAcceptanceStood = false;
        bool m_crouchAcceptanceBasePreserved = true;
        bool m_crouchAcceptanceCameraLowered = false;
        bool m_crouchAcceptanceReported = false;
        float m_crouchAcceptanceStartBaseZ = 0.0f;
        float m_crouchAcceptanceStandingHeight = 0.0f;
        bool m_slideAcceptanceStimulusStarted = false;
        bool m_slideAcceptanceStarted = false;
        bool m_slideAcceptanceSpeedDecayed = false;
        bool m_slideAcceptanceEnded = false;
        bool m_slideAcceptanceReported = false;
        bool m_slideAcceptanceGroundedStart = false;
        AZ::Vector3 m_slideAcceptanceStartPosition = AZ::Vector3::CreateZero();
        float m_slideAcceptanceStartSpeed = 0.0f;
        float m_slideAcceptanceEndSpeed = 0.0f;
        float m_slideAcceptanceMaxTravel = 0.0f;
        int m_slideAcceptanceInitialEvents = 0;
        bool m_mantleAcceptanceStimulusStarted = false;
        bool m_mantleAcceptanceStarted = false;
        bool m_mantleAcceptanceValidated = false;
        bool m_mantleAcceptanceAscended = false;
        bool m_mantleAcceptanceCompleted = false;
        bool m_mantleAcceptanceReported = false;
        bool m_mantleAcceptanceMovementReported = false;
        AZ::Vector3 m_mantleAcceptanceStartPosition = AZ::Vector3::CreateZero();
        float m_mantleAcceptanceStartZ = 0.0f;
        float m_mantleAcceptanceMaxZ = 0.0f;
        float m_mantleAcceptanceMaxForward = 0.0f;
        int m_mantleAcceptanceInitialEvents = 0;
        int m_mantleAcceptanceInitialJumpEvents = 0;
        int m_mantleAcceptanceInitialSlideEvents = 0;
    };
}
