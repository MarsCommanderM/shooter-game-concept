#include "STWGameplaySystemComponent.h"

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/Physics/CharacterBus.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <Atom/Feature/Utils/FrameCaptureBus.h>
#include <STWGameplay/STWGameplayTypeIds.h>

#include <cstdlib>
#include <algorithm>
#include <cmath>

namespace STWGameplay
{
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
        if (const char* capturePath = std::getenv("STW_NATIVE_CAPTURE_PATH"); capturePath && capturePath[0] != '\0')
        {
            m_nativeCapturePath = capturePath;
        }
        m_automatedAcceptance = std::getenv("STW_PHYSX_ACCEPTANCE") != nullptr;
        if (!m_physicsPlayer.Initialize())
        {
            AZ_Error("STWGameplay", false, "Player Movement V2 could not create its PhysX controller");
            return;
        }
        AZ::Vector3 physicalPosition = AZ::Vector3::CreateZero();
        bool grounded = false;
        m_physicsPlayer.Synchronize(physicalPosition, grounded);
        m_model.SynchronizePhysicalState(physicalPosition, grounded);
        AzFramework::InputChannelEventListener::Connect();
        AZ::TickBus::Handler::BusConnect();
        AZ_Printf("STWGameplay", "Native Player Movement V2 PhysX active\n");
    }

    void STWGameplaySystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        AzFramework::InputChannelEventListener::Disconnect();
        m_physicsPlayer.Shutdown();
    }

    void STWGameplaySystemComponent::OnTick(float deltaTime, AZ::ScriptTimePoint)
    {
        UpdateAutomatedAcceptance(deltaTime);
        m_model.Update(deltaTime, m_input);
        m_physicsPlayer.QueueVelocity(m_model.GetDesiredVelocity(m_input));
        AZ::Vector3 physicalPosition = AZ::Vector3::CreateZero();
        bool grounded = false;
        if (m_physicsPlayer.Synchronize(physicalPosition, grounded))
        {
            m_model.SynchronizePhysicalState(physicalPosition, grounded);
        }
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
        if (!m_automatedAcceptance || m_acceptanceReported || !std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            return;
        }
        m_acceptanceTime += deltaTime;
        m_input.m_forward = 0.0f;
        m_input.m_strafe = 0.0f;
        m_input.m_sprint = false;

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

        // Camera-relative native placeholder presentation. It consumes weapon
        // events only; authoritative fire and damage remain in PlayerSliceModel.
        const AZ::Vector3 weaponCenter = m_model.GetEyePosition() + aim * 0.62f + right * 0.24f - up * 0.20f;
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(0.08f, 0.11f, 0.14f, 1.0f));
        Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawSolidOBB,
            weaponCenter, right, aim, up, AZ::Vector3(0.10f, 0.30f, 0.08f));
        if (presentation.m_fireCueRemaining > 0.0f)
        {
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(1.0f, 0.72f, 0.12f, 1.0f));
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawBall,
                weaponCenter + aim * 0.34f, 0.055f, true);
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
        if (presentation.m_hitCueRemaining > 0.0f)
        {
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::SetColor, AZ::Color(1.0f, 0.25f, 0.2f, 1.0f));
            Bus::Event(displayId, &AzFramework::DebugDisplayRequests::DrawWireCircle2d, AZ::Vector2(0.5f, 0.5f), 0.018f, 0.0f);
        }
    }
}
