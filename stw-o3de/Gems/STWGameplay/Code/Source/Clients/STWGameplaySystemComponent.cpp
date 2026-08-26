#include "STWGameplaySystemComponent.h"

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <STWGameplay/STWGameplayTypeIds.h>

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

    void STWGameplaySystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType&) {}
    void STWGameplaySystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType&) {}

    void STWGameplaySystemComponent::Activate()
    {
        AzFramework::InputChannelEventListener::Connect();
        AZ::TickBus::Handler::BusConnect();
        AZ_Printf("STWGameplay", "Native Player Vertical Slice V1 active\n");
    }

    void STWGameplaySystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        AzFramework::InputChannelEventListener::Disconnect();
    }

    void STWGameplaySystemComponent::OnTick(float deltaTime, AZ::ScriptTimePoint)
    {
        m_model.Update(deltaTime, m_input);
        m_input.m_lookX = 0.0f;
        m_input.m_lookY = 0.0f;
        m_input.m_reload = false;
        UpdateCamera();
        DrawPresentation();
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
