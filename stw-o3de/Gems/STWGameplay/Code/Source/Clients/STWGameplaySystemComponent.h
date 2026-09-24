#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>
#include <AzFramework/Input/Channels/InputChannelId.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <STWGameplay/PlayerSimulationTypes.h>
#include <STWGameplay/DestructibleObjectModel.h>
#include <STWGameplay/FixedSimulationClock.h>
#include <STWGameplay/CharacterPhysicalState.h>
#include <STWGameplay/PlayerCommandHistory.h>
#include <STWGameplay/PlayerPrediction.h>
#include <STWGameplay/PresentationInterpolation.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <STWGameplay/CombatFeedbackPresentation.h>
#include <STWGameplay/EncounterModel.h>
#include <STWGameplay/EnemyPresentation.h>
#include <STWGameplay/SpawnCheckpointModel.h>
#include <STWGameplay/BodycamCameraPresentation.h>
#include <STWGameplay/AudioFeedbackPresentation.h>
#include <STWGameplay/ArenaPresentation.h>
#include <STWGameplay/EnvironmentPresentation.h>
#include <STWGameplay/MainMenuPresentation.h>
#include <STWGameplay/HudPresentation.h>
#include <STWGameplay/STWSkeletalCharacterPresentation.h>
#include <STWGameplay/STWFirstPersonArmsPresentation.h>
#include <STWGameplay/ViewmodelPresentation.h>
#include "PhysXArenaRuntime.h"
#include "PhysXPlayerRuntime.h"
#include "PhysXEnemyRuntime.h"
#include "STWMultiplayerRuntime.h"
#include "STWNetworkPlayerAuthority.h"

namespace STWGameplay
{
    class STWGameplaySystemComponent final
        : public AZ::Component
        , public AZ::TickBus::Handler
        , public AzFramework::InputChannelEventListener
    {
    public:
        AZ_COMPONENT_DECL(STWGameplaySystemComponent);

        static constexpr size_t MaxNetworkPlayerCount = 8;

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        void Activate() override;
        void Deactivate() override;
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        bool StartMultiplayerHost(uint16_t port, bool isDedicated = true)
        {
            return m_multiplayer.StartHosting(port, isDedicated);
        }

        bool ConnectMultiplayer(const AZStd::string& remoteAddress, uint16_t port)
        {
            return m_multiplayer.Connect(remoteAddress, port);
        }

        STWMultiplayerTransportState GetMultiplayerState() const
        {
            return m_multiplayer.GetState();
        }

        const AuthoritativePlayerSnapshot& GetAuthoritativeSnapshot() const;

        const PlayerCommandHistory& GetPlayerCommandHistory() const;

        //! Evaluates an externally supplied authoritative snapshot and prunes only commands
        //! explicitly acknowledged by it. This boundary never applies correction or replay.
        ReconciliationEvaluation ProcessAuthoritativeSnapshot(
            const AuthoritativePlayerSnapshot& authoritativeSnapshot);

        const ReconciliationEvaluation& GetLastReconciliationEvaluation() const;

        //! Binds one network entity to one independent player authority slot. The first slot
        //! adapts the existing local authority for compatibility; later slots own their player
        //! state and physical runtime while sharing the world enemy authority.
        bool BindNetworkPlayer(AZ::EntityId entityId);
        void UnbindNetworkPlayer(AZ::EntityId entityId);
        bool CreateNetworkCommand(AZ::EntityId entityId, PlayerCommand& command);
        bool SubmitNetworkCommand(AZ::EntityId entityId, const PlayerCommand& command);
        //! Routes one replicated snapshot from the currently bound network entity to the
        //! existing pure reconciliation policy. It never applies correction or replay.
        bool ReceiveNetworkSnapshot(AZ::EntityId entityId, const AuthoritativePlayerSnapshot& snapshot);
        bool BindRemoteNetworkPlayer(AZ::EntityId entityId);
        const AuthoritativePlayerSnapshot* GetRemoteNetworkSnapshot(AZ::EntityId entityId) const;
        const PresentationFrameState* GetRemotePlayerPresentationState(AZ::EntityId entityId) const;

        size_t GetNetworkPlayerCount() const;
        size_t GetNetworkPlayerCommandHistorySize(AZ::EntityId entityId) const;

    private:
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;
        void SampleGamepadLook(float deltaTime);
        void UpdateCamera();
        void DrawPresentation(float deltaTime);
        void RecordPerformance(float deltaTime);
        void UpdateAutomatedAcceptance(float deltaTime);
        void UpdateAdsAcceptanceMarkers();
        void EmitAdsAcceptanceState(const char* phase, bool requested) const;
        void UpdateSwayAcceptanceMarkers();
        // Attempts to create the PhysX controller once the O3DE default physics scene exists.
        void TryStartPhysics();
        // Re-enters the existing startup path after O3DE replaces the default physics scene.
        void ResetPhysicsAfterSceneLoss();
        void ShutdownEnemyPhysics();
        void SynchronizeSkeletalCharacterPhysicalState();
        void UpdateRemotePlayerPresentation(float deltaTime);
        void ReleaseRemotePlayerPresentation(AZ::EntityId entityId);
        void UpdateEnemyPresentationInterpolation(
            const AZStd::array<bool, EnemyCollectionModel::MaxEnemyCount>& physicalStateSynchronized);
        PlayerCommand BuildPlayerCommand(const PlayerInput& input);
        void TryBeginMantle(
            PlayerSliceModel& model, PhysXPlayerRuntime& physics, const PlayerInput& input);
        struct FixedSimulationFrameResult
        {
            PlayerCommand m_lastCommand;
            AZ::Vector3 m_requestedVelocity = AZ::Vector3::CreateZero();
            bool m_gameplayUpdated = false;
            bool m_jumpImpulseObserved = false;
            float m_jumpImpulse = 0.0f;
            bool m_shotFired = false;
            bool m_hit = false;
            EnemyId m_hitEnemyId = InvalidEnemyId;
            bool m_equipmentUsed = false;
            bool m_equipmentChanged = false;
            AZ::u32 m_fixedStepCount = 0;
        };
        FixedSimulationFrameResult RunFixedGameplaySteps(float frameDelta);
        void CaptureAuthoritativeSnapshot(PlayerCommandSequence acknowledgedCommandSequence);
        void RunAdditionalNetworkPlayerSteps(AZ::u32 stepCount);
        void CaptureNetworkPlayerSnapshot(STWNetworkPlayerAuthority& authority);
        void PublishNetworkPlayerSnapshot(
            AZ::EntityId entityId, const AuthoritativePlayerSnapshot& snapshot);
        STWNetworkPlayerAuthority* FindNetworkPlayer(AZ::EntityId entityId);
        const STWNetworkPlayerAuthority* FindNetworkPlayer(AZ::EntityId entityId) const;
        STWNetworkPlayerAuthority* FindCompositionRootNetworkPlayer();
        const STWNetworkPlayerAuthority* FindCompositionRootNetworkPlayer() const;
        STWNetworkPlayerAuthority* FindFirstNetworkPlayer();
        const STWNetworkPlayerAuthority* FindFirstNetworkPlayer() const;
        void UnbindAllNetworkPlayers();
        // Attempts to acquire the real Atom viewmodel mesh once the render scene exists.
        void TryStartViewmodelMesh();
        // Drives the Atom mesh from the same first-person basis the presentation computes.
        void UpdateViewmodelMeshTransform(
            const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim, const AZ::Vector3& up);
        void ReportViewmodelRuntimeIdentity(
            size_t slot, const AZ::Vector3& cameraPosition, const AZ::Vector3& right, const AZ::Vector3& aim,
            const AZ::Vector3& up);
        void ShutdownViewmodelMesh();
        void TryStartEnemyMesh();
        void UpdateEnemyMeshTransform();
        void ShutdownEnemyMesh();
        void TryStartArenaMesh();
        void ShutdownArenaMesh();
        void UpdateArenaAcceptance();
        void UpdateEnemyCombatAcceptance();
        void UpdateMultiEnemyAcceptance();
        void UpdateEnemyAiAcceptance(float deltaTime);
        void UpdateEnemyPresentationAcceptance();
        void UpdateSkeletalCharacterAcceptance();
        void UpdateBodycamAcceptance();
        void UpdateMainMenuAcceptance();
        //! Real-play-only driver (mirrors UpdateInteractivePlayerRespawn's
        //! own m_automatedAcceptance gate) for the Niederlage/Sieg overlay -
        //! kept out of the scripted acceptance battery so it cannot disturb
        //! that battery's own timing-sensitive combat/respawn/encounter
        //! counters. EvaluateEndGameState() holds the actual, ungated logic
        //! so UpdateMainMenuAcceptance() can call it directly for proof.
        void UpdateEndGameFlow();
        void UpdateHudAcceptance();
        //! Links DestructibleObjectModel's fixed small object set to the
        //! real world AABBs already defined for "STW Left Cover"/"STW Right
        //! Cover" in PhysXArenaRuntime::GetStaticColliderDescriptions() - no
        //! duplicated/hand-copied coordinates. Called once real static
        //! collision exists (right after m_physicsArena.Initialize()
        //! succeeds in TryStartPhysics()).
        void ConfigureDestructibleObjects();
        //! Loads the real mesh+material once (AssetProcessor-compiled from
        //! Project/Assets/Environment/STW_Destructible_Cover_01/ and the
        //! Industrial Yard's own real steel material) and acquires one real
        //! Atom mesh instance per configured object - same
        //! AcquireMesh/MeshHandleDescriptor pattern already proven for the
        //! viewmodel/enemy meshes.
        void TryStartDestructibleObjects();
        //! Every tick: reflects DestructibleObjectModel's real m_active
        //! state into the matching PhysX collider (RigidBodyRequestBus::
        //! DisablePhysics, via PhysXArenaRuntime::FindColliderEntityByName)
        //! and the matching Atom mesh's visibility
        //! (MeshFeatureProcessorInterface::SetVisible) - edge-triggered, not
        //! reapplied every tick once already reflected.
        void UpdateDestructibleObjects();
        void UpdateDestructibleAcceptance();
        void EvaluateEndGameState();
        //! Extracted from UpdateInteractivePlayerRespawn() so both the timed
        //! real-play respawn and the End-Game screen's "WEITER" button (an
        //! explicit, user-requested respawn) share one real implementation.
        void RespawnPlayer();
        void UpdateCombatFeedbackAcceptance();
        void UpdateAudioAcceptance();
        void UpdateEncounterAcceptance();
        void UpdateSpawnCheckpointAcceptance();
        void UpdateWeaponSwitchAcceptance();
        void UpdateLoadoutAcceptance();
        void UpdateJumpAcceptance(bool physicalStateSynchronized);
        void UpdateCrouchAcceptance(bool physicalStateSynchronized);
        void UpdateSlideAcceptance(bool physicalStateSynchronized);
        void UpdateMantleAcceptance(bool physicalStateSynchronized);
        // Real interactive play has no scripted acceptance driver to respawn the player, so a
        // dead player previously stayed dead forever outside the T4 gate's scripted sequence.
        // Mutually exclusive with the gate's one-shot acceptance respawn in
        // UpdateEnemyAiAcceptance(): that path only runs when m_automatedAcceptance is true,
        // this one only when it is false.
        void UpdateInteractivePlayerRespawn(float deltaTime);

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
        AZStd::array<AZ::Render::MeshFeatureProcessorInterface::MeshHandle, PlayerSliceModel::EquipmentProfileCount>
            m_viewmodelMeshHandles;
        AZStd::array<bool, PlayerSliceModel::EquipmentProfileCount> m_viewmodelMaterialsApplied{};
        uint32_t m_viewmodelRuntimeDiagnosticAttempts = 0;
        bool m_viewmodelRuntimeDiagnosticReported = false;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_fireFeedbackMeshHandle;
        AZStd::array<AZStd::string, PlayerSliceModel::EquipmentProfileCount> m_viewmodelMeshAssetPaths;
        size_t m_visibleViewmodelSlot = PlayerSliceModel::EquipmentProfileCount;
        bool m_viewmodelMeshReported = false;
        ViewmodelMeshStartup m_enemyMeshStartup = ViewmodelMeshStartup::Waiting;
        AZStd::array<AZ::Render::MeshFeatureProcessorInterface::MeshHandle, EnemyCollectionModel::MaxEnemyCount>
            m_enemyMeshHandles;
        AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_impactFeedbackMeshHandle;
        AZStd::string m_enemyMeshAssetPath;
        bool m_enemyMeshReported = false;
        ViewmodelMeshStartup m_destructibleObjectsStartup = ViewmodelMeshStartup::Waiting;
        AZ::Data::Asset<AZ::RPI::MaterialAsset> m_destructibleMaterialAsset;
        AZ::Data::Instance<AZ::RPI::Material> m_destructibleMaterial;
        AZStd::array<AZ::Render::MeshFeatureProcessorInterface::MeshHandle, DestructibleObjectModel::MaxObjectCount>
            m_destructibleMeshHandles;
        // Edge-trigger: only DisablePhysics()/SetVisible(false) once per
        // object, not every tick after it's already destroyed.
        AZStd::array<bool, DestructibleObjectModel::MaxObjectCount> m_destructibleReflectedInactive{};
        bool m_destructibleObjectsReported = false;
        ViewmodelMeshStartup m_arenaMeshStartup = ViewmodelMeshStartup::Waiting;
        bool m_arenaMeshReported = false;
        bool m_arenaAcceptanceReported = false;
        ArenaPresentation m_arenaPresentation;
        EnvironmentPresentation m_environmentPresentation;
        MainMenuPresentation m_mainMenuPresentation;
        bool m_mainMenuInitialized = false;
        bool m_mainMenuAcceptanceReported = false;
        HudPresentation m_hudPresentation;
        bool m_hudInitialized = false;
        bool m_hudAcceptanceReported = false;

        // True while the Niederlage/Sieg overlay is showing - suppresses
        // UpdateInteractivePlayerRespawn's timer-based auto-respawn so the
        // player doesn't silently pop back into the arena underneath the
        // overlay; the overlay's own "WEITER" button is the only way to
        // clear it in real play (see EvaluateEndGameState() and
        // MainMenuPresentation::SetEndGameContinueHandler()).
        bool m_gameOverActive = false;
        // Edge-trigger flags so ShowScreen(EndGame) fires once per
        // death/completion transition, not every tick.
        bool m_defeatShown = false;
        bool m_victoryShown = false;

        // Real, rebindable keyboard bindings - initialized to exactly the
        // key IDs OnInputChannelEventFiltered used to have hardcoded, so
        // rebinding is opt-in behavior change, not a silent default change.
        // Gamepad and the two always-on mouse actions (fire/ADS) are not
        // exposed for rebinding in this pass.
        struct STWInputBindings
        {
            AzFramework::InputChannelId m_forward{ AzFramework::InputDeviceKeyboard::Key::AlphanumericW };
            AzFramework::InputChannelId m_back{ AzFramework::InputDeviceKeyboard::Key::AlphanumericS };
            AzFramework::InputChannelId m_left{ AzFramework::InputDeviceKeyboard::Key::AlphanumericA };
            AzFramework::InputChannelId m_right{ AzFramework::InputDeviceKeyboard::Key::AlphanumericD };
            AzFramework::InputChannelId m_jump{ AzFramework::InputDeviceKeyboard::Key::EditSpace };
            AzFramework::InputChannelId m_crouch{ AzFramework::InputDeviceKeyboard::Key::ModifierCtrlL };
            AzFramework::InputChannelId m_sprint{ AzFramework::InputDeviceKeyboard::Key::ModifierShiftL };
            AzFramework::InputChannelId m_reload{ AzFramework::InputDeviceKeyboard::Key::AlphanumericR };
        };
        STWInputBindings m_inputBindings;
        // Set by a "REBIND" button click (via MainMenuPresentation's
        // controls-rebind handler); the next real key/mouse-button press
        // OnInputChannelEventFiltered sees is captured into m_inputBindings
        // instead of being interpreted as gameplay input - see
        // TryCaptureRebind().
        AZStd::string m_pendingRebindAction;
        bool m_awaitingRebindKey = false;
        bool TryCaptureRebind(const AzFramework::InputChannelId& id, bool stateBegan);
        void StartRebind(const char* actionId);
        void SyncControlLabels();
        static AZStd::string GetKeyDisplayName(const AzFramework::InputChannelId& id);
        //! Persists m_inputBindings to @user@/stw_input_bindings.cfg (one
        //! real AzFramework::InputChannelId name per line, fixed order) so a
        //! rebind survives past this session - called after every
        //! successful rebind in TryCaptureRebind(). A missing file is not
        //! an error (first run, or bindings were never changed).
        void SaveInputBindings() const;
        //! Loads m_inputBindings back from the same file, called once from
        //! Activate() before the input listener connects. A missing or
        //! malformed file (wrong line count) leaves the compiled-in
        //! defaults untouched rather than guessing which lines are still
        //! trustworthy.
        void LoadInputBindings();

        PlayerSliceModel m_model;
        BodycamCameraPresentation m_bodycamCameraPresentation;
        AZStd::array<EnemyPresentation, EnemyCollectionModel::MaxEnemyCount> m_enemyPresentations;
        STWSkeletalCharacterPresentation m_skeletalCharacterPresentation;
        // Animated characters for enemies 1..MaxEnemyCount-1 (index 0 uses m_skeletalCharacterPresentation, which also
        // carries the gate's acceptance state). Presentation only; index 0 of this array stays unused.
        AZStd::array<STWSkeletalCharacterPresentation, EnemyCollectionModel::MaxEnemyCount> m_enemyCharacterPresentations;
        ViewmodelPresentation m_viewmodel;
        // Skinned arms/gloves/integrated-rifle presentation for STW_RIFLE_02 only (the profile
        // the STW_FP_01 asset was authored for); other profiles keep the static viewmodel mesh.
        STWFirstPersonArmsPresentation m_firstPersonArms;
        bool m_firstPersonArmsProfileWasActive = false;
        CombatFeedbackPresentation m_combatFeedback;
        AudioFeedbackPresentation m_audioFeedback;
        EncounterModel m_encounter;
        SpawnCheckpointModel m_spawnCheckpoint;
        PhysXArenaRuntime m_physicsArena;
        PhysXPlayerRuntime m_physicsPlayer;
        AZStd::array<STWNetworkPlayerAuthority, MaxNetworkPlayerCount> m_networkPlayerAuthorities;
        STWMultiplayerRuntime m_multiplayer;
        AZStd::array<PhysXEnemyRuntime, EnemyCollectionModel::MaxEnemyCount> m_enemyPhysicsRuntimes;
        AZStd::array<PresentationInterpolation, EnemyCollectionModel::MaxEnemyCount>
            m_enemyPresentationInterpolations;
        AZStd::array<STWSkeletalCharacterPresentation, MaxNetworkPlayerCount> m_remotePlayerPresentations;
        AZStd::array<AZ::EntityId, MaxNetworkPlayerCount> m_remotePlayerPresentationEntities;
        AZStd::array<bool, MaxNetworkPlayerCount> m_remotePlayerPresentationReported{};
        AZStd::array<bool, MaxNetworkPlayerCount> m_remotePlayerPresentationRenderReported{};
        AZStd::array<int, EnemyCollectionModel::MaxEnemyCount> m_enemyPresentationRespawnEvents{};
        // Composition-root-owned handoff state for the primary EMotionFX character. Native
        // ragdoll ownership remains unavailable until the character asset supplies a verified
        // ragdoll configuration and runtime adapter.
        CharacterPhysicalState m_skeletalCharacterPhysicalState;
        int m_skeletalCharacterRespawnEvents = 0;
        FixedSimulationClock m_fixedSimulationClock;
        PlayerInput m_input;
        float m_pendingLookX = 0.0f;
        float m_pendingLookY = 0.0f;
        bool m_pendingReload = false;
        PlayerCommandHistory m_commandHistory;
        AuthoritativePlayerSnapshot m_authoritativeSnapshot;
        PlayerCommandSequence m_nextCommandSequence = InvalidPlayerSimulationSequence;
        PlayerSnapshotSequence m_nextSnapshotSequence = InvalidPlayerSimulationSequence;
        PlayerSnapshotSequence m_physicalReadbackSequence = InvalidPlayerSimulationSequence;
        PlayerSnapshotSequence m_lastAcceptedSnapshotSequence = InvalidPlayerSimulationSequence;
        ReconciliationEvaluation m_lastReconciliationEvaluation;
        bool m_adsHeld = false;
        // Analog thumb-stick deflection persists between input events (unlike a mouse delta,
        // a stick reports its current position, not a movement delta), so look integration
        // happens once per fixed step in SampleGamepadLook() rather than accumulating directly
        // in the input-channel callback.
        float m_gamepadLookStickX = 0.0f;
        float m_gamepadLookStickY = 0.0f;
        static constexpr float GamepadLookDeadZone = 0.15f;
        static constexpr float GamepadLookSensitivity = 3.5f; // radians/sec at full deflection; needs real-pad tuning
        AZStd::string m_nativeCapturePath;
        float m_nativeCaptureDelay = 0.0f;
        bool m_nativeCaptureAttempted = false;
        // Single-capture mode only: simulation time spent waiting for the arena visual
        // variant to settle before the 0.75 s capture delay starts.
        float m_nativeCaptureSettleTime = 0.0f;
        // Opt-in, off by default (unset in production and in the standard task.sh gate):
        // periodic re-capture into indexed sibling files, for building a real gameplay
        // frame sequence instead of one still frame.
        float m_nativeCaptureIntervalSeconds = 0.0f;
        int m_nativeCaptureMaxFrames = 0;
        int m_nativeCaptureFrameIndex = 0;
        AZStd::array<float, 2048> m_frameSamples{};
        size_t m_frameSampleCount = 0;
        float m_performanceDuration = 0.0f;
        bool m_performanceReported = false;
        // Budget-protocol profile (Docs/PerformanceBudgets): 30 s warmup, then a 60 s window
        // of gapless per-frame samples for frame, main-thread CPU and pass-extent GPU time.
        static constexpr size_t ProfileSampleCapacity = 8192;
        void RecordPerformanceProfile(float deltaTime);
        AZStd::vector<float> m_profileFrameMs;
        AZStd::vector<float> m_profileCpuMs;
        AZStd::vector<float> m_profileGpuMs;
        float m_profileElapsed = 0.0f;
        float m_profileWindowElapsed = 0.0f;
        double m_profileLastCpuSeconds = -1.0;
        bool m_profileGpuQueriesEnabled = false;
        bool m_profileReported = false;
        bool m_automatedAcceptance = false;
        float m_interactiveRespawnDelay = 0.0f;
        static constexpr float InteractiveRespawnDelaySeconds = 2.0f;
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
        bool m_skeletalCharacterAcceptanceReported = false;
        bool m_skeletalPresentationAuthoritySeparated = true;
        bool m_bodycamAcceptanceReported = false;
        bool m_multiEnemyPrepared = false;
        bool m_multiEnemyInitialActiveSet = false;
        bool m_multiEnemyFirstEliminationObserved = false;
        bool m_multiEnemySecondEliminationObserved = false;
        bool m_multiEnemyThirdEliminationObserved = false;
        bool m_multiEnemyActiveAfterFirstElimination = false;
        bool m_multiEnemyActiveAfterSecondElimination = false;
        bool m_multiEnemyDuplicateCompletionBlocked = false;
        bool m_multiEnemyRearmObserved = false;
        bool m_multiEnemyPostRearmActive = false;
        bool m_multiEnemyIndependentHealth = false;
        bool m_multiEnemyIndependentAi = false;
        bool m_multiEnemyIndependentPhysical = false;
        bool m_multiEnemySecondCycleBEliminated = false;
        bool m_multiEnemySecondCycleCEliminated = false;
        bool m_multiEnemyAcceptanceReported = false;
        bool m_combatFeedbackAuthoritySeparated = true;
        bool m_combatFeedbackAcceptanceReported = false;
        bool m_audioAuthoritySeparated = true;
        bool m_audioAcceptanceReported = false;
        bool m_audioEnemyBaselineCaptured = false;
        EnemyBehaviorState m_audioPreviousEnemyState = EnemyBehaviorState::Idle;
        int m_audioPreviousEnemyAttackEvents = 0;
        int m_audioPreviousEnemyDeathEvents = 0;
        int m_audioPreviousRespawnEvents = 0;
        bool m_encounterAcceptanceReported = false;
        bool m_encounterAcceptanceFirstCompletion = false;
        bool m_encounterAcceptanceDuplicateBlocked = false;
        bool m_encounterAcceptanceRearmObserved = false;
        bool m_encounterAcceptancePostRearmActive = false;
        bool m_encounterAcceptanceSecondCompletion = false;
        bool m_encounterAcceptanceSecondEliminationTriggered = false;
        int m_encounterAcceptanceLastRespawnEvents = 0;
        enum class WeaponSwitchAcceptancePhase
        {
            WaitingForReset,
            InitialSwitch,
            InitialSwitchRelease,
            PostResetRelease,
            EnsureSecondary,
            EnsureSecondaryRelease,
            SecondSwitch,
            Complete
        };
        WeaponSwitchAcceptancePhase m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::WaitingForReset;
        float m_weaponSwitchAcceptancePhaseStartTime = 0.0f;
        bool m_weaponSwitchAcceptanceStarted = false;
        bool m_weaponSwitchFirstSwitchObserved = false;
        bool m_weaponSwitchFirstWeaponVisible = false;
        bool m_weaponSwitchHeldStable = false;
        bool m_weaponSwitchBAmmoChangedOnFire = false;
        bool m_weaponSwitchInactiveAUnchanged = true;
        bool m_weaponSwitchSecondSwitchObserved = false;
        bool m_weaponSwitchAAmmoPreserved = false;
        bool m_weaponSwitchAcceptanceReported = false;
        bool m_weaponSwitchDiagnosticReported = false;
        bool m_weaponSwitchPostResetDiagnosticReported = false;
        bool m_weaponSwitchResetComplete = false;
        int m_weaponSwitchInitialRespawnEvents = 0;
        bool m_weaponSwitchPostResetReleaseObserved = false;
        bool m_weaponSwitchEnsureSecondaryInputAsserted = false;
        bool m_weaponSwitchEnsureSecondaryEdge = false;
        int m_weaponSwitchEnsureSecondarySlotBefore = -1;
        int m_weaponSwitchEnsureSecondarySlotAfter = -1;
        bool m_weaponSwitchPreSecondSwitchReleaseObserved = false;
        bool m_weaponSwitchSecondSwitchInputAsserted = false;
        bool m_weaponSwitchSecondSwitchEdge = false;
        int m_weaponSwitchSecondSwitchSlotBefore = -1;
        int m_weaponSwitchSecondSwitchSlotAfter = -1;
        int m_weaponSwitchSecondSwitchTransitionEventDelta = 0;
        int m_weaponSwitchSecondSwitchObservationCount = 0;
        bool m_weaponSwitchPostResetBaselineCaptured = false;
        int m_weaponSwitchPostResetAMagazine = 0;
        int m_weaponSwitchPostResetAReserve = 0;
        bool m_weaponBFireDiagnosticStarted = false;
        bool m_weaponBFireDiagnosticReported = false;
        int m_weaponBFireSlotBefore = -1;
        bool m_weaponBFireSelectedBefore = false;
        bool m_weaponBFirePlayerAliveBefore = false;
        float m_weaponBFireHealthBefore = 0.0f;
        int m_weaponBFireDamageEventsBefore = 0;
        int m_weaponBFireDeathEventsBefore = 0;
        int m_weaponBFireRespawnEventsBefore = 0;
        int m_weaponBFireAmmoBefore = 0;
        bool m_weaponBFireInputAsserted = false;
        bool m_weaponBFireInputHeld = false;
        bool m_weaponBFireInputEdge = false;
        bool m_weaponBFireRequestReachedModel = false;
        bool m_weaponBFireAcceptedByModel = false;
        bool m_weaponBFireRejectedByModel = false;
        bool m_weaponBFireSelectedDuringWindow = false;
        bool m_weaponBFirePreviousInput = false;
        int m_weaponBFireEventCount = 0;
        int m_weaponSwitchInitialSlot = -1;
        int m_weaponSwitchInitialAMagazine = 0;
        int m_weaponSwitchInitialAReserve = 0;
        int m_weaponSwitchInitialBMagazine = 0;
        int m_weaponSwitchInitialBReserve = 0;
        int m_weaponSwitchInactiveAAmmoAfterBFire = 0;
        bool m_weaponSwitchInactiveAAmmoAfterBFireCaptured = false;
        bool m_loadoutAcceptanceReported = false;
        bool m_loadoutDiagnosticReported = false;
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
        bool m_jumpAcceptanceSingleEventObserved = false;
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
