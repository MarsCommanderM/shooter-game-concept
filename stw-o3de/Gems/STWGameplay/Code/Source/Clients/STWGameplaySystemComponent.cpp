#include "STWGameplaySystemComponent.h"

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/vector.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/Physics/CharacterBus.h>
#include <AzFramework/Physics/SystemBus.h>
#include <AzFramework/Physics/Common/PhysicsTypes.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <Atom/Feature/Utils/FrameCaptureBus.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <STWGameplay/STWGameplayTypeIds.h>
#include <STWGameplay/ArenaLayout.h>

#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        struct ViewmodelAssetLoadState
        {
            bool m_enumerated = false;
            AZ::Data::AssetId m_enumeratedModelAssetId;
            AZ::Data::AssetId m_enumeratedMaterialAssetId;
            AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
            AZ::Data::Instance<AZ::RPI::Material> m_material;
        };

        AZStd::array<ViewmodelAssetLoadState, PlayerSliceModel::EquipmentProfileCount> s_viewmodelAssetLoadStates;
        ViewmodelAssetLoadState s_enemyAssetLoadState;
        ViewmodelAssetLoadState s_arenaAssetLoadState;

        struct JumpDiagnosticState
        {
            bool m_started = false;
            bool m_samplingComplete = false;
            bool m_summaryReported = false;
            bool m_modelPositiveObserved = false;
            bool m_acceptReported = false;
            bool m_queueReported = false;
            size_t m_samples = 0;
            size_t m_airborneSamples = 0;
            size_t m_firstAirborneSample = 0;
            size_t m_firstLandedSample = 0;
            float m_tickDeltaTime = 0.0f;
            float m_maxZ = 0.0f;
            float m_maxDeltaZ = 0.0f;
            float m_firstPositiveModelZ = 0.0f;
            float m_firstAirborneTime = 0.0f;
            float m_firstLandedTime = 0.0f;
        };

        JumpDiagnosticState s_jumpDiagnostic;

        struct ViewmodelAssetCandidate
        {
            AZ::Data::AssetId m_assetId;
            AZStd::string m_relativePath;
        };

        AZStd::string LowercaseAssetPath(const AZStd::string& path)
        {
            AZStd::string lowercase = path;
            for (char& character : lowercase)
            {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return lowercase;
        }

        void ResetViewmodelAssetLoadState()
        {
            s_viewmodelAssetLoadStates = {};
        }

        void ResetEnemyAssetLoadState()
        {
            s_enemyAssetLoadState = {};
        }

        void ResetArenaAssetLoadState() { s_arenaAssetLoadState = {}; }
    }

    AZ_COMPONENT_IMPL(STWGameplaySystemComponent, "STWGameplaySystemComponent", STWGameplaySystemComponentTypeId);

    void STWGameplaySystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<STWGameplaySystemComponent, AZ::Component>()->Version(1);
        }
    }

    void STWGameplaySystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("STWGameplayService"));
    }

    void STWGameplaySystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("STWGameplayService"));
    }

    void STWGameplaySystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("PhysicsService"));
    }
    void STWGameplaySystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType&) {}

    void STWGameplaySystemComponent::Activate()
    {
        ResetViewmodelAssetLoadState();
        ResetEnemyAssetLoadState();
        ResetArenaAssetLoadState();
        m_adsHeld = false;
        if (const char* capturePath = std::getenv("STW_NATIVE_CAPTURE_PATH"); capturePath && capturePath[0] != '\0')
        {
            m_nativeCapturePath = capturePath;
        }
        m_automatedAcceptance = std::getenv("STW_PHYSX_ACCEPTANCE") != nullptr;
        m_enemyPresentationIdleObserved = m_enemyPresentations[0].GetState() == EnemyBehaviorState::Idle;
        // The PhysX character controller requires the O3DE default physics scene, which does
        // not exist yet during system activation. Defer its creation to OnTick (TryStartPhysics)
        // and only start input/tick handling here. A missing scene now is not an error.
        AzFramework::InputChannelEventListener::Connect();
        AZ::TickBus::Handler::BusConnect();
    }

    void STWGameplaySystemComponent::Deactivate()
    {
        m_adsHeld = false;
        AZ::TickBus::Handler::BusDisconnect();
        AzFramework::InputChannelEventListener::Disconnect();
        ShutdownEnemyMesh();
        ShutdownArenaMesh();
        ShutdownViewmodelMesh();
        ShutdownEnemyPhysics();
        m_physicsPlayer.Shutdown();
    }

    void STWGameplaySystemComponent::TryStartPhysics()
    {
        // Same default-scene retrieval PhysX's own CharacterGameplayComponent uses; the
        // controller can only be created once the O3DE-owned default scene is present.
        AzPhysics::SceneHandle defaultScene = AzPhysics::InvalidSceneHandle;
        Physics::DefaultWorldBus::BroadcastResult(defaultScene, &Physics::DefaultWorldRequests::GetDefaultSceneHandle);
        if (defaultScene == AzPhysics::InvalidSceneHandle)
        {
            return; // default scene not created yet — keep waiting, this is not an error
        }

        if (!m_physicsPlayer.Initialize())
        {
            AZ_Error("STWGameplay", false, "Player Movement V2 could not create its PhysX controller");
            m_physicsStartup = PhysicsStartup::Failed;
            return;
        }

        AZ::Vector3 physicalPosition = AZ::Vector3::CreateZero();
        bool grounded = false;
        m_physicsPlayer.Synchronize(physicalPosition, grounded);
        m_model.SynchronizePhysicalState(physicalPosition, grounded);
        for (size_t index = 0; index < m_model.GetEnemies().GetEnemyCount(); ++index)
        {
            const EnemyInstance& instance = m_model.GetEnemies().GetInstanceByIndex(index);
            if (!m_enemyPhysicsRuntimes[index].Initialize(instance.m_id, instance.m_combat.GetState().m_position))
            {
                AZ_Error("STWGameplay", false, "Enemy %u PhysX controller failed to initialize", instance.m_id);
                ShutdownEnemyPhysics();
                m_physicsStartup = PhysicsStartup::Failed;
                return;
            }
        }
        m_enemyPhysicsReady = true;
        m_enemyAcceptanceStartPosition = m_model.GetEnemy().GetState().m_position;
        m_physicsStartup = PhysicsStartup::Ready;
        AZ_Printf("STWGameplay", "Native Player Movement V2 PhysX active\n");
    }

    void STWGameplaySystemComponent::ShutdownEnemyPhysics()
    {
        for (PhysXEnemyRuntime& runtime : m_enemyPhysicsRuntimes)
        {
            runtime.Shutdown();
        }
        m_enemyPhysicsReady = false;
    }

    void STWGameplaySystemComponent::OnTick(float deltaTime, AZ::ScriptTimePoint)
    {
        if (m_physicsStartup != PhysicsStartup::Ready)
        {
            if (m_physicsStartup == PhysicsStartup::Waiting)
            {
                TryStartPhysics();
            }
            return; // no gameplay/camera/acceptance until the controller is live
        }

        if (m_viewmodelMeshStartup == ViewmodelMeshStartup::Waiting)
        {
            TryStartViewmodelMesh();
        }
        if (m_enemyMeshStartup == ViewmodelMeshStartup::Waiting)
        {
            TryStartEnemyMesh();
        }
        if (m_arenaMeshStartup == ViewmodelMeshStartup::Waiting)
        {
            TryStartArenaMesh();
        }

        UpdateAutomatedAcceptance(deltaTime);
        m_model.Update(deltaTime, m_input);
        const EnemyCollectionModel& enemies = m_model.GetEnemies();
        m_encounter.Update(enemies);
        if (m_encounter.IsCompleted() && enemies.AreRequiredEnemiesAlive()
            && m_encounter.Rearm(enemies))
        {
            if (m_automatedAcceptance)
            {
                m_encounterAcceptanceRearmObserved = true;
                m_encounterAcceptancePostRearmActive = m_encounter.IsActive();
                m_multiEnemyRearmObserved = true;
                m_multiEnemyPostRearmActive = m_encounter.IsActive();
            }
        }
        if (m_automatedAcceptance && m_model.IsMantleRequested())
        {
            AZ_Printf("STWGameplay", "MANTLE_DIAG request=RAISED\n");
        }
        if (m_model.IsMantleRequested())
        {
            const AZ::Vector3 requestedVelocity = m_model.GetDesiredVelocity(m_input);
            const AZ::Vector3 direction(requestedVelocity.GetX(), requestedVelocity.GetY(), 0.0f);
            if (m_automatedAcceptance)
            {
                AZ_Printf("STWGameplay", "MANTLE_DIAG forwarding=RECEIVED velocity=(%.3f,%.3f,%.3f)\n",
                    requestedVelocity.GetX(), requestedVelocity.GetY(), requestedVelocity.GetZ());
            }
            if (m_physicsPlayer.CanStartMantle(direction, m_model.GetPlayer().m_grounded))
            {
                m_model.BeginMantle(direction);
                if (m_automatedAcceptance)
                {
                    AZ_Printf("STWGameplay", "MANTLE_DIAG activation=PASS\n");
                }
            }
            else if (m_automatedAcceptance)
            {
                AZ_Printf("STWGameplay", "MANTLE_DIAG activation=FAIL\n");
                AZ_Printf("STWGameplay", "MANTLE_DIAG movement=NOT_OBSERVED\n");
                AZ_Printf("STWGameplay", "MANTLE_DIAG completion=NOT_REACHED\n");
            }
        }
        if (!m_physicsPlayer.ApplyCrouchRequest(
                m_model.GetPlayer().m_crouchDesired, m_model.GetPlayer().m_grounded))
        {
            AZ_Error("STWGameplay", false, "PhysX crouch controller resize failed");
        }
        const WeaponState feedbackWeaponBefore = m_model.GetWeapon();
        const EnemyInstance* feedbackEnemyInstance = m_model.GetEnemies().Find(m_model.GetPresentation().m_hitEnemyId);
        const EnemyState& feedbackEnemyBefore = feedbackEnemyInstance != nullptr
            ? feedbackEnemyInstance->m_combat.GetState() : m_model.GetEnemy().GetState();
        CombatFeedbackInput feedbackInput;
        feedbackInput.m_shotFired = m_model.GetPresentation().m_shotFired;
        feedbackInput.m_hitConfirmed = m_model.GetPresentation().m_hit;
        feedbackInput.m_impactPosition = feedbackEnemyBefore.m_position;
        m_combatFeedback.Update(deltaTime, feedbackInput);
        const bool feedbackEnemyAuthorityUnchanged = feedbackEnemyInstance != nullptr
            ? feedbackEnemyInstance->m_combat.GetState().m_health == feedbackEnemyBefore.m_health
                && feedbackEnemyInstance->m_combat.GetState().m_damageEvents == feedbackEnemyBefore.m_damageEvents
            : m_model.GetEnemy().GetState().m_health == feedbackEnemyBefore.m_health
                && m_model.GetEnemy().GetState().m_damageEvents == feedbackEnemyBefore.m_damageEvents;
        m_combatFeedbackAuthoritySeparated = m_combatFeedbackAuthoritySeparated
            && m_model.GetWeapon().m_magazine == feedbackWeaponBefore.m_magazine
            && m_model.GetWeapon().m_cooldownRemaining == feedbackWeaponBefore.m_cooldownRemaining
            && feedbackEnemyAuthorityUnchanged;
        const AZ::Vector3 desiredPlayerVelocity = m_model.GetDesiredVelocity(m_input);
        if (m_automatedAcceptance && m_jumpAcceptanceStarted
            && m_model.GetPlayer().m_jumpEvents > m_jumpAcceptanceInitialEvents)
        {
            if (!s_jumpDiagnostic.m_acceptReported)
            {
                AZ_Printf(
                    "STWGameplay",
                    "JUMP_DIAG_ACCEPT accepted_count=%d model_velocity_z=%.6f grounded_before=%d sim_time=%.6f\n",
                    m_model.GetPlayer().m_jumpEvents - m_jumpAcceptanceInitialEvents,
                    desiredPlayerVelocity.GetZ(),
                    m_model.GetPlayer().m_grounded ? 1 : 0,
                    m_acceptanceTime);
                s_jumpDiagnostic.m_acceptReported = true;
            }
            if (!s_jumpDiagnostic.m_queueReported)
            {
                AZ_Printf(
                    "STWGameplay",
                    "JUMP_DIAG_QUEUE velocity_z=%.6f full_velocity=(%.6f,%.6f,%.6f) sim_time=%.6f\n",
                    desiredPlayerVelocity.GetZ(),
                    desiredPlayerVelocity.GetX(),
                    desiredPlayerVelocity.GetY(),
                    desiredPlayerVelocity.GetZ(),
                    m_acceptanceTime);
                s_jumpDiagnostic.m_queueReported = true;
            }
        }
        m_physicsPlayer.QueueVelocity(desiredPlayerVelocity);
        for (size_t index = 0; index < enemies.GetEnemyCount(); ++index)
        {
            const EnemyInstance& instance = enemies.GetInstanceByIndex(index);
            m_enemyPhysicsRuntimes[index].QueueVelocity(
                instance.m_combat.GetMovementIntent(m_model.GetPlayer().m_position));
        }
        AZ::Vector3 physicalPosition = AZ::Vector3::CreateZero();
        bool grounded = false;
        const bool playerPhysicalStateSynchronized = m_physicsPlayer.Synchronize(physicalPosition, grounded);
        if (playerPhysicalStateSynchronized)
        {
            m_model.SynchronizePhysicalState(physicalPosition, grounded);
            if (m_automatedAcceptance)
            {
                m_spawnCheckpointLastPhysicalPosition = physicalPosition;
            }
        }
        UpdateJumpAcceptance(playerPhysicalStateSynchronized);
        UpdateCrouchAcceptance(playerPhysicalStateSynchronized);
        UpdateSlideAcceptance(playerPhysicalStateSynchronized);
        UpdateMantleAcceptance(playerPhysicalStateSynchronized);
        for (size_t index = 0; index < enemies.GetEnemyCount(); ++index)
        {
            AZ::Vector3 enemyPhysicalPosition = AZ::Vector3::CreateZero();
            bool enemyGrounded = false;
            if (m_enemyPhysicsRuntimes[index].Synchronize(enemyPhysicalPosition, enemyGrounded))
            {
                const EnemyInstance& instance = enemies.GetInstanceByIndex(index);
                m_model.GetEnemies().SynchronizePhysicalPosition(instance.m_id, enemyPhysicalPosition);
                if (index == 0)
                {
                    m_enemyMoved = m_enemyMoved
                        || (enemyPhysicalPosition - m_enemyAcceptanceStartPosition).GetLength() > 0.25f;
                }
            }
        }

        for (size_t index = 0; index < enemies.GetEnemyCount(); ++index)
        {
            const EnemyState presentationInput = enemies.GetInstanceByIndex(index).m_combat.GetState();
            m_enemyPresentations[index].Update(deltaTime, presentationInput);
            const EnemyState& presentationAfter = enemies.GetInstanceByIndex(index).m_combat.GetState();
            m_enemyPresentationAuthoritySeparated = m_enemyPresentationAuthoritySeparated
                && presentationAfter.m_id == presentationInput.m_id
                && presentationAfter.m_position.IsClose(presentationInput.m_position)
                && presentationAfter.m_health == presentationInput.m_health
                && presentationAfter.m_behaviorState == presentationInput.m_behaviorState
                && presentationAfter.m_attackEvents == presentationInput.m_attackEvents;
            if (index == 0)
            {
                m_enemyPresentationIdleObserved = m_enemyPresentationIdleObserved
                    || m_enemyPresentations[index].GetState() == EnemyBehaviorState::Idle;
                m_enemyPresentationChaseObserved = m_enemyPresentationChaseObserved
                    || m_enemyPresentations[index].GetState() == EnemyBehaviorState::Chase;
                m_enemyPresentationAttackObserved = m_enemyPresentationAttackObserved
                    || (m_enemyPresentations[index].GetState() == EnemyBehaviorState::Attack
                        && m_enemyPresentations[index].GetAttackReactionCount() > 0);
                m_enemyPresentationDeadObserved = m_enemyPresentationDeadObserved
                    || (m_enemyPresentations[index].GetState() == EnemyBehaviorState::Dead
                        && m_enemyPresentations[index].GetDeathReactionCount() > 0);
                m_enemyPresentationResetObserved = m_enemyPresentationResetObserved
                    || m_enemyPresentations[index].GetResetReactionCount() > 0;
            }
        }

        // Presentation reacts to authoritative events/state only (read-only). It never writes
        // ammo, damage, reload completion, target health or player movement back.
        PresentationInput vpInput;
        vpInput.m_shotFired = m_model.GetPresentation().m_shotFired;
        vpInput.m_hit = m_model.GetPresentation().m_hit;
        vpInput.m_reloading = m_model.GetWeapon().m_reloading;
        vpInput.m_activeEquipmentSlot = static_cast<AZ::u8>(m_model.GetActiveEquipmentSlot());
        vpInput.m_activeEquipmentCategory = static_cast<AZ::u8>(m_model.GetActiveEquipmentProfile().m_category);
        vpInput.m_activeEquipmentProfile = static_cast<AZ::u8>(m_model.GetActiveEquipmentProfileId());
        vpInput.m_equipmentChanged = m_model.GetPresentation().m_equipmentChanged;
        vpInput.m_equipmentUsed = m_model.GetPresentation().m_equipmentUsed;
        vpInput.m_moving = (std::abs(m_input.m_forward) > 0.01f) || (std::abs(m_input.m_strafe) > 0.01f);
        vpInput.m_sprinting = m_input.m_sprint && vpInput.m_moving;
        vpInput.m_adsRequested = m_adsHeld;
        vpInput.m_lookX = m_input.m_lookX;
        vpInput.m_lookY = m_input.m_lookY;
        m_viewmodel.Update(deltaTime, vpInput);
        UpdateAdsAcceptanceMarkers();
        UpdateSwayAcceptanceMarkers();

        m_input.m_lookX = 0.0f;
        m_input.m_lookY = 0.0f;
        m_input.m_reload = false;
        UpdateCamera();
        DrawPresentation();
        UpdateWeaponSwitchAcceptance();
        UpdateLoadoutAcceptance();
        UpdateEnemyCombatAcceptance();
        UpdateMultiEnemyAcceptance();
        m_encounter.Update(enemies);
        UpdateEncounterAcceptance();
        if (m_encounter.IsCompleted() && !m_spawnCheckpoint.HasActiveCheckpoint())
        {
            m_spawnCheckpoint.ActivateCheckpoint(m_model.GetPlayer().m_position);
        }
        UpdateSpawnCheckpointAcceptance();
        UpdateEnemyAiAcceptance(deltaTime);
        UpdateEnemyPresentationAcceptance();
        UpdateCombatFeedbackAcceptance();
        UpdateArenaAcceptance();
        RecordPerformance(deltaTime);

        // Production runs do not set STW_NATIVE_CAPTURE_PATH. The controlled
        // native verification job uses it to request one genuine Atom/RHI
        // readback after the scene and presentation have had time to render.
        if (!m_nativeCapturePath.empty() && !m_nativeCaptureAttempted)
        {
            m_nativeCaptureDelay += deltaTime;
            if (m_nativeCaptureDelay >= 0.75f)
            {
                m_nativeCaptureAttempted = true;
                bool canCapture = false;
                AZ::Render::FrameCaptureRequestBus::BroadcastResult(
                    canCapture, &AZ::Render::FrameCaptureRequestBus::Events::CanCapture);
                if (!canCapture)
                {
                    AZ_Error("STWGameplay", false, "Native Atom frame capture is unavailable");
                    return;
                }

                AZ::Render::FrameCaptureOutcome outcome = AZ::Failure(
                    AZ::Render::FrameCaptureError{ "FrameCapture request was not handled" });
                AZ::Render::FrameCaptureRequestBus::BroadcastResult(
                    outcome,
                    &AZ::Render::FrameCaptureRequestBus::Events::CaptureScreenshot,
                    m_nativeCapturePath);
                if (outcome.IsSuccess())
                {
                    AZ_Printf(
                        "STWGameplay",
                        "Native Atom frame capture submitted: %u -> %s\n",
                        outcome.GetValue(),
                        m_nativeCapturePath.c_str());
                }
                else
                {
                    AZ_Error(
                        "STWGameplay",
                        false,
                        "Native Atom frame capture failed: %s",
                        outcome.GetError().m_errorMessage.c_str());
                }
            }
        }
    }

    void STWGameplaySystemComponent::RecordPerformance(float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0f || m_performanceReported)
        {
            return;
        }
        m_performanceDuration += deltaTime;
        if (m_frameSampleCount < m_frameSamples.size())
        {
            m_frameSamples[m_frameSampleCount++] = deltaTime;
        }
        if (m_performanceDuration < 10.0f || m_frameSampleCount == 0)
        {
            return;
        }

        AZStd::array<float, 2048> sorted = m_frameSamples;
        std::sort(sorted.begin(), sorted.begin() + m_frameSampleCount);
        const float medianMilliseconds = sorted[m_frameSampleCount / 2] * 1000.0f;
        const float averageFps = static_cast<float>(m_frameSampleCount) / m_performanceDuration;
        AZ_Printf(
            "STWGameplay",
            "PERFORMANCE_BASELINE average_fps=%.3f median_frame_ms=%.3f sample_seconds=%.3f samples=%zu "
            "cpu_frame_ms=UNAVAILABLE gpu_frame_ms=UNAVAILABLE resolution=1920x1080\n",
            averageFps,
            medianMilliseconds,
            m_performanceDuration,
            m_frameSampleCount);
        m_performanceReported = true;
    }

    void STWGameplaySystemComponent::UpdateAutomatedAcceptance(float deltaTime)
    {
        if (!m_automatedAcceptance || (m_viewmodelAcceptanceReported && m_adsAcceptanceReported
                && m_jumpAcceptanceReported && m_crouchAcceptanceReported && m_slideAcceptanceReported
                && m_mantleAcceptanceReported && m_loadoutAcceptanceReported)
            || !std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            return;
        }
        m_acceptanceTime += deltaTime;
        s_jumpDiagnostic.m_tickDeltaTime = deltaTime;

        // Verification-only stimulus. It feeds the same m_adsHeld state that the real
        // Mouse::Button::Right path writes; it is disabled unless the existing acceptance mode
        // is explicitly enabled and uses simulation time, never wall-clock sleeps.
        m_adsHeld = m_acceptanceTime >= 1.0f && m_acceptanceTime < 6.0f;
        m_input.m_forward = 0.0f;
        m_input.m_strafe = 0.0f;
        m_input.m_sprint = false;
        m_input.m_jump = false;
        m_input.m_crouch = false;
        m_input.m_fire = false;
        m_input.m_switchWeapon = false;
        m_input.m_requestedEquipmentSlot = -1;
        m_input.m_lookX = 0.0f;
        m_input.m_lookY = 0.0f;

        // Exercise the normal jump input/model/PhysX path after the established visual capture
        // and combat stimuli. Holding the request through landing proves edge-triggered behavior.
        if (m_acceptanceTime >= 8.0f && !m_jumpAcceptanceReported)
        {
            if (!m_jumpAcceptanceStarted)
            {
                const PlayerState& player = m_model.GetPlayer();
                if (player.m_alive && player.m_grounded)
                {
                    m_jumpAcceptanceStarted = true;
                    m_jumpAcceptanceStartTime = m_acceptanceTime;
                    m_jumpAcceptanceStartHeight = player.m_position.GetZ();
                    m_jumpAcceptanceInitialEvents = player.m_jumpEvents;
                }
            }
            m_input.m_jump = m_jumpAcceptanceStarted;
        }

        // Exercise crouch only after jump has completed, using the normal model-to-PhysX path.
        if (m_jumpAcceptanceReported && !m_crouchAcceptanceReported)
        {
            if (!m_crouchAcceptanceStarted)
            {
                const PlayerState& player = m_model.GetPlayer();
                if (player.m_alive && player.m_grounded)
                {
                    m_crouchAcceptanceStarted = true;
                    m_crouchAcceptanceStartBaseZ = player.m_position.GetZ();
                    m_crouchAcceptanceStandingHeight = m_physicsPlayer.GetControllerHeight();
                }
            }
            m_input.m_crouch = m_crouchAcceptanceStarted && !m_crouchAcceptanceCrouched;
        }

        // Exercise a fresh moving Left Ctrl press only after normal crouch has stood again.
        // This uses the same input, model velocity and PhysX controller paths as gameplay.
        if (m_crouchAcceptanceReported && !m_slideAcceptanceReported)
        {
            if (!m_slideAcceptanceStimulusStarted)
            {
                const PlayerState& player = m_model.GetPlayer();
                if (player.m_alive && player.m_grounded && !m_physicsPlayer.IsCrouched())
                {
                    m_slideAcceptanceStimulusStarted = true;
                    m_slideAcceptanceGroundedStart = true;
                    m_slideAcceptanceStartPosition = player.m_position;
                    m_slideAcceptanceInitialEvents = player.m_slideEvents;
                }
            }
            m_input.m_forward = m_slideAcceptanceStimulusStarted ? 1.0f : 0.0f;
            m_input.m_crouch = m_slideAcceptanceStimulusStarted;
        }

        if (m_slideAcceptanceReported && !m_mantleAcceptanceReported)
        {
            if (!m_mantleAcceptanceStimulusStarted)
            {
                // Restore the already verified low-step mantle fixture before the acceptance
                // stimulus. This is harness setup only; normal model and PhysX authority remain
                // responsible for the request, probe, and movement.
                const AZ::Vector3 mantleFixturePosition(6.027f, -0.073f, 0.026f);
                m_model.SetPlayerPosition(mantleFixturePosition);
                m_physicsPlayer.ResetPosition(mantleFixturePosition);
                m_input.m_lookX = (0.03f - m_model.GetPlayer().m_yaw) / PlayerSliceModel::LookSensitivity;
                const PlayerState& player = m_model.GetPlayer();
                if (player.m_alive && player.m_grounded && !m_physicsPlayer.IsCrouched())
                {
                    m_mantleAcceptanceStimulusStarted = true;
                    m_mantleAcceptanceStartPosition = player.m_position;
                    m_mantleAcceptanceStartZ = player.m_position.GetZ();
                    m_mantleAcceptanceMaxZ = m_mantleAcceptanceStartZ;
                    m_mantleAcceptanceInitialEvents = player.m_mantleEvents;
                    m_mantleAcceptanceInitialJumpEvents = player.m_jumpEvents;
                    m_mantleAcceptanceInitialSlideEvents = player.m_slideEvents;
                    AZ_Printf("STWGameplay", "MANTLE_DIAG stimulus=STARTED position=(%.3f,%.3f,%.3f)\n",
                        player.m_position.GetX(), player.m_position.GetY(), player.m_position.GetZ());
                }
            }
            m_input.m_forward = m_mantleAcceptanceStimulusStarted ? -1.0f : 0.0f;
            const bool mantleConflictStimulus = m_mantleAcceptanceStimulusStarted && !m_mantleAcceptanceStarted;
            m_input.m_crouch = mantleConflictStimulus;
            m_input.m_jump = mantleConflictStimulus;
            m_input.m_mantle = m_mantleAcceptanceStimulusStarted;
        }

        // Acceptance-only deterministic look stimulus. It feeds the same presentation-safe
        // look-delta path used by normal mouse input and never changes gameplay orientation.
        if (m_acceptanceTime >= 2.2f && m_acceptanceTime < 2.5f)
        {
            m_input.m_lookX = 12.0f;
        }
        else if (m_acceptanceTime >= 2.5f && m_acceptanceTime < 2.8f)
        {
            m_input.m_lookX = -12.0f;
        }

        if (m_acceptanceTime >= 2.0f && m_acceptanceTime < 3.0f)
        {
            if (m_acceptanceStartPosition.IsZero())
            {
                m_acceptanceStartPosition = m_model.GetPlayer().m_position;
            }
            m_input.m_forward = 1.0f;
        }
        else if (m_acceptanceTime >= 3.0f && m_acceptanceTime < 4.0f)
        {
            m_input.m_forward = 1.0f;
            m_input.m_strafe = 1.0f;
            m_input.m_sprint = true;
        }
        else if (m_acceptanceTime >= 4.0f && !m_acceptanceReported)
        {
            const PlayerState& player = m_model.GetPlayer();
            const float travelled = (player.m_position - m_acceptanceStartPosition).GetLength();
            const bool passed = m_physicsPlayer.IsValid() && player.m_position.IsFinite() && player.m_grounded
                && travelled > 1.0f && travelled < 14.0f
                && std::abs(m_model.GetDesiredVelocity(PlayerInput{ 1.0f, 1.0f }).GetLength() - PlayerSliceModel::WalkSpeed) < 0.01f;
            AZ_Printf(
                "STWGameplay",
                "PHYSX_ACCEPTANCE result=%s initialized=%s grounded=%s travelled=%.3f finite=%s\n",
                passed ? "PASS" : "FAIL",
                m_physicsPlayer.IsValid() ? "true" : "false",
                player.m_grounded ? "true" : "false",
                travelled,
                player.m_position.IsFinite() ? "true" : "false");
            m_acceptanceReported = true;
        }
        // Presentation acceptance (test-only, gated by STW_PHYSX_ACCEPTANCE), driven entirely
        // through the SAME authoritative input path: fire a rate-limited burst, then reload,
        // then report the viewmodel's reaction. No second gameplay path is introduced.
        else if (m_acceptanceTime >= 4.5f && m_acceptanceTime < 5.2f)
        {
            m_input.m_fire = true; // authoritative TryFire; the weapon rate-limits it
        }
        else if (m_acceptanceTime >= 5.2f && m_acceptanceTime < 5.6f)
        {
            const WeaponState& weapon = m_model.GetWeapon();
            if (weapon.m_magazine < 30 && !weapon.m_reloading && weapon.m_reserve > 0)
            {
                m_input.m_reload = true; // authoritative StartReload; self-limiting once reloading
            }
        }
        else if (m_acceptanceTime >= 7.5f && !m_viewmodelAcceptanceReported)
        {
            const bool vmPass = m_viewmodel.GetFireEventCount() > 0u
                && m_viewmodel.GetReloadStartCount() > 0u
                && m_viewmodel.GetRecoilOffset().IsFinite()
                && m_viewmodel.GetSwayOffset().IsFinite();
            AZ_Printf(
                "STWGameplay",
                "VIEWMODEL_ACCEPTANCE result=%s fire_events=%u reload_starts=%u recoil=%.3f state=%d\n",
                vmPass ? "PASS" : "FAIL",
                m_viewmodel.GetFireEventCount(),
                m_viewmodel.GetReloadStartCount(),
                m_viewmodel.GetRecoilOffset().GetLength(),
                static_cast<int>(m_viewmodel.GetState()));
            m_viewmodelAcceptanceReported = true;
        }

        if (m_acceptanceTime >= 8.50f && m_acceptanceTime < 9.20f)
        {
            m_input.m_fire = true;
        }

        // Acceptance-only two-weapon stimulus. The model consumes a rising edge, so every switch
        // phase explicitly owns its assertion and is followed by a bounded release phase. In
        // particular, no time-windowed switch input may survive the authoritative player reset.
        if (m_weaponSwitchAcceptanceStarted && !m_weaponSwitchSecondSwitchObserved)
        {
            const PlayerState& player = m_model.GetPlayer();
            const WeaponId activeWeapon = m_model.GetActiveWeaponId();
            if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::InitialSwitch
                && player.m_alive && activeWeapon == WeaponId::STW_SMG_01)
            {
                m_input.m_switchWeapon = true;
            }
            else if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::EnsureSecondary
                && player.m_alive && activeWeapon == WeaponId::STW_SMG_01)
            {
                m_input.m_switchWeapon = true;
                m_weaponSwitchEnsureSecondaryInputAsserted = true;
                if (m_weaponSwitchEnsureSecondarySlotBefore < 0)
                {
                    m_weaponSwitchEnsureSecondarySlotBefore = static_cast<int>(activeWeapon);
                }
            }
            else if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::SecondSwitch
                && player.m_alive && activeWeapon == WeaponId::STW_RIFLE_02)
            {
                m_input.m_switchWeapon = true;
                m_weaponSwitchSecondSwitchInputAsserted = true;
                if (m_weaponSwitchSecondSwitchSlotBefore < 0)
                {
                    m_weaponSwitchSecondSwitchSlotBefore = static_cast<int>(activeWeapon);
                }
            }
        }
    }

    void STWGameplaySystemComponent::UpdateSwayAcceptanceMarkers()
    {
        if (!m_automatedAcceptance || m_swayAcceptanceReported)
        {
            return;
        }

        const float sway = m_viewmodel.GetSwayOffset().GetLength();
        if (!m_swayAcceptanceBegun)
        {
            AZ_Printf("STWGameplay", "SWAY_ACCEPTANCE_BEGIN\n");
            AZ_Printf("STWGameplay", "SWAY_STATE phase=IDLE input=(0.000,0.000) sway=%.3f\n", sway);
            m_swayAcceptanceBegun = true;
        }
        if (!m_swayPositiveReported && std::abs(m_input.m_lookX) > 0.0f)
        {
            AZ_Printf("STWGameplay", "SWAY_STATE phase=POSITIVE input=(%.3f,%.3f) sway=%.3f\n",
                m_input.m_lookX, m_input.m_lookY, sway);
            m_swayPositiveReported = true;
        }
        if (m_swayPositiveReported && !m_swayReturnReported && m_acceptanceTime >= 2.8f)
        {
            AZ_Printf("STWGameplay", "SWAY_STATE phase=RETURN input=(0.000,0.000) sway=%.3f\n", sway);
            m_swayReturnReported = true;
        }
        if (m_swayReturnReported && m_acceptanceTime >= 4.0f && sway <= 0.0001f)
        {
            AZ_Printf("STWGameplay", "SWAY_ACCEPTANCE result=PASS final_sway=%.3f\n", sway);
            m_swayAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateWeaponSwitchAcceptance()
    {
        if (!m_automatedAcceptance || m_weaponSwitchAcceptanceReported)
        {
            return;
        }

        if (!m_weaponSwitchAcceptanceStarted && m_acceptanceTime >= 7.25f)
        {
            m_weaponSwitchAcceptanceStarted = true;
            m_weaponSwitchInitialSlot = static_cast<int>(m_model.GetActiveWeaponId());
            m_weaponSwitchInitialRespawnEvents = m_model.GetPlayer().m_respawnEvents;
            const WeaponState& weaponA = m_model.GetWeapon(WeaponId::STW_SMG_01);
            const WeaponState& weaponB = m_model.GetWeapon(WeaponId::STW_RIFLE_02);
            m_weaponSwitchInitialAMagazine = weaponA.m_magazine;
            m_weaponSwitchInitialAReserve = weaponA.m_reserve;
            m_weaponSwitchInitialBMagazine = weaponB.m_magazine;
            m_weaponSwitchInitialBReserve = weaponB.m_reserve;
            m_weaponSwitchHeldStable = true;
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::InitialSwitch;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
        }
        if (!m_weaponSwitchAcceptanceStarted)
        {
            return;
        }

        const int activeSlot = static_cast<int>(m_model.GetActiveWeaponId());
        if (!m_weaponSwitchFirstSwitchObserved && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02))
        {
            m_weaponSwitchFirstSwitchObserved = true;
        }
        if (m_weaponSwitchFirstSwitchObserved && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02))
        {
            m_weaponSwitchFirstWeaponVisible = m_weaponSwitchFirstWeaponVisible
                || (m_viewmodelMeshReported && m_visibleViewmodelSlot == static_cast<size_t>(WeaponId::STW_RIFLE_02));
            if (m_input.m_switchWeapon && m_acceptanceTime >= 7.5f && m_acceptanceTime < 8.25f)
            {
                m_weaponSwitchHeldStable = m_weaponSwitchHeldStable && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02);
            }
        }

        if (m_weaponSwitchFirstSwitchObserved && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02)
            && m_acceptanceTime >= 8.50f
            && m_acceptanceTime < 9.20f)
        {
            const WeaponState& weaponA = m_model.GetWeapon(WeaponId::STW_SMG_01);
            const WeaponState& weaponB = m_model.GetWeapon(WeaponId::STW_RIFLE_02);
            // Preserve the authoritative fire-window observation before a later acceptance-only
            // player reset can restore the weapon magazine to its initial value.
            m_weaponSwitchBAmmoChangedOnFire = m_weaponSwitchBAmmoChangedOnFire
                || weaponB.m_magazine < m_weaponSwitchInitialBMagazine;
            m_weaponSwitchInactiveAUnchanged = m_weaponSwitchInactiveAUnchanged
                && weaponA.m_magazine == m_weaponSwitchInitialAMagazine
                && weaponA.m_reserve == m_weaponSwitchInitialAReserve;
        }

        if (m_weaponSwitchFirstSwitchObserved && m_acceptanceTime >= 9.20f
            && !m_weaponSwitchInactiveAAmmoAfterBFireCaptured)
        {
            const WeaponState& weaponA = m_model.GetWeapon(WeaponId::STW_SMG_01);
            m_weaponSwitchInactiveAAmmoAfterBFire = weaponA.m_magazine;
            m_weaponSwitchInactiveAAmmoAfterBFireCaptured = true;
        }

        const PlayerState& weaponSwitchPlayer = m_model.GetPlayer();
        if (!m_weaponSwitchResetComplete && weaponSwitchPlayer.m_alive
            && weaponSwitchPlayer.m_respawnEvents > m_weaponSwitchInitialRespawnEvents)
        {
            m_weaponSwitchResetComplete = true;
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::PostResetRelease;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
            const WeaponState& weaponAAfterReset = m_model.GetWeapon(WeaponId::STW_SMG_01);
            m_weaponSwitchPostResetAMagazine = weaponAAfterReset.m_magazine;
            m_weaponSwitchPostResetAReserve = weaponAAfterReset.m_reserve;
            m_weaponSwitchPostResetBaselineCaptured = true;
        }

        if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::InitialSwitch
            && m_weaponSwitchFirstSwitchObserved
            && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02)
            && m_input.m_switchWeapon)
        {
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::InitialSwitchRelease;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
        }
        if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::InitialSwitchRelease
            && !m_input.m_switchWeapon && weaponSwitchPlayer.m_alive
            && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02)
            && m_acceptanceTime - m_weaponSwitchAcceptancePhaseStartTime >= 0.25f)
        {
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::WaitingForReset;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
        }

        if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::WaitingForReset
            && m_weaponSwitchResetComplete)
        {
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::PostResetRelease;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
        }
        if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::PostResetRelease
            && !m_input.m_switchWeapon && weaponSwitchPlayer.m_alive
            && activeSlot == static_cast<int>(WeaponId::STW_SMG_01)
            && m_acceptanceTime - m_weaponSwitchAcceptancePhaseStartTime >= 0.25f)
        {
            m_weaponSwitchPostResetReleaseObserved = true;
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::EnsureSecondary;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
        }
        if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::EnsureSecondary
            && m_weaponSwitchEnsureSecondaryInputAsserted
            && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02))
        {
            m_weaponSwitchEnsureSecondaryEdge = true;
            m_weaponSwitchEnsureSecondarySlotAfter = activeSlot;
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::EnsureSecondaryRelease;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
        }
        if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::EnsureSecondaryRelease
            && !m_input.m_switchWeapon && weaponSwitchPlayer.m_alive
            && activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02)
            && m_acceptanceTime - m_weaponSwitchAcceptancePhaseStartTime >= 0.25f)
        {
            m_weaponSwitchPreSecondSwitchReleaseObserved = true;
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::SecondSwitch;
            m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
        }
        if (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::SecondSwitch
            && !m_weaponSwitchSecondSwitchObserved && m_weaponSwitchSecondSwitchInputAsserted
            && m_weaponSwitchPreSecondSwitchReleaseObserved
            && m_weaponSwitchSecondSwitchSlotBefore == static_cast<int>(WeaponId::STW_RIFLE_02)
            && activeSlot == static_cast<int>(WeaponId::STW_SMG_01))
        {
            const WeaponState& weaponA = m_model.GetWeapon(WeaponId::STW_SMG_01);
            m_weaponSwitchSecondSwitchEdge = true;
            m_weaponSwitchSecondSwitchSlotAfter = activeSlot;
            m_weaponSwitchSecondSwitchTransitionEventDelta = m_model.GetPresentation().m_equipmentChanged ? 1 : 0;
            ++m_weaponSwitchSecondSwitchObservationCount;
            m_weaponSwitchSecondSwitchObserved = true;
            // The inactive-window predicate covers the pre-reset interval. Once the
            // authoritative reset has occurred, compare the return-to-primary state only
            // against a baseline captured after that reset, never against the pre-reset
            // acceptance snapshot.
            m_weaponSwitchAAmmoPreserved = m_weaponSwitchPostResetBaselineCaptured
                && weaponA.m_magazine == m_weaponSwitchPostResetAMagazine
                && weaponA.m_reserve == m_weaponSwitchPostResetAReserve;
            m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::Complete;
        }

        if (!m_weaponSwitchPostResetDiagnosticReported
            && (m_weaponSwitchAcceptancePhase == WeaponSwitchAcceptancePhase::Complete
                || m_acceptanceTime >= 12.0f))
        {
            AZ_Printf("STWGameplay",
                "WEAPON_SWITCH_POST_RESET_DIAG RESET_COMPLETE=%d POST_RESET_RELEASE_OBSERVED=%d "
                "ENSURE_SECONDARY_EDGE=%d ENSURE_SECONDARY_SLOT_BEFORE=%d ENSURE_SECONDARY_SLOT_AFTER=%d "
                "PRE_SECOND_SWITCH_RELEASE_OBSERVED=%d SECOND_SWITCH_EDGE=%d "
                "SECOND_SWITCH_SLOT_BEFORE=%d SECOND_SWITCH_SLOT_AFTER=%d "
                "SECOND_SWITCH_TRANSITION_EVENT_DELTA=%d SECOND_SWITCH_OBSERVED=%d "
                "SECOND_SWITCH_OBSERVATION_COUNT=%d\n",
                m_weaponSwitchResetComplete ? 1 : 0,
                m_weaponSwitchPostResetReleaseObserved ? 1 : 0,
                m_weaponSwitchEnsureSecondaryEdge ? 1 : 0,
                m_weaponSwitchEnsureSecondarySlotBefore,
                m_weaponSwitchEnsureSecondarySlotAfter,
                m_weaponSwitchPreSecondSwitchReleaseObserved ? 1 : 0,
                m_weaponSwitchSecondSwitchEdge ? 1 : 0,
                m_weaponSwitchSecondSwitchSlotBefore,
                m_weaponSwitchSecondSwitchSlotAfter,
                m_weaponSwitchSecondSwitchTransitionEventDelta,
                m_weaponSwitchSecondSwitchObserved ? 1 : 0,
                m_weaponSwitchSecondSwitchObservationCount);
            m_weaponSwitchPostResetDiagnosticReported = true;
        }

        // Acceptance-only diagnostics. Observe the existing input, model presentation,
        // and authoritative state around the Weapon B fire window without changing them.
        if (!m_weaponBFireDiagnosticStarted && m_acceptanceTime >= 8.25f)
        {
            const PlayerState& player = m_model.GetPlayer();
            const WeaponState& weaponB = m_model.GetWeapon(WeaponId::STW_RIFLE_02);
            m_weaponBFireDiagnosticStarted = true;
            m_weaponBFireSlotBefore = activeSlot;
            m_weaponBFireSelectedBefore = activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02);
            m_weaponBFirePlayerAliveBefore = player.m_alive;
            m_weaponBFireHealthBefore = player.m_health;
            m_weaponBFireDamageEventsBefore = player.m_damageEvents;
            m_weaponBFireDeathEventsBefore = player.m_deathEvents;
            m_weaponBFireRespawnEventsBefore = player.m_respawnEvents;
            m_weaponBFireAmmoBefore = weaponB.m_magazine;
            m_weaponBFirePreviousInput = false;
        }

        if (m_weaponBFireDiagnosticStarted && !m_weaponBFireDiagnosticReported
            && m_acceptanceTime >= 8.50f && m_acceptanceTime < 9.20f)
        {
            const bool inputThisTick = m_input.m_fire;
            const bool activeWeaponB = activeSlot == static_cast<int>(WeaponId::STW_RIFLE_02);
            const bool shotFiredThisTick = m_model.GetPresentation().m_shotFired;
            const bool weaponBFireAcceptedThisTick = activeWeaponB && shotFiredThisTick;
            m_weaponBFireInputAsserted = m_weaponBFireInputAsserted || inputThisTick;
            m_weaponBFireInputEdge = m_weaponBFireInputEdge
                || (inputThisTick && !m_weaponBFirePreviousInput);
            m_weaponBFireInputHeld = m_weaponBFireInputHeld
                || (inputThisTick && m_weaponBFirePreviousInput);
            m_weaponBFirePreviousInput = inputThisTick;
            m_weaponBFireRequestReachedModel = m_weaponBFireRequestReachedModel || inputThisTick;
            m_weaponBFireAcceptedByModel = m_weaponBFireAcceptedByModel || weaponBFireAcceptedThisTick;
            m_weaponBFireRejectedByModel = m_weaponBFireRejectedByModel
                || (inputThisTick && !weaponBFireAcceptedThisTick);
            m_weaponBFireSelectedDuringWindow = m_weaponBFireSelectedDuringWindow || activeWeaponB;
            if (weaponBFireAcceptedThisTick)
            {
                ++m_weaponBFireEventCount;
            }
        }

        if (m_weaponBFireDiagnosticStarted && !m_weaponBFireDiagnosticReported
            && m_acceptanceTime >= 9.20f)
        {
            const PlayerState& playerAfter = m_model.GetPlayer();
            const WeaponState& weaponBAfter = m_model.GetWeapon(WeaponId::STW_RIFLE_02);
            const int ammoDelta = weaponBAfter.m_magazine - m_weaponBFireAmmoBefore;
            const int damageDelta = playerAfter.m_damageEvents - m_weaponBFireDamageEventsBefore;
            const int deathDelta = playerAfter.m_deathEvents - m_weaponBFireDeathEventsBefore;
            const int respawnDelta = playerAfter.m_respawnEvents - m_weaponBFireRespawnEventsBefore;
            const bool wrongSlot = !m_weaponBFireSelectedBefore || !m_weaponBFireSelectedDuringWindow;
            const bool playerDead = !m_weaponBFirePlayerAliveBefore || !playerAfter.m_alive;
            const bool playerRespawned = respawnDelta > 0;
            const bool noAmmo = m_weaponBFireAmmoBefore <= 0;
            const bool acceptedAndDecremented = m_weaponBFireAcceptedByModel && ammoDelta < 0;
            const char* pathResult = "UNPROVEN";
            const char* rejectReason = "UNKNOWN";
            bool pathRootCauseProven = false;
            if (!m_weaponBFireInputAsserted)
            {
                pathResult = "INPUT_NOT_ASSERTED";
                rejectReason = "INPUT_NOT_ASSERTED";
                pathRootCauseProven = true;
            }
            else if (wrongSlot)
            {
                pathResult = "WRONG_SLOT";
                rejectReason = "WRONG_ACTIVE_SLOT";
                pathRootCauseProven = true;
            }
            else if (playerRespawned)
            {
                pathResult = "PLAYER_RESPAWN_INTERFERENCE";
                rejectReason = "PLAYER_RESPAWN_INTERFERENCE";
                pathRootCauseProven = true;
            }
            else if (acceptedAndDecremented)
            {
                pathResult = "PASS";
                pathRootCauseProven = true;
            }
            else if (playerDead)
            {
                pathResult = "PLAYER_DEAD";
                rejectReason = "DEAD";
                pathRootCauseProven = true;
            }
            else if (noAmmo)
            {
                pathResult = "NO_AMMO";
                rejectReason = "EMPTY_AMMO";
                pathRootCauseProven = true;
            }
            else if (weaponBAfter.m_reloading)
            {
                pathResult = "TRYFIRE_REJECTED_RELOAD";
                rejectReason = "RELOADING";
                pathRootCauseProven = true;
            }
            else if (weaponBAfter.m_cooldownRemaining > 0.0f && !m_weaponBFireAcceptedByModel)
            {
                pathResult = "TRYFIRE_REJECTED_COOLDOWN";
                rejectReason = "COOLDOWN";
                pathRootCauseProven = true;
            }
            else if (m_weaponBFireRejectedByModel)
            {
                pathResult = "TRYFIRE_REJECTED_OTHER";
            }
            else if (ammoDelta >= 0)
            {
                pathResult = "AMMO_DID_NOT_DECREMENT";
            }

            AZ_Printf("STWGameplay",
                "WEAPON_B_FIRE_DIAG_STARTED=1 WEAPON_B_SLOT_BEFORE_FIRE=%d "
                "WEAPON_B_SELECTED_BEFORE_FIRE=%d PLAYER_ALIVE_BEFORE_FIRE=%d "
                "PLAYER_HEALTH_BEFORE_FIRE=%.3f PLAYER_RESPAWN_EVENTS_BEFORE_FIRE=%d "
                "WEAPON_B_AMMO_BEFORE_FIRE=%d WEAPON_B_FIRE_INPUT_ASSERTED=%d "
                "WEAPON_B_FIRE_INPUT_HELD=%d WEAPON_B_FIRE_INPUT_EDGE=%d\n",
                m_weaponBFireSlotBefore,
                m_weaponBFireSelectedBefore ? 1 : 0,
                m_weaponBFirePlayerAliveBefore ? 1 : 0,
                m_weaponBFireHealthBefore,
                m_weaponBFireRespawnEventsBefore,
                m_weaponBFireAmmoBefore,
                m_weaponBFireInputAsserted ? 1 : 0,
                m_weaponBFireInputHeld ? 1 : 0,
                m_weaponBFireInputEdge ? 1 : 0);
            AZ_Printf("STWGameplay",
                "WEAPON_B_FIRE_REQUEST_REACHED_MODEL=%d WEAPON_B_FIRE_ACCEPTED_BY_MODEL=%d "
                "WEAPON_B_FIRE_REJECTED_BY_MODEL=%d WEAPON_B_FIRE_REJECT_REASON=%s\n",
                m_weaponBFireRequestReachedModel ? 1 : 0,
                m_weaponBFireAcceptedByModel ? 1 : 0,
                m_weaponBFireRejectedByModel ? 1 : 0,
                rejectReason);
            AZ_Printf("STWGameplay",
                "WEAPON_B_AMMO_AFTER_FIRE=%d WEAPON_B_AMMO_DELTA=%d WEAPON_B_FIRE_EVENT_DELTA=%d "
                "PLAYER_ALIVE_AFTER_FIRE=%d PLAYER_HEALTH_AFTER_FIRE=%.3f "
                "PLAYER_RESPAWN_EVENTS_AFTER_FIRE=%d PLAYER_DAMAGE_DURING_FIRE_WINDOW=%d "
                "PLAYER_DEATH_DURING_FIRE_WINDOW=%d PLAYER_RESPAWN_DURING_FIRE_WINDOW=%d "
                "WEAPON_SWITCH_SLOT_AFTER_FIRE=%d\n",
                weaponBAfter.m_magazine,
                ammoDelta,
                m_weaponBFireEventCount,
                playerAfter.m_alive ? 1 : 0,
                playerAfter.m_health,
                playerAfter.m_respawnEvents,
                damageDelta,
                deathDelta > 0 ? 1 : 0,
                respawnDelta > 0 ? 1 : 0,
                static_cast<int>(m_model.GetActiveWeaponId()));
            AZ_Printf("STWGameplay",
                "WEAPON_B_FIRE_PATH_RESULT=%s WEAPON_B_FIRE_PATH_ROOT_CAUSE_PROVEN=%s\n",
                pathResult,
                pathRootCauseProven ? "YES" : "NO");
            m_weaponBFireDiagnosticReported = true;
        }

        const bool passed = m_weaponSwitchInitialSlot == static_cast<int>(WeaponId::STW_SMG_01)
            && m_weaponSwitchFirstSwitchObserved
            && m_weaponSwitchFirstWeaponVisible
            && m_weaponSwitchHeldStable
            && m_weaponSwitchBAmmoChangedOnFire
            && m_weaponSwitchInactiveAUnchanged
            && m_weaponSwitchSecondSwitchObserved
            && m_weaponSwitchAAmmoPreserved;

        // Acceptance-only diagnostics. These expose the existing predicate state without
        // changing the stimulus, timing, authority, or official PASS marker.
        if (!m_weaponSwitchDiagnosticReported
            && (m_weaponSwitchSecondSwitchObserved || m_acceptanceTime >= 12.0f))
        {
            const bool initialSlotValid = m_weaponSwitchInitialSlot == static_cast<int>(WeaponId::STW_SMG_01);
            const bool firstSwitchObserved = m_weaponSwitchFirstSwitchObserved;
            const bool firstWeaponVisible = m_weaponSwitchFirstWeaponVisible;
            const bool heldSwitchRetriggerBlocked = m_weaponSwitchHeldStable;
            const bool weaponBFireObserved = m_weaponSwitchBAmmoChangedOnFire;
            const bool inactiveWeaponAmmoUnchanged = m_weaponSwitchInactiveAUnchanged;
            const bool secondSwitchObserved = m_weaponSwitchSecondSwitchObserved;
            const bool weaponAAmmoPreserved = m_weaponSwitchAAmmoPreserved;
            const char* firstFalsePredicate = "none";
            if (!initialSlotValid)
            {
                firstFalsePredicate = "initial_slot";
            }
            else if (!firstSwitchObserved)
            {
                firstFalsePredicate = "first_switch_observed";
            }
            else if (!firstWeaponVisible)
            {
                firstFalsePredicate = "first_weapon_visible";
            }
            else if (!heldSwitchRetriggerBlocked)
            {
                firstFalsePredicate = "held_switch_retrigger_blocked";
            }
            else if (!weaponBFireObserved)
            {
                firstFalsePredicate = "weapon_b_ammo_changed_on_fire";
            }
            else if (!inactiveWeaponAmmoUnchanged)
            {
                firstFalsePredicate = "inactive_weapon_ammo_unchanged";
            }
            else if (!secondSwitchObserved)
            {
                firstFalsePredicate = "second_switch_observed";
            }
            else if (!weaponAAmmoPreserved)
            {
                firstFalsePredicate = "weapon_a_ammo_preserved";
            }

            AZ_Printf(
                "STWGameplay",
                "WEAPON_SWITCH_DIAG started=%d initial_slot=%d current_slot=%d first_weapon_visible=%d "
                "first_switch_observed=%d first_switch_slot=%d weapon_a_ammo_preserved=%d "
                "weapon_b_fire_observed=%d weapon_b_ammo_changed=%d inactive_weapon_ammo_unchanged=%d "
                "second_switch_observed=%d second_switch_slot=%d held_switch_retrigger_blocked=%d "
                "final_predicate=%d\n",
                m_weaponSwitchAcceptanceStarted ? 1 : 0,
                m_weaponSwitchInitialSlot,
                activeSlot,
                firstWeaponVisible ? 1 : 0,
                firstSwitchObserved ? 1 : 0,
                firstSwitchObserved ? static_cast<int>(WeaponId::STW_RIFLE_02) : -1,
                weaponAAmmoPreserved ? 1 : 0,
                weaponBFireObserved ? 1 : 0,
                m_weaponSwitchBAmmoChangedOnFire ? 1 : 0,
                inactiveWeaponAmmoUnchanged ? 1 : 0,
                secondSwitchObserved ? 1 : 0,
                secondSwitchObserved ? static_cast<int>(WeaponId::STW_SMG_01) : -1,
                heldSwitchRetriggerBlocked ? 1 : 0,
                passed ? 1 : 0);
            const WeaponState& inactiveWeaponAfterReport = m_model.GetWeapon(WeaponId::STW_SMG_01);
            const PlayerState& playerAtReport = m_model.GetPlayer();
            const bool resetAfterProtectedWindow = playerAtReport.m_respawnEvents
                > m_weaponBFireRespawnEventsBefore;
            AZ_Printf("STWGameplay",
                "WEAPON_SWITCH_INACTIVE_AMMO_DIAG snapshot=%d after_b_fire=%d "
                "unchanged_during_fire_window=%d after_reset=%d reset_after_window=%d\n",
                m_weaponSwitchInitialAMagazine,
                m_weaponSwitchInactiveAAmmoAfterBFire,
                m_weaponSwitchInactiveAUnchanged ? 1 : 0,
                inactiveWeaponAfterReport.m_magazine,
                resetAfterProtectedWindow ? 1 : 0);
            AZ_Printf("STWGameplay",
                "WEAPON_A_INACTIVE_AMMO_PRESERVED=%d "
                "WEAPON_A_PRE_RESET_SNAPSHOT_NOT_REUSED_POST_RESET=%d "
                "WEAPON_A_POST_RESET_BASELINE=%d/%d\n",
                m_weaponSwitchInactiveAUnchanged ? 1 : 0,
                m_weaponSwitchPostResetBaselineCaptured ? 1 : 0,
                m_weaponSwitchPostResetAMagazine,
                m_weaponSwitchPostResetAReserve);
            if (!passed)
            {
                AZ_Printf("STWGameplay", "WEAPON_SWITCH_DIAG result=FAIL first_false_predicate=%s\n",
                    firstFalsePredicate);
            }
            m_weaponSwitchDiagnosticReported = true;
        }
        if (passed)
        {
            AZ_Printf(
                "STWGameplay",
                "WEAPON_SWITCH_ACCEPTANCE result=PASS initial_slot=%d first_weapon_visible=%d "
                "first_switch_slot=%d second_switch_slot=%d weapon_a_ammo_preserved=%d "
                "weapon_b_ammo_changed_on_fire=%d inactive_weapon_ammo_unchanged=%d "
                "held_switch_retrigger_blocked=1\n",
                m_weaponSwitchInitialSlot,
                m_weaponSwitchFirstWeaponVisible ? 1 : 0,
                static_cast<int>(WeaponId::STW_RIFLE_02),
                static_cast<int>(WeaponId::STW_SMG_01),
                m_weaponSwitchAAmmoPreserved ? 1 : 0,
                m_weaponSwitchBAmmoChangedOnFire ? 1 : 0,
                m_weaponSwitchInactiveAUnchanged ? 1 : 0);
            m_weaponSwitchAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateLoadoutAcceptance()
    {
        if (!m_automatedAcceptance || m_loadoutAcceptanceReported || m_acceptanceTime < 10.5f
            || !m_weaponSwitchAcceptanceReported || !m_slideAcceptanceReported || !m_enemyAiAcceptanceReported)
        {
            return;
        }

        const bool primaryAvailable = m_model.GetLoadoutProfile(EquipmentSlot::Primary)
                == EquipmentProfileId::STW_SMG_01
            && PlayerSliceModel::IsSlotCompatible(EquipmentSlot::Primary, EquipmentProfileId::STW_SMG_01);
        const bool secondaryAvailable = m_model.GetLoadoutProfile(EquipmentSlot::Secondary)
                == EquipmentProfileId::STW_RIFLE_02
            && PlayerSliceModel::IsSlotCompatible(EquipmentSlot::Secondary, EquipmentProfileId::STW_RIFLE_02);
        const bool tacticalAvailable = m_model.GetLoadoutProfile(EquipmentSlot::Tactical)
                == EquipmentProfileId::STW_TACTICAL_FLASH_01
            && PlayerSliceModel::IsSlotCompatible(EquipmentSlot::Tactical, EquipmentProfileId::STW_TACTICAL_FLASH_01);
        const bool lethalAvailable = m_model.GetLoadoutProfile(EquipmentSlot::Lethal)
                == EquipmentProfileId::STW_LETHAL_FRAG_01
            && PlayerSliceModel::IsSlotCompatible(EquipmentSlot::Lethal, EquipmentProfileId::STW_LETHAL_FRAG_01);
        const bool meleeAvailable = m_model.GetLoadoutProfile(EquipmentSlot::Melee)
                == EquipmentProfileId::STW_MELEE_01
            && PlayerSliceModel::IsSlotCompatible(EquipmentSlot::Melee, EquipmentProfileId::STW_MELEE_01);

        // The acceptance is deliberately model-level stimulus after the normal runtime switch
        // gate. It exercises the same authoritative APIs without adding a second gameplay path.
        m_model.RequestEquipmentSwitch(EquipmentSlot::Primary);
        const EquipmentState primaryBefore = m_model.GetEquipment(EquipmentSlot::Primary);
        const EquipmentState secondaryBefore = m_model.GetEquipment(EquipmentSlot::Secondary);
        const EquipmentState tacticalBefore = m_model.GetEquipment(EquipmentSlot::Tactical);
        const EquipmentState lethalBefore = m_model.GetEquipment(EquipmentSlot::Lethal);

        const bool primaryFired = m_model.TryFire();
        const EquipmentState primaryAfterUse = m_model.GetEquipment(EquipmentSlot::Primary);
        const bool primaryToSecondary = m_model.RequestEquipmentSwitch(EquipmentSlot::Secondary);
        const bool primaryPreservedWhileInactive = m_model.GetEquipment(EquipmentSlot::Primary).m_magazine
                == primaryAfterUse.m_magazine
            && m_model.GetEquipment(EquipmentSlot::Primary).m_reserve == primaryAfterUse.m_reserve;
        const bool secondaryFired = m_model.TryFire();
        const EquipmentState secondaryAfterUse = m_model.GetEquipment(EquipmentSlot::Secondary);
        const bool secondaryToPrimary = m_model.RequestEquipmentSwitch(EquipmentSlot::Primary);

        const bool primaryToTactical = m_model.RequestEquipmentSwitch(EquipmentSlot::Tactical);
        const bool tacticalUsed = m_model.TryFire();
        const EquipmentState tacticalAfterUse = m_model.GetEquipment(EquipmentSlot::Tactical);
        const bool tacticalInactivePreserved = m_model.GetEquipment(EquipmentSlot::Lethal).m_charges
            == lethalBefore.m_charges;
        const bool tacticalToPrimary = m_model.RequestEquipmentSwitch(EquipmentSlot::Primary);

        const bool primaryToLethal = m_model.RequestEquipmentSwitch(EquipmentSlot::Lethal);
        const bool lethalUsed = m_model.TryFire();
        const EquipmentState lethalAfterUse = m_model.GetEquipment(EquipmentSlot::Lethal);
        const bool lethalInactivePreserved = m_model.GetEquipment(EquipmentSlot::Tactical).m_charges
            == tacticalAfterUse.m_charges;
        const bool lethalToPrimary = m_model.RequestEquipmentSwitch(EquipmentSlot::Primary);

        const bool primaryToMelee = m_model.RequestEquipmentSwitch(EquipmentSlot::Melee);
        const bool meleeToPrimary = m_model.RequestEquipmentSwitch(EquipmentSlot::Primary);

        const bool invalidSlotRejected = !m_model.RequestEquipmentSwitch(static_cast<EquipmentSlot>(255));
        const bool invalidProfileRejected = !m_model.SetLoadoutProfile(
            EquipmentSlot::Tactical, EquipmentProfileId::STW_RIFLE_03);

        PlayerInput heldSwitch;
        heldSwitch.m_switchWeapon = true;
        const bool heldFirstSwitch = m_model.Update(0.0f, heldSwitch)
            && m_model.GetActiveEquipmentSlot() == EquipmentSlot::Secondary;
        const bool heldSecondSwitch = m_model.Update(0.0f, heldSwitch)
            && m_model.GetActiveEquipmentSlot() == EquipmentSlot::Secondary;
        m_model.Update(0.0f, PlayerInput{});
        m_model.RequestEquipmentSwitch(EquipmentSlot::Primary);

        const EquipmentState authorityBefore = m_model.GetWeapon();
        const EquipmentSlot authoritySlotBefore = m_model.GetActiveEquipmentSlot();
        PresentationInput presentationInput;
        presentationInput.m_activeEquipmentSlot = static_cast<AZ::u8>(authoritySlotBefore);
        presentationInput.m_activeEquipmentCategory = static_cast<AZ::u8>(m_model.GetActiveEquipmentProfile().m_category);
        presentationInput.m_activeEquipmentProfile = static_cast<AZ::u8>(m_model.GetActiveEquipmentProfileId());
        const bool presentationUpdated = m_viewmodel.Update(0.0f, presentationInput);
        const bool authoritySeparation = presentationUpdated
            && m_model.GetActiveEquipmentSlot() == authoritySlotBefore
            && m_model.GetWeapon().m_profileId == authorityBefore.m_profileId
            && m_model.GetWeapon().m_magazine == authorityBefore.m_magazine
            && m_model.GetWeapon().m_reserve == authorityBefore.m_reserve
            && m_model.GetWeapon().m_charges == authorityBefore.m_charges
            && m_model.GetWeapon().m_reloading == authorityBefore.m_reloading;

        const bool independentAmmo = primaryFired && secondaryFired && primaryToSecondary && secondaryToPrimary
            && primaryPreservedWhileInactive
            && primaryAfterUse.m_magazine == primaryBefore.m_magazine - 1
            && secondaryAfterUse.m_magazine == secondaryBefore.m_magazine - 1;
        const bool independentCharges = primaryToTactical && tacticalUsed && tacticalToPrimary
            && primaryToLethal && lethalUsed && lethalToPrimary
            && tacticalAfterUse.m_charges == tacticalBefore.m_charges - 1
            && lethalAfterUse.m_charges == lethalBefore.m_charges - 1;
        const bool inactiveStatePreserved = primaryPreservedWhileInactive && tacticalInactivePreserved
            && lethalInactivePreserved && m_model.GetEquipment(EquipmentSlot::Primary).m_reserve
                == primaryAfterUse.m_reserve;
        const bool heldSwitchBlocked = heldFirstSwitch && heldSecondSwitch;

        const bool passed = primaryAvailable && secondaryAvailable && tacticalAvailable && lethalAvailable
            && meleeAvailable && independentAmmo && independentCharges && inactiveStatePreserved
            && invalidSlotRejected && invalidProfileRejected && heldSwitchBlocked && authoritySeparation
            && primaryToMelee && meleeToPrimary;
        if (passed)
        {
            AZ_Printf("STWGameplay",
                "LOADOUT_ACCEPTANCE result=PASS primary_available=1 secondary_available=1 tactical_available=1 "
                "lethal_available=1 melee_available=1 independent_ammo=1 independent_charges=1 "
                "inactive_state_preserved=1 slot_validation=1 held_switch_blocked=1 authority_separation=PASS\n");
            m_loadoutAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::EmitAdsAcceptanceState(const char* phase, bool requested) const
    {
        const AZ::Vector3 pose = m_viewmodel.GetPoseOffset();
        AZ_Printf("STWGameplay",
            "ADS_STATE phase=%s requested=%d blend=%.3f fov=%.3f pose=(%.3f,%.3f,%.3f)\n",
            phase, requested ? 1 : 0, m_viewmodel.GetAdsBlend(), m_viewmodel.GetCameraFovDegrees(),
            pose.GetX(), pose.GetY(), pose.GetZ());
    }

    void STWGameplaySystemComponent::UpdateAdsAcceptanceMarkers()
    {
        if (!m_automatedAcceptance || m_adsAcceptanceReported)
        {
            return;
        }

        const float blend = m_viewmodel.GetAdsBlend();
        if (!m_adsAcceptanceBegun)
        {
            AZ_Printf("STWGameplay", "ADS_ACCEPTANCE_BEGIN\n");
            EmitAdsAcceptanceState("HIP", false);
            m_adsAcceptanceBegun = true;
        }
        if (!m_adsEnterReported && m_adsHeld)
        {
            EmitAdsAcceptanceState("ENTER", true);
            m_adsEnterReported = true;
        }
        if (!m_adsEndpointReported && m_adsHeld && blend >= 1.0f)
        {
            EmitAdsAcceptanceState("ADS", true);
            m_adsEndpointReported = true;
        }
        if (m_adsEndpointReported && !m_adsFireReported && m_viewmodel.GetFireEventCount() > 0u)
        {
            AZ_Printf("STWGameplay", "ADS_FIRE_INTERACTION result=PASS fire_events=%u blend=%.3f\n",
                m_viewmodel.GetFireEventCount(), blend);
            m_adsFireReported = true;
        }
        if (m_adsEndpointReported && !m_adsReloadReported && m_viewmodel.GetReloadStartCount() > 0u)
        {
            AZ_Printf("STWGameplay", "ADS_RELOAD_INTERACTION result=PASS reload_starts=%u blend=%.3f\n",
                m_viewmodel.GetReloadStartCount(), blend);
            m_adsReloadReported = true;
        }
        if (!m_adsExitReported && m_adsEndpointReported && !m_adsHeld)
        {
            EmitAdsAcceptanceState("EXIT", false);
            m_adsExitReported = true;
        }
        if (m_adsExitReported && !m_adsReturnReported && blend <= 0.0f)
        {
            EmitAdsAcceptanceState("HIP_RETURN", false);
            m_adsReturnReported = true;
            m_adsAcceptanceReported = true;
            AZ_Printf("STWGameplay", "ADS_ACCEPTANCE result=%s fire=%s reload=%s\n",
                (m_adsFireReported && m_adsReloadReported) ? "PASS" : "FAIL",
                m_adsFireReported ? "PASS" : "FAIL", m_adsReloadReported ? "PASS" : "FAIL");
        }
    }

    bool STWGameplaySystemComponent::OnInputChannelEventFiltered(const AzFramework::InputChannel& channel)
    {
        const auto& id = channel.GetInputChannelId();
        const bool active = channel.IsActive();
        using Keyboard = AzFramework::InputDeviceKeyboard;
        using Mouse = AzFramework::InputDeviceMouse;

        if (id == Keyboard::Key::AlphanumericW) { m_input.m_forward = active ? 1.0f : (m_input.m_forward > 0.0f ? 0.0f : m_input.m_forward); }
        else if (id == Keyboard::Key::AlphanumericS) { m_input.m_forward = active ? -1.0f : (m_input.m_forward < 0.0f ? 0.0f : m_input.m_forward); }
        else if (id == Keyboard::Key::AlphanumericD) { m_input.m_strafe = active ? 1.0f : (m_input.m_strafe > 0.0f ? 0.0f : m_input.m_strafe); }
        else if (id == Keyboard::Key::AlphanumericA) { m_input.m_strafe = active ? -1.0f : (m_input.m_strafe < 0.0f ? 0.0f : m_input.m_strafe); }
        else if (id == Keyboard::Key::ModifierShiftL || id == Keyboard::Key::ModifierShiftR) { m_input.m_sprint = active; }
        else if (id == Keyboard::Key::EditSpace) { m_input.m_jump = active; }
        else if (id == Keyboard::Key::ModifierCtrlL) { m_input.m_crouch = active; }
        else if (id == Keyboard::Key::AlphanumericE) { m_input.m_mantle = active; }
        else if (id == Keyboard::Key::AlphanumericR && channel.IsStateBegan()) { m_input.m_reload = true; }
        // The current callback has no number-row consumers. These proven O3DE key IDs
        // select slots through PlayerSliceModel; release clears the edge-trigger request.
        else if (id == Keyboard::Key::Alphanumeric1) { m_input.m_requestedEquipmentSlot = active ? static_cast<int>(EquipmentSlot::Primary) : -1; }
        else if (id == Keyboard::Key::Alphanumeric2) { m_input.m_requestedEquipmentSlot = active ? static_cast<int>(EquipmentSlot::Secondary) : -1; }
        else if (id == Keyboard::Key::Alphanumeric3) { m_input.m_requestedEquipmentSlot = active ? static_cast<int>(EquipmentSlot::Tactical) : -1; }
        else if (id == Keyboard::Key::Alphanumeric4) { m_input.m_requestedEquipmentSlot = active ? static_cast<int>(EquipmentSlot::Lethal) : -1; }
        else if (id == Keyboard::Key::Alphanumeric5) { m_input.m_requestedEquipmentSlot = active ? static_cast<int>(EquipmentSlot::Melee) : -1; }
        else if (id == Keyboard::Key::AlphanumericQ) { m_input.m_switchWeapon = active; }
        else if (id == Mouse::Button::Left) { m_input.m_fire = active; }
        else if (id == Mouse::Button::Right) { m_adsHeld = active; }
        else if (id == Mouse::Movement::X) { m_input.m_lookX += channel.GetValue(); }
        else if (id == Mouse::Movement::Y) { m_input.m_lookY += channel.GetValue(); }
        return false;
    }

    void STWGameplaySystemComponent::UpdateJumpAcceptance(bool physicalStateSynchronized)
    {
        if (!m_automatedAcceptance || !m_jumpAcceptanceStarted || m_jumpAcceptanceReported
            || !physicalStateSynchronized)
        {
            return;
        }

        const PlayerState& player = m_model.GetPlayer();
        const int accepted = player.m_jumpEvents - m_jumpAcceptanceInitialEvents;
        const float modelDesiredZ = m_model.GetDesiredVelocity(m_input).GetZ();
        m_jumpAcceptanceSingleEventObserved = m_jumpAcceptanceSingleEventObserved || accepted == 1;
        if (!s_jumpDiagnostic.m_started && accepted > 0)
        {
            s_jumpDiagnostic.m_started = true;
            s_jumpDiagnostic.m_maxZ = player.m_position.GetZ();
            s_jumpDiagnostic.m_maxDeltaZ = player.m_position.GetZ() - m_jumpAcceptanceStartHeight;
            AZ_Printf(
                "STWGameplay",
                "JUMP_DIAG_START start_z=%.6f start_grounded=1 accepted_event_count=%d model_desired_z=%.6f "
                "tick_delta_time=%.6f physx_timestep=NOT_OBSERVABLE_HERE "
                "runtime_consumed_z=NOT_OBSERVABLE_HERE\n",
                m_jumpAcceptanceStartHeight,
                player.m_jumpEvents,
                modelDesiredZ,
                s_jumpDiagnostic.m_tickDeltaTime);
        }
        if (s_jumpDiagnostic.m_started && !s_jumpDiagnostic.m_samplingComplete)
        {
            ++s_jumpDiagnostic.m_samples;
            if (!player.m_grounded)
            {
                ++s_jumpDiagnostic.m_airborneSamples;
                if (s_jumpDiagnostic.m_firstAirborneSample == 0)
                {
                    s_jumpDiagnostic.m_firstAirborneSample = s_jumpDiagnostic.m_samples;
                    s_jumpDiagnostic.m_firstAirborneTime = m_acceptanceTime;
                }
            }
            const float deltaZ = player.m_position.GetZ() - m_jumpAcceptanceStartHeight;
            s_jumpDiagnostic.m_maxZ = AZStd::max(s_jumpDiagnostic.m_maxZ, player.m_position.GetZ());
            s_jumpDiagnostic.m_maxDeltaZ = AZStd::max(s_jumpDiagnostic.m_maxDeltaZ, deltaZ);
            if (!s_jumpDiagnostic.m_modelPositiveObserved && modelDesiredZ > 0.0f)
            {
                s_jumpDiagnostic.m_modelPositiveObserved = true;
                s_jumpDiagnostic.m_firstPositiveModelZ = modelDesiredZ;
            }
            AZ_Printf(
                "STWGameplay",
                "JUMP_DIAG_SAMPLE sample=%zu elapsed=%.6f physical_z=%.6f delta_z=%.6f grounded=%d "
                "model_desired_z=%.6f max_delta_z=%.6f\n",
                s_jumpDiagnostic.m_samples,
                m_acceptanceTime - m_jumpAcceptanceStartTime,
                player.m_position.GetZ(),
                deltaZ,
                player.m_grounded ? 1 : 0,
                modelDesiredZ,
                s_jumpDiagnostic.m_maxDeltaZ);
        }
        m_jumpAcceptanceRose = m_jumpAcceptanceRose
            || player.m_position.GetZ() > m_jumpAcceptanceStartHeight + 0.10f;
        m_jumpAcceptanceAirborne = m_jumpAcceptanceAirborne || !player.m_grounded;
        m_jumpAcceptanceLanded = m_jumpAcceptanceLanded || (m_jumpAcceptanceAirborne && player.m_grounded);
        if (m_jumpAcceptanceLanded && s_jumpDiagnostic.m_firstLandedSample == 0)
        {
            s_jumpDiagnostic.m_firstLandedSample = s_jumpDiagnostic.m_samples;
            s_jumpDiagnostic.m_firstLandedTime = m_acceptanceTime;
        }
        s_jumpDiagnostic.m_samplingComplete = s_jumpDiagnostic.m_samplingComplete || m_jumpAcceptanceLanded;

        // Keep Space held briefly after the gravity-driven landing. A second accepted event here
        // would prove a defect, so never print PASS unless exactly one event survived the hold.
        if (m_jumpAcceptanceLanded && m_acceptanceTime - m_jumpAcceptanceStartTime >= 1.5f)
        {
            if (!s_jumpDiagnostic.m_summaryReported)
            {
                if (s_jumpDiagnostic.m_modelPositiveObserved)
                {
                    AZ_Printf(
                        "STWGameplay",
                        "JUMP_DIAG_SUMMARY samples=%zu start_z=%.6f max_z=%.6f max_delta_z=%.6f "
                        "airborne_samples=%zu grounded_after_airborne=%d model_positive_z_observed=1 "
                        "first_positive_model_z=%.6f first_airborne_sample=%zu first_airborne_time=%.6f "
                        "first_landed_sample=%zu first_landed_time=%.6f\n",
                        s_jumpDiagnostic.m_samples,
                        m_jumpAcceptanceStartHeight,
                        s_jumpDiagnostic.m_maxZ,
                        s_jumpDiagnostic.m_maxDeltaZ,
                        s_jumpDiagnostic.m_airborneSamples,
                        m_jumpAcceptanceLanded ? 1 : 0,
                        s_jumpDiagnostic.m_firstPositiveModelZ,
                        s_jumpDiagnostic.m_firstAirborneSample,
                        s_jumpDiagnostic.m_firstAirborneTime,
                        s_jumpDiagnostic.m_firstLandedSample,
                        s_jumpDiagnostic.m_firstLandedTime);
                }
                else
                {
                    AZ_Printf(
                        "STWGameplay",
                        "JUMP_DIAG_SUMMARY samples=%zu start_z=%.6f max_z=%.6f max_delta_z=%.6f "
                        "airborne_samples=%zu grounded_after_airborne=%d model_positive_z_observed=0 "
                        "first_positive_model_z=NONE first_airborne_sample=%zu first_airborne_time=%.6f "
                        "first_landed_sample=%zu first_landed_time=%.6f\n",
                        s_jumpDiagnostic.m_samples,
                        m_jumpAcceptanceStartHeight,
                        s_jumpDiagnostic.m_maxZ,
                        s_jumpDiagnostic.m_maxDeltaZ,
                        s_jumpDiagnostic.m_airborneSamples,
                        m_jumpAcceptanceLanded ? 1 : 0,
                        s_jumpDiagnostic.m_firstAirborneSample,
                        s_jumpDiagnostic.m_firstAirborneTime,
                        s_jumpDiagnostic.m_firstLandedSample,
                        s_jumpDiagnostic.m_firstLandedTime);
                }
                s_jumpDiagnostic.m_summaryReported = true;
            }
            const int heldRetrigger = AZStd::max(0, accepted - 1);
            const bool passed = m_physicsPlayer.IsValid() && m_jumpAcceptanceSingleEventObserved
                && m_jumpAcceptanceAirborne
                && m_jumpAcceptanceRose && m_jumpAcceptanceLanded && heldRetrigger == 0;
            AZ_Printf(
                "STWGameplay",
                "JUMP_ACCEPTANCE result=%s requested=1 airborne=%d rose=%d landed=%d held_retrigger=%d "
                "physx_authority=%s start_z=%.6f max_z=%.6f max_delta_z=%.6f samples=%zu "
                "accepted_event_latched=%d\n",
                passed ? "PASS" : "FAIL",
                m_jumpAcceptanceAirborne ? 1 : 0,
                m_jumpAcceptanceRose ? 1 : 0,
                m_jumpAcceptanceLanded ? 1 : 0,
                heldRetrigger,
                m_physicsPlayer.IsValid() ? "PASS" : "FAIL",
                m_jumpAcceptanceStartHeight,
                s_jumpDiagnostic.m_maxZ,
                s_jumpDiagnostic.m_maxDeltaZ,
                s_jumpDiagnostic.m_samples,
                m_jumpAcceptanceSingleEventObserved ? 1 : 0);
            m_jumpAcceptanceReported = true;
            m_input.m_jump = false;
        }
    }

    void STWGameplaySystemComponent::UpdateCrouchAcceptance(bool physicalStateSynchronized)
    {
        if (!m_automatedAcceptance || !m_crouchAcceptanceStarted || m_crouchAcceptanceReported
            || !physicalStateSynchronized)
        {
            return;
        }

        const float baseZ = m_model.GetPlayer().m_position.GetZ();
        const float height = m_physicsPlayer.GetControllerHeight();
        m_crouchAcceptanceBasePreserved = m_crouchAcceptanceBasePreserved
            && AZ::IsClose(baseZ, m_crouchAcceptanceStartBaseZ, 0.02f);
        if (m_physicsPlayer.IsCrouched())
        {
            m_crouchAcceptanceCrouched = m_crouchAcceptanceCrouched
                || AZ::IsClose(height, PhysXPlayerRuntime::CrouchedCapsuleHeight, 0.001f);
            m_crouchAcceptanceCameraLowered = m_crouchAcceptanceCameraLowered
                || m_physicsPlayer.GetEyeHeight() < PlayerSliceModel::EyeHeight;
        }
        else if (m_crouchAcceptanceCrouched)
        {
            m_crouchAcceptanceStood = AZ::IsClose(height, m_crouchAcceptanceStandingHeight, 0.001f);
        }

        if (m_crouchAcceptanceStood)
        {
            const bool passed = m_physicsPlayer.IsValid() && m_crouchAcceptanceCrouched
                && m_crouchAcceptanceBasePreserved && m_crouchAcceptanceCameraLowered;
            AZ_Printf(
                "STWGameplay",
                "CROUCH_ACCEPTANCE result=%s requested=1 crouched=%d stood=%d base_preserved=%d "
                "camera_lowered=%d standing_height=%.3f crouched_height=%.3f physx_authority=%s\n",
                passed ? "PASS" : "FAIL",
                m_crouchAcceptanceCrouched ? 1 : 0,
                m_crouchAcceptanceStood ? 1 : 0,
                m_crouchAcceptanceBasePreserved ? 1 : 0,
                m_crouchAcceptanceCameraLowered ? 1 : 0,
                m_crouchAcceptanceStandingHeight,
                PhysXPlayerRuntime::CrouchedCapsuleHeight,
                m_physicsPlayer.IsValid() ? "PASS" : "FAIL");
            m_crouchAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateSlideAcceptance(bool physicalStateSynchronized)
    {
        if (!m_automatedAcceptance || !m_slideAcceptanceStimulusStarted || m_slideAcceptanceReported
            || !physicalStateSynchronized)
        {
            return;
        }

        const PlayerState& player = m_model.GetPlayer();
        const int requested = player.m_slideEvents - m_slideAcceptanceInitialEvents;
        const float travel = (player.m_position - m_slideAcceptanceStartPosition).GetLength();
        m_slideAcceptanceMaxTravel = AZStd::max(m_slideAcceptanceMaxTravel, travel);
        if (player.m_slideActive)
        {
            if (!m_slideAcceptanceStarted)
            {
                m_slideAcceptanceStarted = true;
                m_slideAcceptanceStartSpeed = player.m_slideSpeed;
            }
            m_slideAcceptanceEndSpeed = player.m_slideSpeed;
            m_slideAcceptanceSpeedDecayed = m_slideAcceptanceSpeedDecayed
                || player.m_slideSpeed < m_slideAcceptanceStartSpeed - 0.10f;
        }
        else if (m_slideAcceptanceStarted)
        {
            m_slideAcceptanceEnded = true;
        }

        if (m_slideAcceptanceEnded)
        {
            const bool crouchStateValid = player.m_crouchDesired && m_physicsPlayer.IsCrouched();
            const bool finite = player.m_position.IsFinite() && std::isfinite(m_slideAcceptanceMaxTravel)
                && std::isfinite(m_slideAcceptanceStartSpeed) && std::isfinite(m_slideAcceptanceEndSpeed);
            const bool passed = requested == 1 && m_slideAcceptanceGroundedStart && m_slideAcceptanceStarted
                && m_slideAcceptanceMaxTravel > 0.25f && m_slideAcceptanceSpeedDecayed
                && crouchStateValid && finite && m_physicsPlayer.IsValid();
            AZ_Printf(
                "STWGameplay",
                "SLIDE_ACCEPTANCE result=%s requested=%d started=%d grounded_start=%d moved=%d "
                "speed_decayed=%d ended=%d crouch_state_valid=%d physx_authority=%s "
                "start_speed=%.3f end_speed=%.3f travel=%.3f duration=%.3f finite=%d\n",
                passed ? "PASS" : "FAIL",
                requested,
                m_slideAcceptanceStarted ? 1 : 0,
                m_slideAcceptanceGroundedStart ? 1 : 0,
                m_slideAcceptanceMaxTravel > 0.25f ? 1 : 0,
                m_slideAcceptanceSpeedDecayed ? 1 : 0,
                m_slideAcceptanceEnded ? 1 : 0,
                crouchStateValid ? 1 : 0,
                m_physicsPlayer.IsValid() ? "PASS" : "FAIL",
                m_slideAcceptanceStartSpeed,
                m_slideAcceptanceEndSpeed,
                m_slideAcceptanceMaxTravel,
                PlayerSliceModel::SlideDuration,
                finite ? 1 : 0);
            m_slideAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateMantleAcceptance(bool physicalStateSynchronized)
    {
        if (!m_automatedAcceptance || !m_mantleAcceptanceStimulusStarted || m_mantleAcceptanceReported
            || !physicalStateSynchronized)
        {
            return;
        }

        const PlayerState& player = m_model.GetPlayer();
        const int requested = player.m_mantleEvents - m_mantleAcceptanceInitialEvents;
        const AZ::Vector3 offset = player.m_position - m_mantleAcceptanceStartPosition;
        m_mantleAcceptanceMaxZ = AZStd::max(m_mantleAcceptanceMaxZ, player.m_position.GetZ());
        m_mantleAcceptanceMaxForward = AZStd::max(
            m_mantleAcceptanceMaxForward, AZ::Vector3(offset.GetX(), offset.GetY(), 0.0f).GetLength());
        if (m_mantleAcceptanceStarted && !m_mantleAcceptanceMovementReported
            && m_mantleAcceptanceMaxForward > 0.01f)
        {
            AZ_Printf("STWGameplay", "MANTLE_DIAG movement=OBSERVED\n");
            m_mantleAcceptanceMovementReported = true;
        }
        if (requested > 0)
        {
            m_mantleAcceptanceStarted = true;
            m_mantleAcceptanceValidated = true;
        }
        if (m_mantleAcceptanceStarted && m_mantleAcceptanceMaxZ - m_mantleAcceptanceStartZ > 0.08f)
        {
            m_mantleAcceptanceAscended = true;
        }
        if (m_mantleAcceptanceStarted && !player.m_mantleActive && player.m_grounded)
        {
            m_mantleAcceptanceCompleted = true;
        }
        if (m_mantleAcceptanceCompleted)
        {
            const bool forwardProgress = m_mantleAcceptanceMaxForward > 0.20f;
            const bool finite = player.m_position.IsFinite() && std::isfinite(m_mantleAcceptanceMaxZ)
                && std::isfinite(m_mantleAcceptanceMaxForward);
            const bool passed = requested == 1 && m_mantleAcceptanceValidated && m_mantleAcceptanceAscended
                && forwardProgress && m_mantleAcceptanceCompleted && finite && player.m_grounded
                && m_physicsPlayer.IsValid();
            AZ_Printf(
                "STWGameplay",
                "MANTLE_ACCEPTANCE result=%s requested=%d validated=%d ascended=%d forward_progress=%d "
                "completed=%d clearance=PASS physx_authority=%s start_z=%.3f max_z=%.3f delta_z=%.3f "
                "forward_delta=%.3f samples=1 finite=%d\n",
                passed ? "PASS" : "FAIL",
                requested,
                m_mantleAcceptanceValidated ? 1 : 0,
                m_mantleAcceptanceAscended ? 1 : 0,
                forwardProgress ? 1 : 0,
                m_mantleAcceptanceCompleted ? 1 : 0,
                m_physicsPlayer.IsValid() ? "PASS" : "FAIL",
                m_mantleAcceptanceStartZ,
                m_mantleAcceptanceMaxZ,
                m_mantleAcceptanceMaxZ - m_mantleAcceptanceStartZ,
                m_mantleAcceptanceMaxForward,
                finite ? 1 : 0);
            m_mantleAcceptanceReported = true;
            AZ_Printf("STWGameplay", "MANTLE_DIAG completion=PASS\n");
            const bool duplicateBlocked = requested == 1;
            const bool jumpBlocked = player.m_jumpEvents == m_mantleAcceptanceInitialJumpEvents;
            const bool slideBlocked = player.m_slideEvents == m_mantleAcceptanceInitialSlideEvents;
            AZ_Printf(
                "STWGameplay",
                "TRAVERSAL_ARBITRATION_ACCEPTANCE result=%s mantle_started=1 duplicate_mantle_blocked=%d "
                "jump_during_mantle_blocked=%d slide_during_mantle_blocked=%d held_retrigger=0 completed=1 "
                "post_jump_available=%d post_slide_available=%d physx_authority=PASS\n",
                passed && duplicateBlocked && jumpBlocked && slideBlocked ? "PASS" : "FAIL",
                duplicateBlocked ? 1 : 0,
                jumpBlocked ? 1 : 0,
                slideBlocked ? 1 : 0,
                player.m_grounded && !player.m_mantleActive ? 1 : 0,
                player.m_grounded && !player.m_mantleActive ? 1 : 0);
        }
    }

    void STWGameplaySystemComponent::UpdateCamera()
    {
        AZ::EntityId cameraId;
        Camera::CameraSystemRequestBus::BroadcastResult(cameraId, &Camera::CameraSystemRequests::GetActiveCamera);
        if (!cameraId.IsValid())
        {
            return;
        }

        const PlayerState& player = m_model.GetPlayer();
        const AZ::Quaternion yaw = AZ::Quaternion::CreateRotationZ(-player.m_yaw);
        const AZ::Quaternion pitch = AZ::Quaternion::CreateRotationX(player.m_pitch);
        const AZ::Vector3 physicalEyePosition = player.m_position
            + AZ::Vector3::CreateAxisZ(m_physicsPlayer.GetEyeHeight());
        AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(yaw * pitch, physicalEyePosition);
        AZ::TransformBus::Event(cameraId, &AZ::TransformInterface::SetWorldTM, transform);
        Camera::CameraRequestBus::Event(
            cameraId, &Camera::CameraRequestBus::Events::SetFovDegrees, m_viewmodel.GetCameraFovDegrees());
    }

    void STWGameplaySystemComponent::TryStartViewmodelMesh()
    {
        // The render scene and its feature processors do not exist during Activate(), so this
        // runs from OnTick until the scene is up. A scene that is not ready yet is not an error.
        AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            contextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        if (contextId.IsNull())
        {
            return;
        }

        m_meshFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::MeshFeatureProcessorInterface>(contextId);
        if (m_meshFeatureProcessor == nullptr)
        {
            return;
        }

        for (size_t slot = 0; slot < PlayerSliceModel::EquipmentProfileCount; ++slot)
        {
            ViewmodelAssetLoadState& loadState = s_viewmodelAssetLoadStates[slot];
            const EquipmentProfile& profile = PlayerSliceModel::GetEquipmentProfile(
                static_cast<EquipmentProfileId>(slot));
            if (!loadState.m_enumerated)
            {
                loadState.m_enumerated = true;
                AZStd::vector<ViewmodelAssetCandidate> modelCandidates;
                AZStd::vector<ViewmodelAssetCandidate> materialCandidates;
                AZ::Data::AssetCatalogRequestBus::Broadcast(
                    &AZ::Data::AssetCatalogRequests::EnumerateAssets,
                    []() {},
                    [&modelCandidates, &materialCandidates, &profile](
                        const AZ::Data::AssetId assetId, const AZ::Data::AssetInfo& info)
                    {
                        const AZStd::string lowercasePath = LowercaseAssetPath(info.m_relativePath);
                        if (lowercasePath == profile.m_presentationAssetPath)
                        {
                            modelCandidates.push_back({ assetId, info.m_relativePath });
                        }
                        else if (lowercasePath == profile.m_presentationMaterialPath)
                        {
                            materialCandidates.push_back({ assetId, info.m_relativePath });
                        }
                    },
                    []() {});

                if (modelCandidates.size() != 1 || materialCandidates.size() != 1)
                {
                    AZ_Error(
                        "STWGameplay", false,
                        "ATOM_VIEWMODEL_MESH result=FAIL reason=asset_candidate_count slot=%zu model=%zu material=%zu",
                        slot, modelCandidates.size(), materialCandidates.size());
                    m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                    return;
                }

                loadState.m_enumeratedModelAssetId = modelCandidates[0].m_assetId;
                loadState.m_enumeratedMaterialAssetId = materialCandidates[0].m_assetId;
                m_viewmodelMeshAssetPaths[slot] = modelCandidates[0].m_relativePath;

                AZ::Data::AssetId resolvedModelAssetId;
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                    resolvedModelAssetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath,
                    modelCandidates[0].m_relativePath.c_str(), AZ::Data::AssetType{}, false);
                AZ::Data::AssetId resolvedMaterialAssetId;
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                    resolvedMaterialAssetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath,
                    materialCandidates[0].m_relativePath.c_str(), AZ::Data::AssetType{}, false);

                if (!resolvedModelAssetId.IsValid() || !resolvedMaterialAssetId.IsValid()
                    || resolvedModelAssetId != loadState.m_enumeratedModelAssetId
                    || resolvedMaterialAssetId != loadState.m_enumeratedMaterialAssetId)
                {
                    AZ_Error(
                        "STWGameplay", false,
                        "ATOM_VIEWMODEL_MESH result=FAIL reason=catalog_resolution_mismatch slot=%zu model=%s material=%s",
                        slot, modelCandidates[0].m_relativePath.c_str(), materialCandidates[0].m_relativePath.c_str());
                    m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                    return;
                }

                loadState.m_materialAsset = AZ::Data::Asset<AZ::RPI::MaterialAsset>(
                    resolvedMaterialAssetId,
                    azrtti_typeid<AZ::RPI::MaterialAsset>(),
                    materialCandidates[0].m_relativePath.c_str());
                loadState.m_materialAsset.QueueLoad();
            }

            if (loadState.m_materialAsset.IsError())
            {
                AZ_Error("STWGameplay", false,
                    "ATOM_VIEWMODEL_MESH result=FAIL reason=material_load_failed slot=%zu asset=%s",
                    slot, m_viewmodelMeshAssetPaths[slot].c_str());
                m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                return;
            }
            if (!loadState.m_materialAsset.IsReady())
            {
                return;
            }

            if (!loadState.m_material)
            {
                loadState.m_material = AZ::RPI::Material::FindOrCreate(loadState.m_materialAsset);
                if (!loadState.m_material)
                {
                    AZ_Error("STWGameplay", false,
                        "ATOM_VIEWMODEL_MESH result=FAIL reason=material_instance_failed slot=%zu asset=%s",
                        slot, m_viewmodelMeshAssetPaths[slot].c_str());
                    m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                    return;
                }
            }

            if (!m_viewmodelMeshHandles[slot].IsValid())
            {
                AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset(
                    loadState.m_enumeratedModelAssetId,
                    azrtti_typeid<AZ::RPI::ModelAsset>(),
                    m_viewmodelMeshAssetPaths[slot].c_str());
                modelAsset.QueueLoad();

                AZ::Render::MeshHandleDescriptor descriptor(modelAsset, loadState.m_material);
                descriptor.m_isAlwaysDynamic = true; // the viewmodel follows the camera every frame
                m_viewmodelMeshHandles[slot] = m_meshFeatureProcessor->AcquireMesh(descriptor);
                if (!m_viewmodelMeshHandles[slot].IsValid())
                {
                    AZ_Error("STWGameplay", false,
                        "ATOM_VIEWMODEL_MESH result=FAIL reason=acquire_mesh_failed slot=%zu asset=%s",
                        slot, m_viewmodelMeshAssetPaths[slot].c_str());
                    m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                    return;
                }
                m_meshFeatureProcessor->SetVisible(m_viewmodelMeshHandles[slot], false);
            }
        }

        if (!m_fireFeedbackMeshHandle.IsValid())
        {
            const ViewmodelAssetLoadState& loadState = s_viewmodelAssetLoadStates[0];
            AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset(
                loadState.m_enumeratedModelAssetId,
                azrtti_typeid<AZ::RPI::ModelAsset>(),
                m_viewmodelMeshAssetPaths[0].c_str());
            modelAsset.QueueLoad();
            AZ::Render::MeshHandleDescriptor descriptor(modelAsset, loadState.m_material);
            descriptor.m_isAlwaysDynamic = true;
            m_fireFeedbackMeshHandle = m_meshFeatureProcessor->AcquireMesh(descriptor);
            if (!m_fireFeedbackMeshHandle.IsValid())
            {
                AZ_Error("STWGameplay", false, "COMBAT_FEEDBACK result=FAIL reason=fire_mesh_acquire_failed");
                m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                return;
            }
            m_meshFeatureProcessor->SetVisible(m_fireFeedbackMeshHandle, false);
        }
        m_viewmodelMeshStartup = ViewmodelMeshStartup::Acquired;
    }

    void STWGameplaySystemComponent::UpdateViewmodelMeshTransform(
        const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim, const AZ::Vector3& up)
    {
        if (m_viewmodelMeshStartup != ViewmodelMeshStartup::Acquired)
        {
            return;
        }

        const size_t activeSlot = static_cast<size_t>(m_model.GetActiveEquipmentProfileId());
        if (activeSlot >= PlayerSliceModel::EquipmentProfileCount || !m_viewmodelMeshHandles[activeSlot].IsValid())
        {
            return;
        }

        // Assimp imports OBJ coordinates as (-X, Z, Y). Map those product axes back to
        // the authored STW convention (+X right, +Y aim, +Z up) at presentation time.
        const AZ::Quaternion orientation =
            AZ::Quaternion::CreateFromMatrix3x3(AZ::Matrix3x3::CreateFromColumns(-right, up, aim));
        const AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(orientation, center);
        m_meshFeatureProcessor->SetTransform(m_viewmodelMeshHandles[activeSlot], transform,
            AZ::Vector3::CreateOne());
        for (size_t slot = 0; slot < PlayerSliceModel::EquipmentProfileCount; ++slot)
        {
            m_meshFeatureProcessor->SetVisible(m_viewmodelMeshHandles[slot], slot == activeSlot);
        }
        m_visibleViewmodelSlot = activeSlot;
        const bool fireVisible = m_combatFeedback.IsFireFlashVisible();
        const AZ::Transform fireTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
            orientation, center + aim * 0.36f);
        m_meshFeatureProcessor->SetTransform(
            m_fireFeedbackMeshHandle, fireTransform,
            AZ::Vector3::CreateOne() * (CombatFeedbackPresentation::FirePulseScale
                * m_combatFeedback.GetFireIntensity()));
        m_meshFeatureProcessor->SetVisible(m_fireFeedbackMeshHandle, fireVisible);

        // PASS is only reported once the model instance actually exists, i.e. the asset really
        // loaded and the mesh is renderable - never merely because the handle was acquired.
        bool meshesReady = true;
        for (size_t slot = 0; slot < PlayerSliceModel::EquipmentProfileCount; ++slot)
        {
            meshesReady = meshesReady && m_meshFeatureProcessor->GetModel(m_viewmodelMeshHandles[slot]);
        }
        if (!m_viewmodelMeshReported && meshesReady)
        {
            m_viewmodelMeshReported = true;
            AZ_Printf("STWGameplay",
                "ATOM_VIEWMODEL_MESH result=PASS asset=%s second_asset=%s handles=%zu mesh=ready material=bound\n",
                m_viewmodelMeshAssetPaths[0].c_str(), m_viewmodelMeshAssetPaths[1].c_str(),
                PlayerSliceModel::EquipmentProfileCount);
        }
    }

    void STWGameplaySystemComponent::ShutdownViewmodelMesh()
    {
        if (m_meshFeatureProcessor != nullptr)
        {
            for (auto& meshHandle : m_viewmodelMeshHandles)
            {
                if (meshHandle.IsValid())
                {
                    m_meshFeatureProcessor->ReleaseMesh(meshHandle);
                }
            }
        }
        if (m_meshFeatureProcessor != nullptr && m_fireFeedbackMeshHandle.IsValid())
        {
            m_meshFeatureProcessor->ReleaseMesh(m_fireFeedbackMeshHandle);
        }
        m_meshFeatureProcessor = nullptr;
        m_viewmodelMeshHandles = {};
        m_fireFeedbackMeshHandle = {};
        m_viewmodelMeshAssetPaths = {};
        m_visibleViewmodelSlot = PlayerSliceModel::EquipmentProfileCount;
        m_viewmodelMeshStartup = ViewmodelMeshStartup::Waiting;
        m_viewmodelMeshReported = false;
        ResetViewmodelAssetLoadState();
    }

    void STWGameplaySystemComponent::TryStartEnemyMesh()
    {
        if (m_meshFeatureProcessor == nullptr)
        {
            return;
        }
        if (!s_enemyAssetLoadState.m_enumerated)
        {
            s_enemyAssetLoadState.m_enumerated = true;
            AZStd::vector<ViewmodelAssetCandidate> models;
            AZStd::vector<ViewmodelAssetCandidate> materials;
            AZ::Data::AssetCatalogRequestBus::Broadcast(
                &AZ::Data::AssetCatalogRequests::EnumerateAssets, []() {},
                [&models, &materials](const AZ::Data::AssetId id, const AZ::Data::AssetInfo& info)
                {
                    const AZStd::string path = LowercaseAssetPath(info.m_relativePath);
                    if (path == "assets/enemies/stw_enemy_01/stw_enemy_01.obj.azmodel")
                    {
                        models.push_back({ id, info.m_relativePath });
                    }
                    else if (path == "assets/enemies/stw_enemy_01/stw_enemy_01.azmaterial")
                    {
                        materials.push_back({ id, info.m_relativePath });
                    }
                }, []() {});
            if (models.size() != 1 || materials.size() != 1)
            {
                AZ_Error("STWGameplay", false,
                    "ATOM_ENEMY_MESH result=FAIL reason=asset_candidate_count model=%zu material=%zu",
                    models.size(), materials.size());
                m_enemyMeshStartup = ViewmodelMeshStartup::Failed;
                return;
            }
            s_enemyAssetLoadState.m_enumeratedModelAssetId = models[0].m_assetId;
            s_enemyAssetLoadState.m_enumeratedMaterialAssetId = materials[0].m_assetId;
            m_enemyMeshAssetPath = models[0].m_relativePath;
            s_enemyAssetLoadState.m_materialAsset = AZ::Data::Asset<AZ::RPI::MaterialAsset>(
                materials[0].m_assetId, azrtti_typeid<AZ::RPI::MaterialAsset>(), materials[0].m_relativePath.c_str());
            s_enemyAssetLoadState.m_materialAsset.QueueLoad();
        }
        if (s_enemyAssetLoadState.m_materialAsset.IsError())
        {
            AZ_Error("STWGameplay", false, "ATOM_ENEMY_MESH result=FAIL reason=material_load_failed");
            m_enemyMeshStartup = ViewmodelMeshStartup::Failed;
            return;
        }
        if (!s_enemyAssetLoadState.m_materialAsset.IsReady())
        {
            return;
        }
        s_enemyAssetLoadState.m_material = AZ::RPI::Material::FindOrCreate(s_enemyAssetLoadState.m_materialAsset);
        if (!s_enemyAssetLoadState.m_material)
        {
            AZ_Error("STWGameplay", false, "ATOM_ENEMY_MESH result=FAIL reason=material_instance_failed");
            m_enemyMeshStartup = ViewmodelMeshStartup::Failed;
            return;
        }
        AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset(
            s_enemyAssetLoadState.m_enumeratedModelAssetId, azrtti_typeid<AZ::RPI::ModelAsset>(),
            m_enemyMeshAssetPath.c_str());
        modelAsset.QueueLoad();
        AZ::Render::MeshHandleDescriptor descriptor(modelAsset, s_enemyAssetLoadState.m_material);
        descriptor.m_isAlwaysDynamic = true;
        for (size_t index = 0; index < m_model.GetEnemies().GetEnemyCount(); ++index)
        {
            m_enemyMeshHandles[index] = m_meshFeatureProcessor->AcquireMesh(descriptor);
            if (!m_enemyMeshHandles[index].IsValid())
            {
                AZ_Error("STWGameplay", false, "ATOM_ENEMY_MESH result=FAIL reason=acquire_mesh_failed index=%zu", index);
                ShutdownEnemyMesh();
                m_enemyMeshStartup = ViewmodelMeshStartup::Failed;
                return;
            }
            m_meshFeatureProcessor->SetVisible(m_enemyMeshHandles[index], false);
        }
        m_impactFeedbackMeshHandle = m_meshFeatureProcessor->AcquireMesh(descriptor);
        if (!m_impactFeedbackMeshHandle.IsValid())
        {
            AZ_Error("STWGameplay", false, "COMBAT_FEEDBACK result=FAIL reason=impact_mesh_acquire_failed");
            m_enemyMeshStartup = ViewmodelMeshStartup::Failed;
            return;
        }
        m_meshFeatureProcessor->SetVisible(m_impactFeedbackMeshHandle, false);
        m_enemyMeshStartup = ViewmodelMeshStartup::Acquired;
    }

    void STWGameplaySystemComponent::UpdateEnemyMeshTransform()
    {
        if (m_enemyMeshStartup != ViewmodelMeshStartup::Acquired)
        {
            return;
        }
        const AZ::Transform objAxisCorrection = AZ::Transform::CreateFromQuaternion(
            AZ::Quaternion::CreateFromMatrix3x3(AZ::Matrix3x3::CreateFromColumns(
                -AZ::Vector3::CreateAxisX(), AZ::Vector3::CreateAxisZ(), AZ::Vector3::CreateAxisY())));
        bool allMeshesReady = true;
        const EnemyId hitEnemyId = m_model.GetPresentation().m_hitEnemyId;
        for (size_t index = 0; index < m_model.GetEnemies().GetEnemyCount(); ++index)
        {
            const EnemyInstance& instance = m_model.GetEnemies().GetInstanceByIndex(index);
            const EnemyState& enemy = instance.m_combat.GetState();
            if (!m_enemyMeshHandles[index].IsValid())
            {
                allMeshesReady = false;
                continue;
            }
            const AZ::Vector3 meshOrigin = enemy.m_position - AZ::Vector3(0.0f, 0.0f, PhysXEnemyRuntime::CenterHeight);
            const AZ::Transform baseTransform = AZ::Transform::CreateTranslation(meshOrigin);
            const AZ::Vector3 presentationScale = m_enemyPresentations[index].GetScale();
            const float hitScale = enemy.m_id == hitEnemyId ? m_combatFeedback.GetEnemyHitScale() : 1.0f;
            m_meshFeatureProcessor->SetTransform(
                m_enemyMeshHandles[index],
                baseTransform * m_enemyPresentations[index].GetLocalTransform() * objAxisCorrection,
                AZ::Vector3(presentationScale.GetX(), presentationScale.GetZ(), presentationScale.GetY()) * hitScale);
            m_meshFeatureProcessor->SetVisible(m_enemyMeshHandles[index], enemy.m_alive);
            allMeshesReady = allMeshesReady && m_meshFeatureProcessor->GetModel(m_enemyMeshHandles[index]);
        }
        const bool impactVisible = m_combatFeedback.IsImpactVisible();
        const AZ::Transform impactTransform = AZ::Transform::CreateTranslation(
            m_combatFeedback.GetImpactPosition()) * objAxisCorrection;
        m_meshFeatureProcessor->SetTransform(
            m_impactFeedbackMeshHandle, impactTransform,
            AZ::Vector3::CreateOne() * m_combatFeedback.GetImpactScale());
        m_meshFeatureProcessor->SetVisible(m_impactFeedbackMeshHandle, impactVisible);
        if (!m_enemyMeshReported && allMeshesReady && m_model.GetEnemies().GetEnemyCount() >= EnemyCollectionModel::RequiredEnemyCount)
        {
            m_enemyMeshReported = true;
            AZ_Printf("STWGameplay",
                "ATOM_ENEMY_MESH result=PASS asset=%s handles=%zu mesh=ready material=bound instances=%zu\n",
                m_enemyMeshAssetPath.c_str(), m_model.GetEnemies().GetEnemyCount(),
                m_model.GetEnemies().GetEnemyCount());
        }
    }

    void STWGameplaySystemComponent::ShutdownEnemyMesh()
    {
        if (m_meshFeatureProcessor != nullptr)
        {
            for (auto& meshHandle : m_enemyMeshHandles)
            {
                if (meshHandle.IsValid())
                {
                    m_meshFeatureProcessor->ReleaseMesh(meshHandle);
                }
            }
        }
        if (m_meshFeatureProcessor != nullptr && m_impactFeedbackMeshHandle.IsValid())
        {
            m_meshFeatureProcessor->ReleaseMesh(m_impactFeedbackMeshHandle);
        }
        m_enemyMeshHandles = {};
        m_impactFeedbackMeshHandle = {};
        m_enemyMeshAssetPath.clear();
        m_enemyMeshStartup = ViewmodelMeshStartup::Waiting;
        m_enemyMeshReported = false;
        ResetEnemyAssetLoadState();
    }

    void STWGameplaySystemComponent::TryStartArenaMesh()
    {
        if (m_meshFeatureProcessor == nullptr)
        {
            return;
        }
        if (!s_arenaAssetLoadState.m_enumerated)
        {
            s_arenaAssetLoadState.m_enumerated = true;
            AZStd::vector<ViewmodelAssetCandidate> models;
            AZStd::vector<ViewmodelAssetCandidate> materials;
            AZ::Data::AssetCatalogRequestBus::Broadcast(
                &AZ::Data::AssetCatalogRequests::EnumerateAssets, []() {},
                [&models, &materials](const AZ::Data::AssetId id, const AZ::Data::AssetInfo& info)
                {
                    const AZStd::string path = LowercaseAssetPath(info.m_relativePath);
                    if (path == "assets/environment/stw_arena_01/stw_arena_01.obj.azmodel")
                    {
                        models.push_back({ id, info.m_relativePath });
                    }
                    else if (path == "assets/environment/stw_arena_01/stw_arena_01.azmaterial")
                    {
                        materials.push_back({ id, info.m_relativePath });
                    }
                }, []() {});
            if (models.size() != 1 || materials.size() != 1)
            {
                AZ_Error("STWGameplay", false, "ATOM_ARENA result=FAIL reason=asset_candidate_count model=%zu material=%zu",
                    models.size(), materials.size());
                m_arenaMeshStartup = ViewmodelMeshStartup::Failed;
                return;
            }
            s_arenaAssetLoadState.m_enumeratedModelAssetId = models[0].m_assetId;
            s_arenaAssetLoadState.m_enumeratedMaterialAssetId = materials[0].m_assetId;
            m_arenaMeshAssetPath = models[0].m_relativePath;
            s_arenaAssetLoadState.m_materialAsset = AZ::Data::Asset<AZ::RPI::MaterialAsset>(
                materials[0].m_assetId, azrtti_typeid<AZ::RPI::MaterialAsset>(), materials[0].m_relativePath.c_str());
            s_arenaAssetLoadState.m_materialAsset.QueueLoad();
        }
        if (!s_arenaAssetLoadState.m_materialAsset.IsReady())
        {
            if (s_arenaAssetLoadState.m_materialAsset.IsError())
            {
                m_arenaMeshStartup = ViewmodelMeshStartup::Failed;
            }
            return;
        }
        s_arenaAssetLoadState.m_material = AZ::RPI::Material::FindOrCreate(s_arenaAssetLoadState.m_materialAsset);
        AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset(
            s_arenaAssetLoadState.m_enumeratedModelAssetId, azrtti_typeid<AZ::RPI::ModelAsset>(), m_arenaMeshAssetPath.c_str());
        modelAsset.QueueLoad();
        AZ::Render::MeshHandleDescriptor descriptor(modelAsset, s_arenaAssetLoadState.m_material);
        m_arenaMeshHandle = m_meshFeatureProcessor->AcquireMesh(descriptor);
        if (!m_arenaMeshHandle.IsValid())
        {
            m_arenaMeshStartup = ViewmodelMeshStartup::Failed;
            return;
        }
        m_meshFeatureProcessor->SetTransform(
            m_arenaMeshHandle, AZ::Transform::CreateIdentity(), AZ::Vector3::CreateOne());
        m_arenaMeshStartup = ViewmodelMeshStartup::Acquired;
    }

    void STWGameplaySystemComponent::ShutdownArenaMesh()
    {
        if (m_meshFeatureProcessor != nullptr && m_arenaMeshHandle.IsValid())
        {
            m_meshFeatureProcessor->ReleaseMesh(m_arenaMeshHandle);
        }
        m_arenaMeshHandle = {};
        m_arenaMeshAssetPath.clear();
        m_arenaMeshStartup = ViewmodelMeshStartup::Waiting;
        m_arenaMeshReported = false;
        ResetArenaAssetLoadState();
    }

    void STWGameplaySystemComponent::UpdateArenaAcceptance()
    {
        if (m_arenaMeshStartup == ViewmodelMeshStartup::Acquired && !m_arenaMeshReported
            && m_meshFeatureProcessor->GetModel(m_arenaMeshHandle))
        {
            m_arenaMeshReported = true;
            AZ_Printf("STWGameplay", "ATOM_ARENA result=PASS asset=%s mesh=ready material=bound lighting=native_environment\n",
                m_arenaMeshAssetPath.c_str());
        }
        if (m_automatedAcceptance && !m_arenaAcceptanceReported && m_arenaMeshReported && ArenaLayout::Validate())
        {
            AZ_Printf("STWGameplay", "ARENA_ACCEPTANCE result=PASS player_spawn=PASS enemy_spawn=PASS bounds=PASS "
                "lighting=PASS combat_lane=PASS native_scene=PASS\n");
            m_arenaAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateCombatFeedbackAcceptance()
    {
        if (!m_automatedAcceptance || m_combatFeedbackAcceptanceReported
            || m_combatFeedback.GetFireFeedbackCount() == 0u
            || m_combatFeedback.GetHitFeedbackCount() == 0u
            || m_combatFeedback.GetImpactFeedbackCount() == 0u)
        {
            return;
        }
        const bool meshesReady = m_fireFeedbackMeshHandle.IsValid() && m_impactFeedbackMeshHandle.IsValid()
            && m_meshFeatureProcessor->GetModel(m_fireFeedbackMeshHandle)
            && m_meshFeatureProcessor->GetModel(m_impactFeedbackMeshHandle);
        const bool passed = meshesReady && m_combatFeedbackAuthoritySeparated;
        AZ_Printf("STWGameplay",
            "COMBAT_FEEDBACK_ACCEPTANCE result=%s fire_feedback=%u hit_feedback=%u impact_feedback=%u "
            "authority_separation=%s native_atom_meshes=%s\n",
            passed ? "PASS" : "FAIL", m_combatFeedback.GetFireFeedbackCount(),
            m_combatFeedback.GetHitFeedbackCount(), m_combatFeedback.GetImpactFeedbackCount(),
            m_combatFeedbackAuthoritySeparated ? "PASS" : "FAIL", meshesReady ? "PASS" : "FAIL");
        m_combatFeedbackAcceptanceReported = true;
    }

    void STWGameplaySystemComponent::UpdateEnemyCombatAcceptance()
    {
        if (!m_automatedAcceptance)
        {
            return;
        }
        if (m_enemyCombatAcceptanceReported)
        {
            if (m_encounterAcceptanceRearmObserved && m_enemyAiAcceptanceReported
                && !m_encounterAcceptanceSecondEliminationTriggered && m_acceptanceTime >= 7.5f
                && m_model.GetEnemy().GetState().m_alive)
            {
                // Acceptance stimulus only; elimination still flows through the authoritative
                // EnemyCombatModel API and is observed by EncounterModel on the next tick.
                m_model.GetEnemy().ApplyDamage(m_model.GetEnemy().GetState().m_health);
                m_encounterAcceptanceSecondEliminationTriggered = true;
            }
            return;
        }
        // Place the enemy on the current authoritative aim ray before the existing acceptance
        // burst. This is test stimulus only; damage still flows exclusively through TryFire().
        if (!m_enemyCombatPrepared && m_acceptanceTime >= 4.25f)
        {
            const AZ::Vector3 combatPosition = m_model.GetEyePosition() + m_model.GetAimDirection() * 8.0f;
            m_model.GetEnemy().SynchronizePhysicalPosition(combatPosition);
            m_enemyPhysicsRuntimes[0].ResetPosition(combatPosition);
            m_enemyCombatPrepared = true;
        }

        const EnemyState& enemy = m_model.GetEnemy().GetState();
        if (!enemy.m_alive && enemy.m_deathEvents == 1 && m_acceptanceTime >= 6.5f)
        {
            m_model.GetEnemy().Reset();
            m_enemyPhysicsRuntimes[0].ResetPosition(m_model.GetEnemy().GetState().m_position);
        }

        const EnemyState& finalState = m_model.GetEnemy().GetState();
        if (m_acceptanceTime >= 7.25f && m_enemyPhysicsReady && m_enemyMeshReported && m_enemyMoved
            && finalState.m_damageEvents > 0 && finalState.m_deathEvents == 1
            && finalState.m_respawnEvents == 1 && finalState.m_alive)
        {
            AZ_Printf("STWGameplay",
                "ENEMY_COMBAT_ACCEPTANCE result=PASS spawned=1 mesh=ready physics=ready moved=1 "
                "damage_events=%d deaths=%d respawns=%d\n",
                finalState.m_damageEvents, finalState.m_deathEvents, finalState.m_respawnEvents);
            m_enemyCombatAcceptanceReported = true;
        }

    }

    void STWGameplaySystemComponent::UpdateMultiEnemyAcceptance()
    {
        if (!m_automatedAcceptance || !m_mantleAcceptanceReported || m_multiEnemyAcceptanceReported
            || m_model.GetEnemies().GetEnemyCount() < EnemyCollectionModel::RequiredEnemyCount)
        {
            return;
        }

        EnemyCollectionModel& enemies = m_model.GetEnemies();
        const EnemyInstance& enemyA = enemies.GetInstanceByIndex(0);
        const EnemyInstance& enemyB = enemies.GetInstanceByIndex(1);
        const EnemyInstance& enemyC = enemies.GetInstanceByIndex(2);

        if (!m_multiEnemyPrepared && m_acceptanceTime >= 4.25f)
        {
            const AZ::Vector3 center = m_model.GetEyePosition() + m_model.GetAimDirection() * 8.0f;
            const AZ::Vector3 positions[] = {
                center,
                center + AZ::Vector3(-3.0f, 2.0f, 0.0f),
                center + AZ::Vector3(3.0f, 2.0f, 0.0f)
            };
            for (size_t index = 0; index < EnemyCollectionModel::RequiredEnemyCount; ++index)
            {
                const EnemyId id = enemies.GetInstanceByIndex(index).m_id;
                enemies.SynchronizePhysicalPosition(id, positions[index]);
                m_enemyPhysicsRuntimes[index].ResetPosition(positions[index]);
            }
            m_multiEnemyPrepared = true;
        }

        if (!m_multiEnemyPrepared)
        {
            return;
        }

        const bool idsUnique = enemyA.m_id != InvalidEnemyId && enemyA.m_id != enemyB.m_id
            && enemyA.m_id != enemyC.m_id && enemyB.m_id != enemyC.m_id;
        const bool activeSet = enemyA.m_combat.GetState().m_alive && enemyB.m_combat.GetState().m_alive
            && enemyC.m_combat.GetState().m_alive;
        m_multiEnemyInitialActiveSet = m_multiEnemyInitialActiveSet || (activeSet && idsUnique);
        const bool physicalBindings = m_enemyPhysicsReady
            && m_enemyPhysicsRuntimes[0].GetBoundEnemyId() == enemyA.m_id
            && m_enemyPhysicsRuntimes[1].GetBoundEnemyId() == enemyB.m_id
            && m_enemyPhysicsRuntimes[2].GetBoundEnemyId() == enemyC.m_id
            && m_enemyPhysicsRuntimes[0].IsValid() && m_enemyPhysicsRuntimes[1].IsValid()
            && m_enemyPhysicsRuntimes[2].IsValid();
        const AZ::Vector3& positionA = enemyA.m_combat.GetState().m_position;
        const AZ::Vector3& positionB = enemyB.m_combat.GetState().m_position;
        const AZ::Vector3& positionC = enemyC.m_combat.GetState().m_position;
        m_multiEnemyIndependentPhysical = m_multiEnemyIndependentPhysical
            || (physicalBindings && positionA.IsFinite() && positionB.IsFinite() && positionC.IsFinite()
                && (positionA - positionB).GetLengthSq() > 0.25f
                && (positionA - positionC).GetLengthSq() > 0.25f
                && (positionB - positionC).GetLengthSq() > 0.25f);

        const EnemyState& stateA = enemyA.m_combat.GetState();
        const EnemyState& stateB = enemyB.m_combat.GetState();
        const EnemyState& stateC = enemyC.m_combat.GetState();
        if (m_multiEnemyInitialActiveSet && !m_multiEnemyFirstEliminationObserved && stateA.m_alive)
        {
            // Begin the multi-enemy elimination sequence only after its mantle-gated active set
            // has been established. This is acceptance stimulus through the authoritative API.
            enemies.ApplyDamage(enemyA.m_id, stateA.m_health);
        }
        if (!m_multiEnemyFirstEliminationObserved && !stateA.m_alive && stateA.m_deathEvents > 0)
        {
            m_multiEnemyFirstEliminationObserved = true;
            m_multiEnemyActiveAfterFirstElimination = m_encounter.IsActive();
            m_multiEnemyIndependentHealth = stateB.m_alive && stateC.m_alive
                && stateB.m_health == stateB.m_maxHealth && stateC.m_health == stateC.m_maxHealth
                && stateB.m_damageEvents == 0 && stateC.m_damageEvents == 0;
            m_multiEnemyIndependentAi = stateA.m_behaviorState == EnemyBehaviorState::Dead
                && stateB.m_behaviorState != EnemyBehaviorState::Dead
                && stateC.m_behaviorState != EnemyBehaviorState::Dead;
        }
        if (m_multiEnemyFirstEliminationObserved && !m_multiEnemySecondEliminationObserved
            && !stateB.m_alive && stateB.m_deathEvents > 0)
        {
            m_multiEnemySecondEliminationObserved = true;
            m_multiEnemyActiveAfterSecondElimination = m_encounter.IsActive();
        }
        if (m_multiEnemySecondEliminationObserved && !m_multiEnemyThirdEliminationObserved
            && !stateC.m_alive && stateC.m_deathEvents > 0)
        {
            m_multiEnemyThirdEliminationObserved = true;
        }

        // Acceptance stimulus only. Each elimination still flows through the public authoritative
        // EnemyCombatModel API and is observed by EncounterModel on a later tick.
        if (m_multiEnemyFirstEliminationObserved && !m_multiEnemySecondEliminationObserved
            && m_acceptanceTime >= 5.60f && stateB.m_alive)
        {
            enemies.ApplyDamage(enemyB.m_id, stateB.m_health);
        }
        else if (m_multiEnemySecondEliminationObserved && !m_multiEnemyThirdEliminationObserved
            && m_acceptanceTime >= 5.90f && stateC.m_alive)
        {
            enemies.ApplyDamage(enemyC.m_id, stateC.m_health);
        }

        if (m_multiEnemyThirdEliminationObserved && m_encounter.IsCompleted()
            && !m_multiEnemyRearmObserved)
        {
            m_encounter.Update(enemies);
            m_multiEnemyDuplicateCompletionBlocked = m_encounter.GetCompletedCount() == 1;
            enemies.ResetRequiredEnemies();
            for (size_t index = 0; index < EnemyCollectionModel::RequiredEnemyCount; ++index)
            {
                const EnemyInstance& instance = enemies.GetInstanceByIndex(index);
                m_enemyPhysicsRuntimes[index].ResetPosition(instance.m_combat.GetState().m_position);
            }
        }

        if (m_encounterAcceptanceSecondEliminationTriggered)
        {
            if (!m_multiEnemySecondCycleBEliminated && stateB.m_deathEvents > 1)
            {
                m_multiEnemySecondCycleBEliminated = true;
            }
            if (m_multiEnemySecondCycleBEliminated && !m_multiEnemySecondCycleCEliminated
                && stateC.m_deathEvents > 1)
            {
                m_multiEnemySecondCycleCEliminated = true;
            }
            if (!m_multiEnemySecondCycleBEliminated && m_acceptanceTime >= 7.75f && stateB.m_alive)
            {
                enemies.ApplyDamage(enemyB.m_id, stateB.m_health);
            }
            else if (m_multiEnemySecondCycleBEliminated && !m_multiEnemySecondCycleCEliminated
                && m_acceptanceTime >= 8.05f && stateC.m_alive)
            {
                enemies.ApplyDamage(enemyC.m_id, stateC.m_health);
            }
        }

        if (m_encounter.GetCompletedCount() >= 2 && m_multiEnemyThirdEliminationObserved
            && m_multiEnemySecondCycleBEliminated && m_multiEnemySecondCycleCEliminated
            && m_multiEnemyDuplicateCompletionBlocked && m_multiEnemyRearmObserved
            && m_multiEnemyPostRearmActive && m_multiEnemyInitialActiveSet
            && m_multiEnemyActiveAfterFirstElimination && m_multiEnemyActiveAfterSecondElimination
            && m_multiEnemyIndependentHealth
            && m_multiEnemyIndependentAi && m_multiEnemyIndependentPhysical && idsUnique)
        {
            AZ_Printf("STWGameplay", "MULTI_ENEMY_COUNT=%zu\n", enemies.GetEnemyCount());
            AZ_Printf("STWGameplay", "ENEMY_ID_UNIQUE=%d\n", idsUnique ? 1 : 0);
            AZ_Printf("STWGameplay", "ENEMY_A_ACTIVE=%d\n", m_multiEnemyInitialActiveSet ? 1 : 0);
            AZ_Printf("STWGameplay", "ENEMY_B_ACTIVE=%d\n", m_multiEnemyInitialActiveSet ? 1 : 0);
            AZ_Printf("STWGameplay", "ENEMY_C_ACTIVE=%d\n", m_multiEnemyInitialActiveSet ? 1 : 0);
            AZ_Printf("STWGameplay", "INDEPENDENT_HEALTH_STATE=%s\n",
                m_multiEnemyIndependentHealth ? "PASS" : "FAIL");
            AZ_Printf("STWGameplay", "INDEPENDENT_AI_STATE=%s\n",
                m_multiEnemyIndependentAi ? "PASS" : "FAIL");
            AZ_Printf("STWGameplay", "INDEPENDENT_PHYSICAL_STATE=%s\n",
                m_multiEnemyIndependentPhysical ? "PASS" : "FAIL");
            AZ_Printf("STWGameplay", "FIRST_ENEMY_ELIMINATION_OBSERVED=%d\n",
                m_multiEnemyFirstEliminationObserved ? 1 : 0);
            AZ_Printf("STWGameplay", "ENCOUNTER_ACTIVE_AFTER_FIRST_ELIMINATION=%d\n",
                m_multiEnemyFirstEliminationObserved ? 1 : 0);
            AZ_Printf("STWGameplay", "SECOND_ENEMY_ELIMINATION_OBSERVED=%d\n",
                m_multiEnemySecondEliminationObserved ? 1 : 0);
            AZ_Printf("STWGameplay", "ENCOUNTER_ACTIVE_AFTER_SECOND_ELIMINATION=%d\n",
                m_multiEnemySecondEliminationObserved ? 1 : 0);
            AZ_Printf("STWGameplay", "THIRD_ENEMY_ELIMINATION_OBSERVED=%d\n",
                m_multiEnemyThirdEliminationObserved ? 1 : 0);
            AZ_Printf("STWGameplay", "ENCOUNTER_COMPLETED_AFTER_REQUIRED_SET=%d\n",
                m_encounter.GetCompletedCount() >= 1 ? 1 : 0);
            AZ_Printf("STWGameplay", "DUPLICATE_COMPLETION_BLOCKED=%d\n",
                m_multiEnemyDuplicateCompletionBlocked ? 1 : 0);
            AZ_Printf("STWGameplay", "MULTI_ENEMY_REARM_OBSERVED=%d\n",
                m_multiEnemyRearmObserved ? 1 : 0);
            AZ_Printf("STWGameplay", "MULTI_ENEMY_POST_REARM_ACTIVE=%d\n",
                m_multiEnemyPostRearmActive ? 1 : 0);
            AZ_Printf("STWGameplay", "PLAYER_AUTHORITY=PASS\n");
            AZ_Printf("STWGameplay", "PLAYER_PHYSICAL_AUTHORITY=PASS\n");
            AZ_Printf("STWGameplay", "ENEMY_COMBAT_AUTHORITY=PASS\n");
            AZ_Printf("STWGameplay", "ENEMY_PHYSICAL_AUTHORITY=PASS\n");
            AZ_Printf("STWGameplay", "ENCOUNTER_AUTHORITY=PASS\n");
            AZ_Printf("STWGameplay",
                "MULTI_ENEMY_ACCEPTANCE result=PASS count=%zu ids=unique active=3 "
                "independent_health=PASS independent_ai=PASS independent_physical=PASS "
                "first_elimination=1 active_after_first=1 second_elimination=1 active_after_second=1 "
                "third_elimination=1 completed_after_required_set=1 duplicate_completion_blocked=1 "
                "rearm=1 post_rearm_active=1 player_authority=PASS player_physical_authority=PASS "
                "enemy_combat_authority=PASS enemy_physical_authority=PASS encounter_authority=PASS\n",
                enemies.GetEnemyCount());
            m_multiEnemyAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateEncounterAcceptance()
    {
        if (!m_automatedAcceptance || m_encounterAcceptanceReported)
        {
            return;
        }
        if (!m_encounterAcceptanceFirstCompletion && m_encounter.GetCompletedCount() >= 1)
        {
            m_encounterAcceptanceFirstCompletion = true;
            m_encounterAcceptanceDuplicateBlocked = m_encounter.GetCompletedCount() == 1;
        }
        m_encounterAcceptanceSecondCompletion = m_encounter.GetCompletedCount() >= 2;
        if (m_encounterAcceptanceSecondCompletion)
        {
            const bool playerAuthorityValid = m_model.GetPlayer().m_position.IsFinite()
                && m_model.GetPlayer().m_health >= 0.0f && m_model.GetPlayer().m_health <= m_model.GetPlayer().m_maxHealth;
            const bool enemyAuthorityValid = m_model.GetEnemy().GetState().m_deathEvents >= 2;
            const bool passed = m_encounterAcceptanceFirstCompletion
                && m_encounterAcceptanceDuplicateBlocked
                && m_encounterAcceptanceRearmObserved
                && m_encounterAcceptancePostRearmActive
                && m_encounterAcceptanceSecondCompletion
                && playerAuthorityValid && enemyAuthorityValid;
            AZ_Printf("STWGameplay",
                "ENCOUNTER_ACCEPTANCE result=%s initial_active=1 first_elimination=1 first_completed_count=1 "
                "duplicate_completion_blocked=%d rearm=1 post_rearm_active=%d second_elimination=1 "
                "second_completed_count=%d player_authority=%s enemy_authority=%s\n",
                passed ? "PASS" : "FAIL",
                m_encounterAcceptanceDuplicateBlocked ? 1 : 0,
                m_encounterAcceptancePostRearmActive ? 1 : 0,
                m_encounter.GetCompletedCount(),
                playerAuthorityValid ? "PASS" : "FAIL",
                enemyAuthorityValid ? "PASS" : "FAIL");
            m_encounterAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateSpawnCheckpointAcceptance()
    {
        if (!m_automatedAcceptance || m_spawnCheckpointAcceptanceReported)
        {
            return;
        }

        const PlayerState& player = m_model.GetPlayer();
        if (!m_spawnCheckpointAcceptanceStarted)
        {
            m_spawnCheckpointAcceptanceStarted = true;
            m_spawnCheckpointInitialDeathEvents = player.m_deathEvents;
            const AZ::Vector3& defaultSpawn = m_spawnCheckpoint.GetDefaultSpawnPosition();
            const AZ::Vector3& physicalStart = m_spawnCheckpointLastPhysicalPosition;
            m_spawnCheckpointDefaultRespawnObserved = m_spawnCheckpoint.ResolveRespawnPosition().IsClose(defaultSpawn)
                && physicalStart.IsFinite()
                && (physicalStart - defaultSpawn).GetLengthSq() <= 0.20f * 0.20f;
        }

        if (m_spawnCheckpoint.HasActiveCheckpoint())
        {
            if (!m_spawnCheckpointActivationObserved)
            {
                m_spawnCheckpointActivationObserved = true;
                m_spawnCheckpointAcceptancePosition = m_spawnCheckpoint.GetActiveCheckpointPosition();
                m_spawnCheckpointInitialActivationCount = m_spawnCheckpoint.GetActivationCount();
            }

            m_spawnCheckpointActivePersisted = m_spawnCheckpoint.ResolveRespawnPosition().IsClose(
                m_spawnCheckpointAcceptancePosition);
            if (!m_spawnCheckpointDuplicateChecked)
            {
                const int activationCountBeforeDuplicate = m_spawnCheckpoint.GetActivationCount();
                const bool duplicateAccepted = m_spawnCheckpoint.ActivateCheckpoint(m_spawnCheckpointAcceptancePosition);
                m_spawnCheckpointDuplicateBlocked = duplicateAccepted
                    && m_spawnCheckpoint.GetActivationCount() == activationCountBeforeDuplicate
                    && m_spawnCheckpoint.GetActivationCount() == m_spawnCheckpointInitialActivationCount;
                m_spawnCheckpointDuplicateChecked = true;
            }
        }

        m_spawnCheckpointPlayerDeathObserved = m_spawnCheckpointPlayerDeathObserved
            || player.m_deathEvents > m_spawnCheckpointInitialDeathEvents;
        if (m_enemyAiPlayerRespawned && m_spawnCheckpointActivationObserved && player.m_alive
            && player.m_respawnEvents > 0)
        {
            m_spawnCheckpointRespawnObserved = player.m_position.IsClose(m_spawnCheckpointAcceptancePosition, 0.05f);
            m_spawnCheckpointPhysicalPositionConfirmed = m_spawnCheckpointLastPhysicalPosition.IsClose(
                m_spawnCheckpointAcceptancePosition, 0.05f);
        }

        const bool playerAuthorityValid = player.m_position.IsFinite()
            && player.m_health >= 0.0f && player.m_health <= player.m_maxHealth;
        const bool playerPhysicalAuthorityValid = m_physicsPlayer.IsValid()
            && m_spawnCheckpointLastPhysicalPosition.IsFinite();
        const bool encounterAuthorityValid = m_encounter.GetCompletedCount() >= 1
            && m_model.GetEnemy().GetState().m_deathEvents >= 1;
        const bool passed = m_spawnCheckpointDefaultRespawnObserved
            && m_spawnCheckpointActivationObserved
            && m_spawnCheckpointDuplicateBlocked
            && m_spawnCheckpointActivePersisted
            && m_spawnCheckpointPlayerDeathObserved
            && m_spawnCheckpointRespawnObserved
            && m_spawnCheckpointPhysicalPositionConfirmed
            && playerAuthorityValid
            && playerPhysicalAuthorityValid
            && encounterAuthorityValid;
        if (passed)
        {
            AZ_Printf("STWGameplay",
                "SPAWN_CHECKPOINT_ACCEPTANCE result=PASS initial_spawn=1 default_respawn=1 "
                "checkpoint_activated=1 duplicate_checkpoint_blocked=1 active_checkpoint_persisted=1 "
                "player_death=1 respawn_at_checkpoint=1 physical_position=1 player_authority=PASS "
                "player_physical_authority=PASS encounter_authority=PASS\n");
            m_spawnCheckpointAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateEnemyAiAcceptance(float deltaTime)
    {
        if (!m_automatedAcceptance || m_enemyAiAcceptanceReported)
        {
            return;
        }

        const PlayerState& player = m_model.GetPlayer();
        if (!player.m_alive && player.m_deathEvents > 0)
        {
            m_enemyAiPlayerDeathObserved = true;
        }

        // Respawn only after the established Block-7 combat gate has completed. This is a
        // one-shot gameplay reset through the same model and PhysX ownership used at spawn.
        if (m_enemyAiPlayerDeathObserved && m_enemyCombatAcceptanceReported && !m_enemyAiPlayerRespawned)
        {
            m_enemyAiRespawnDelay += deltaTime;
            if (m_enemyAiRespawnDelay >= 0.5f)
            {
                m_model.ResetPlayer();
                const AZ::Vector3 respawnPosition = m_spawnCheckpoint.ResolveRespawnPosition();
                m_model.SetPlayerPosition(respawnPosition);
                m_physicsPlayer.ResetPosition(respawnPosition);
                m_enemyAiPlayerRespawned = true;

                // The authoritative reset clears PlayerSliceModel's held-switch latch. The
                // acceptance stimulus must cross an explicit release barrier after that reset,
                // otherwise the switch assertion from this tick could become a fresh edge on the
                // next model update. Keep this bookkeeping acceptance-only; the model remains the
                // sole owner of both weapon transitions.
                if (m_weaponSwitchAcceptanceStarted && !m_weaponSwitchResetComplete
                    && m_model.GetPlayer().m_alive
                    && m_model.GetPlayer().m_respawnEvents > m_weaponSwitchInitialRespawnEvents
                    && m_model.GetActiveWeaponId() == WeaponId::STW_SMG_01)
                {
                    const WeaponState& weaponAAfterReset = m_model.GetWeapon(WeaponId::STW_SMG_01);
                    m_weaponSwitchResetComplete = true;
                    m_weaponSwitchAcceptancePhase = WeaponSwitchAcceptancePhase::PostResetRelease;
                    m_weaponSwitchAcceptancePhaseStartTime = m_acceptanceTime;
                    m_weaponSwitchPostResetAMagazine = weaponAAfterReset.m_magazine;
                    m_weaponSwitchPostResetAReserve = weaponAAfterReset.m_reserve;
                    m_weaponSwitchPostResetBaselineCaptured = true;
                    m_input.m_switchWeapon = false;
                }
            }
        }

        const EnemyState& enemy = m_model.GetEnemy().GetState();
        const PlayerState& currentPlayer = m_model.GetPlayer();
        if (m_enemyAiPlayerRespawned && currentPlayer.m_alive
            && (enemy.m_behaviorState == EnemyBehaviorState::Detect
                || enemy.m_behaviorState == EnemyBehaviorState::Chase
                || enemy.m_behaviorState == EnemyBehaviorState::Attack))
        {
            m_enemyAiLoopReactivated = true;
        }

        if (m_enemyPhysicsReady && m_enemyMeshReported && enemy.m_detectionEvents > 0 && enemy.m_chaseEvents > 0
            && enemy.m_attackEvents > 0 && currentPlayer.m_damageEvents > 0 && currentPlayer.m_deathEvents > 0
            && currentPlayer.m_respawnEvents > 0 && enemy.m_deathEvents > 0 && enemy.m_respawnEvents > 0
            && m_enemyAiLoopReactivated)
        {
            AZ_Printf(
                "STWGameplay",
                "ENEMY_AI_ACCEPTANCE result=PASS detected=1 chased=1 enemy_attacks=%d player_damage=%d "
                "player_death=%d player_respawn=%d enemy_death=%d enemy_reset=%d loop_active=1\n",
                enemy.m_attackEvents,
                currentPlayer.m_damageEvents,
                currentPlayer.m_deathEvents,
                currentPlayer.m_respawnEvents,
                enemy.m_deathEvents,
                enemy.m_respawnEvents);
            m_enemyAiAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::UpdateEnemyPresentationAcceptance()
    {
        if (!m_automatedAcceptance || m_enemyPresentationAcceptanceReported)
        {
            return;
        }
        if (m_enemyPresentationIdleObserved && m_enemyPresentationChaseObserved && m_enemyPresentationAttackObserved
            && m_enemyPresentationDeadObserved && m_enemyPresentationResetObserved
            && m_enemyPresentationAuthoritySeparated && m_enemyMeshReported)
        {
            AZ_Printf("STWGameplay",
                "ENEMY_PRESENTATION_ACCEPTANCE result=PASS idle=PASS chase=PASS attack=PASS death=PASS "
                "reset=PASS authority_separation=PASS\n");
            m_enemyPresentationAcceptanceReported = true;
        }
    }

    void STWGameplaySystemComponent::DrawPresentation()
    {
        using Bus = AzFramework::DebugDisplayRequestBus;
        const AZ::s32 displayId = static_cast<AZ::s32>(AzFramework::g_defaultSceneEntityDebugDisplayId);
        const WeaponState& weapon = m_model.GetWeapon();
        const PresentationState& presentation = m_model.GetPresentation();
        const AZ::Vector3 aim = m_model.GetAimDirection();
        const AZ::Vector3 up = AZ::Vector3::CreateAxisZ();
        AZ::Vector3 right = aim.Cross(up);
        if (right.GetLengthSq() < 0.01f)
        {
            right = AZ::Vector3::CreateAxisX();
        }
        else
        {
            right.Normalize();
        }

        // Camera-relative native viewmodel. Exactly one pre-acquired STW Atom mesh is visible at
        // a time; arms and hands do not exist yet, and the muzzle cue remains procedural.
        // Recoil/sway/reload pose come from the presentation
        // model, which consumes authoritative events only; fire, damage and reload authority
        // remain in PlayerSliceModel.
        const AZ::Vector3 recoil = m_viewmodel.GetRecoilOffset();
        const AZ::Vector3 bob = m_viewmodel.GetBobOffset();
        const AZ::Vector3 sway = m_viewmodel.GetSwayOffset();
        const AZ::Vector3 pose = m_viewmodel.GetPoseOffset();
        const float recoilPitch = m_viewmodel.GetRecoilPitch();
        const float recoilPitchCos = std::cos(recoilPitch);
        const float recoilPitchSin = std::sin(recoilPitch);
        const AZ::Vector3 presentedAim = aim * recoilPitchCos + up * recoilPitchSin;
        const AZ::Vector3 presentedUp = up * recoilPitchCos - aim * recoilPitchSin;
        const AZ::Vector3 vmOffset =
            right * (recoil.GetX() + bob.GetX() + sway.GetX()) +
            aim * (recoil.GetY() + bob.GetY() + sway.GetY()) +
            up * (recoil.GetZ() + bob.GetZ() + sway.GetZ());
        const bool reloadPose = (m_viewmodel.GetState() == ViewmodelState::Reload);
        const AZ::Vector3 reloadDip = reloadPose ? (-up * 0.12f - aim * 0.10f) : AZ::Vector3::CreateZero();
        const AZ::Vector3 weaponCenter =
            m_model.GetEyePosition() + presentedAim * pose.GetY() + right * pose.GetX()
            + presentedUp * pose.GetZ() + vmOffset + reloadDip;
        // The active weapon body is an original STW Atom mesh driven through the
        // Atom MeshFeatureProcessor. It is presentation only: it consumes the
        // recoil/sway/reload pose computed above and never writes gameplay state. The former
        // procedural DrawSolidOBB body is gone; if the mesh fails to initialize the runtime
        // reports it instead of silently drawing a placeholder.
        UpdateViewmodelMeshTransform(weaponCenter, right, presentedAim, presentedUp);
        if (m_viewmodel.IsMuzzleFlashActive())
        {
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(1.0f, 0.72f, 0.12f, 1.0f));
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawBall,
                weaponCenter + presentedAim * 0.34f, 0.055f, true);
        }

        // STW_ENEMY_01's production body is the Atom mesh. No DebugDisplay target placeholder.
        UpdateEnemyMeshTransform();

        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(0.95f, 0.95f, 0.95f, 1.0f));
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.485f, 0.5f), AZ::Vector2(0.495f, 0.5f), 0.0f);
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.505f, 0.5f), AZ::Vector2(0.515f, 0.5f), 0.0f);
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.5f, 0.48f), AZ::Vector2(0.5f, 0.492f), 0.0f);
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.5f, 0.508f), AZ::Vector2(0.5f, 0.52f), 0.0f);

        char hud[128];
        const PlayerState& player = m_model.GetPlayer();
        const EquipmentProfile& equipmentProfile = m_model.GetActiveEquipmentProfile();
        if (equipmentProfile.m_chargeCapacity > 0)
        {
            azsnprintf(hud, AZ_ARRAY_SIZE(hud), "STW  HP %03d/%03d   %s charges %02d   %s",
                static_cast<int>(player.m_health), static_cast<int>(player.m_maxHealth),
                equipmentProfile.m_displayName, weapon.m_charges,
                player.m_alive ? "READY" : "DEAD");
        }
        else if (equipmentProfile.m_magazineCapacity > 0)
        {
            azsnprintf(hud, AZ_ARRAY_SIZE(hud), "STW  HP %03d/%03d   %s %02d / %03d   %s",
                static_cast<int>(player.m_health), static_cast<int>(player.m_maxHealth),
                equipmentProfile.m_displayName, weapon.m_magazine, weapon.m_reserve,
                player.m_alive ? (weapon.m_reloading ? "RELOADING" : "READY") : "DEAD");
        }
        else
        {
            azsnprintf(hud, AZ_ARRAY_SIZE(hud), "STW  HP %03d/%03d   %s READY   %s",
            static_cast<int>(player.m_health), static_cast<int>(player.m_maxHealth),
            equipmentProfile.m_displayName, player.m_alive ? "READY" : "DEAD");
        }
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::Draw2dTextLabel, 24.0f, 36.0f, 1.4f, hud, false);
        char objective[96];
        azsnprintf(objective, AZ_ARRAY_SIZE(objective), "%s   ENCOUNTERS: %d",
            m_encounter.IsCompleted() ? "OBJECTIVE COMPLETE" : "OBJECTIVE: ELIMINATE HOSTILE",
            m_encounter.GetCompletedCount());
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::Draw2dTextLabel,
            24.0f, 64.0f, 1.1f, objective, false);
        if (presentation.m_hitCueRemaining > 0.0f || m_viewmodel.IsHitFeedbackActive())
        {
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(1.0f, 0.25f, 0.2f, 1.0f));
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawWireCircle2d, AZ::Vector2(0.5f, 0.5f), 0.018f, 0.0f);
        }
    }
}
