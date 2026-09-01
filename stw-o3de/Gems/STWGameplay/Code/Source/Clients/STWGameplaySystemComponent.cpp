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

        ViewmodelAssetLoadState s_viewmodelAssetLoadState;

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
            s_viewmodelAssetLoadState = {};
        }
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
        m_adsHeld = false;
        if (const char* capturePath = std::getenv("STW_NATIVE_CAPTURE_PATH"); capturePath && capturePath[0] != '\0')
        {
            m_nativeCapturePath = capturePath;
        }
        m_automatedAcceptance = std::getenv("STW_PHYSX_ACCEPTANCE") != nullptr;
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
        ShutdownViewmodelMesh();
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
        m_physicsStartup = PhysicsStartup::Ready;
        AZ_Printf("STWGameplay", "Native Player Movement V2 PhysX active\n");
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

        UpdateAutomatedAcceptance(deltaTime);
        m_model.Update(deltaTime, m_input);
        m_physicsPlayer.QueueVelocity(m_model.GetDesiredVelocity(m_input));
        AZ::Vector3 physicalPosition = AZ::Vector3::CreateZero();
        bool grounded = false;
        if (m_physicsPlayer.Synchronize(physicalPosition, grounded))
        {
            m_model.SynchronizePhysicalState(physicalPosition, grounded);
        }

        // Presentation reacts to authoritative events/state only (read-only). It never writes
        // ammo, damage, reload completion, target health or player movement back.
        PresentationInput vpInput;
        vpInput.m_shotFired = m_model.GetPresentation().m_shotFired;
        vpInput.m_hit = m_model.GetPresentation().m_hit;
        vpInput.m_reloading = m_model.GetWeapon().m_reloading;
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
        RecordPerformance(deltaTime);

        // Production runs do not set STW_NATIVE_CAPTURE_PATH. The controlled
        // native verification job uses it to request one genuine Atom/RHI
        // readback after the scene and presentation have had time to render.
        if (!m_nativeCapturePath.empty() && !m_nativeCaptureAttempted)
        {
            m_nativeCaptureDelay += deltaTime;
            if (m_nativeCaptureDelay >= 8.0f)
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
        if (!m_automatedAcceptance || (m_viewmodelAcceptanceReported && m_adsAcceptanceReported)
            || !std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            return;
        }
        m_acceptanceTime += deltaTime;

        // Verification-only stimulus. It feeds the same m_adsHeld state that the real
        // Mouse::Button::Right path writes; it is disabled unless the existing acceptance mode
        // is explicitly enabled and uses simulation time, never wall-clock sleeps.
        m_adsHeld = m_acceptanceTime >= 1.0f && m_acceptanceTime < 6.0f;
        m_input.m_forward = 0.0f;
        m_input.m_strafe = 0.0f;
        m_input.m_sprint = false;
        m_input.m_fire = false;
        m_input.m_lookX = 0.0f;
        m_input.m_lookY = 0.0f;

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
        else if (m_acceptanceTime >= 7.5f)
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
        else if (id == Keyboard::Key::AlphanumericR && channel.IsStateBegan()) { m_input.m_reload = true; }
        else if (id == Mouse::Button::Left) { m_input.m_fire = active; }
        else if (id == Mouse::Button::Right) { m_adsHeld = active; }
        else if (id == Mouse::Movement::X) { m_input.m_lookX += channel.GetValue(); }
        else if (id == Mouse::Movement::Y) { m_input.m_lookY += channel.GetValue(); }
        return false;
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
        AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(yaw * pitch, m_model.GetEyePosition());
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

        if (!s_viewmodelAssetLoadState.m_enumerated)
        {
            s_viewmodelAssetLoadState.m_enumerated = true;
            AZStd::vector<ViewmodelAssetCandidate> modelCandidates;
            AZStd::vector<ViewmodelAssetCandidate> materialCandidates;
            AZ::Data::AssetCatalogRequestBus::Broadcast(
                &AZ::Data::AssetCatalogRequests::EnumerateAssets,
                []() {},
                [&modelCandidates, &materialCandidates](const AZ::Data::AssetId assetId, const AZ::Data::AssetInfo& info)
                {
                    const AZStd::string lowercasePath = LowercaseAssetPath(info.m_relativePath);
                    // Accept only the two canonical STW_SMG_01 product paths from the O3DE asset
                    // cache; any other product is ignored so the wrong asset can never be bound.
                    if (lowercasePath == "assets/weapons/stw_smg_01/stw_smg_01.obj.azmodel")
                    {
                        modelCandidates.push_back({ assetId, info.m_relativePath });
                    }
                    else if (lowercasePath == "assets/weapons/stw_smg_01/stw_smg_01.azmaterial")
                    {
                        materialCandidates.push_back({ assetId, info.m_relativePath });
                    }
                },
                []() {});

            if (modelCandidates.size() != 1 || materialCandidates.size() != 1)
            {
                AZ_Error(
                    "STWGameplay", false,
                    "ATOM_VIEWMODEL_MESH result=FAIL reason=asset_candidate_count model=%zu material=%zu",
                    modelCandidates.size(), materialCandidates.size());
                m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                return;
            }

            s_viewmodelAssetLoadState.m_enumeratedModelAssetId = modelCandidates[0].m_assetId;
            s_viewmodelAssetLoadState.m_enumeratedMaterialAssetId = materialCandidates[0].m_assetId;
            m_viewmodelMeshAssetPath = modelCandidates[0].m_relativePath;

            AZ::Data::AssetId resolvedModelAssetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                resolvedModelAssetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath,
                modelCandidates[0].m_relativePath.c_str(), AZ::Data::AssetType{}, false);
            AZ::Data::AssetId resolvedMaterialAssetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                resolvedMaterialAssetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath,
                materialCandidates[0].m_relativePath.c_str(), AZ::Data::AssetType{}, false);

            if (!resolvedModelAssetId.IsValid() || !resolvedMaterialAssetId.IsValid()
                || resolvedModelAssetId != s_viewmodelAssetLoadState.m_enumeratedModelAssetId
                || resolvedMaterialAssetId != s_viewmodelAssetLoadState.m_enumeratedMaterialAssetId)
            {
                AZ_Error(
                    "STWGameplay", false,
                    "ATOM_VIEWMODEL_MESH result=FAIL reason=catalog_resolution_mismatch model=%s material=%s",
                    modelCandidates[0].m_relativePath.c_str(), materialCandidates[0].m_relativePath.c_str());
                m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
                return;
            }

            s_viewmodelAssetLoadState.m_materialAsset = AZ::Data::Asset<AZ::RPI::MaterialAsset>(
                resolvedMaterialAssetId,
                azrtti_typeid<AZ::RPI::MaterialAsset>(),
                materialCandidates[0].m_relativePath.c_str());
            s_viewmodelAssetLoadState.m_materialAsset.QueueLoad();
        }

        if (s_viewmodelAssetLoadState.m_materialAsset.IsError())
        {
            AZ_Error("STWGameplay", false,
                "ATOM_VIEWMODEL_MESH result=FAIL reason=material_load_failed asset=%s",
                m_viewmodelMeshAssetPath.c_str());
            m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
            return;
        }
        if (!s_viewmodelAssetLoadState.m_materialAsset.IsReady())
        {
            return;
        }

        s_viewmodelAssetLoadState.m_material =
            AZ::RPI::Material::FindOrCreate(s_viewmodelAssetLoadState.m_materialAsset);
        if (!s_viewmodelAssetLoadState.m_material)
        {
            AZ_Error("STWGameplay", false,
                "ATOM_VIEWMODEL_MESH result=FAIL reason=material_instance_failed asset=%s",
                m_viewmodelMeshAssetPath.c_str());
            m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
            return;
        }

        AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset(
            s_viewmodelAssetLoadState.m_enumeratedModelAssetId,
            azrtti_typeid<AZ::RPI::ModelAsset>(),
            m_viewmodelMeshAssetPath.c_str());
        modelAsset.QueueLoad();

        AZ::Render::MeshHandleDescriptor descriptor(modelAsset, s_viewmodelAssetLoadState.m_material);
        descriptor.m_isAlwaysDynamic = true; // the viewmodel follows the camera every frame
        m_viewmodelMeshHandle = m_meshFeatureProcessor->AcquireMesh(descriptor);
        if (!m_viewmodelMeshHandle.IsValid())
        {
            AZ_Error("STWGameplay", false,
                "ATOM_VIEWMODEL_MESH result=FAIL reason=acquire_mesh_failed asset=%s",
                m_viewmodelMeshAssetPath.c_str());
            m_viewmodelMeshStartup = ViewmodelMeshStartup::Failed;
            return;
        }
        m_viewmodelMeshStartup = ViewmodelMeshStartup::Acquired;
    }

    void STWGameplaySystemComponent::UpdateViewmodelMeshTransform(
        const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim, const AZ::Vector3& up)
    {
        if (m_viewmodelMeshStartup != ViewmodelMeshStartup::Acquired || !m_viewmodelMeshHandle.IsValid())
        {
            return;
        }

        const AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(
            AZ::Quaternion::CreateFromMatrix3x3(AZ::Matrix3x3::CreateFromColumns(right, aim, up)), center);
        m_meshFeatureProcessor->SetTransform(m_viewmodelMeshHandle, transform,
            AZ::Vector3::CreateOne());

        // PASS is only reported once the model instance actually exists, i.e. the asset really
        // loaded and the mesh is renderable - never merely because the handle was acquired.
        if (!m_viewmodelMeshReported && m_meshFeatureProcessor->GetModel(m_viewmodelMeshHandle))
        {
            m_viewmodelMeshReported = true;
            AZ_Printf("STWGameplay",
                "ATOM_VIEWMODEL_MESH result=PASS asset=%s handle=valid mesh=ready material=bound\n",
                m_viewmodelMeshAssetPath.c_str());
        }
    }

    void STWGameplaySystemComponent::ShutdownViewmodelMesh()
    {
        if (m_meshFeatureProcessor != nullptr && m_viewmodelMeshHandle.IsValid())
        {
            m_meshFeatureProcessor->ReleaseMesh(m_viewmodelMeshHandle);
        }
        m_meshFeatureProcessor = nullptr;
        m_viewmodelMeshHandle = {};
        m_viewmodelMeshAssetPath.clear();
        m_viewmodelMeshStartup = ViewmodelMeshStartup::Waiting;
        m_viewmodelMeshReported = false;
        ResetViewmodelAssetLoadState();
    }

    void STWGameplaySystemComponent::DrawPresentation()
    {
        using Bus = AzFramework::DebugDisplayRequestBus;
        const AZ::s32 displayId = static_cast<AZ::s32>(AzFramework::g_defaultSceneEntityDebugDisplayId);
        const TargetState& target = m_model.GetTarget();
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

        // Camera-relative native viewmodel. The weapon BODY is now the original STW Atom mesh;
        // arms and hands do not exist yet, and the muzzle cue remains procedural.
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
        // The weapon body is the original STW_SMG_01 Atom mesh driven
        // through the Atom MeshFeatureProcessor. It is presentation only: it consumes the
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

        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor,
            target.m_alive ? AZ::Color(0.12f, 0.65f, 0.85f, 1.0f) : AZ::Color(0.22f, 0.22f, 0.22f, 1.0f));
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawBall, target.m_position, target.m_radius, true);

        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(0.95f, 0.95f, 0.95f, 1.0f));
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.485f, 0.5f), AZ::Vector2(0.495f, 0.5f), 0.0f);
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.505f, 0.5f), AZ::Vector2(0.515f, 0.5f), 0.0f);
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.5f, 0.48f), AZ::Vector2(0.5f, 0.492f), 0.0f);
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawLine2d, AZ::Vector2(0.5f, 0.508f), AZ::Vector2(0.5f, 0.52f), 0.0f);

        char hud[128];
        azsnprintf(hud, AZ_ARRAY_SIZE(hud), "STW  HP 100/100   MP5 %02d / %03d   %s",
            weapon.m_magazine, weapon.m_reserve, weapon.m_reloading ? "RELOADING" : "READY");
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::Draw2dTextLabel, 24.0f, 36.0f, 1.4f, hud, false);
        if (presentation.m_hitCueRemaining > 0.0f || m_viewmodel.IsHitFeedbackActive())
        {
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(1.0f, 0.25f, 0.2f, 1.0f));
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawWireCircle2d, AZ::Vector2(0.5f, 0.5f), 0.018f, 0.0f);
        }
    }
}
