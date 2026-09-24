#include <STWGameplay/HudPresentation.h>

#include <AzCore/Component/Entity.h>

#include <LyShine/UiBase.h>
#include <LyShine/UiComponentTypes.h>
#include <LyShine/IDraw2d.h>
#include <LyShine/Bus/UiCanvasManagerBus.h>
#include <LyShine/Bus/UiCanvasBus.h>
#include <LyShine/Bus/UiElementBus.h>
#include <LyShine/Bus/UiTransform2dBus.h>
#include <LyShine/Bus/UiTransformBus.h>
#include <LyShine/Bus/UiImageBus.h>
#include <LyShine/Bus/UiTextBus.h>

namespace STWGameplay
{
    namespace
    {
        // Named distinctly from MainMenuPresentation.cpp's identical
        // constants - this build concatenates STWGameplay's .cpp files into
        // unity translation units, which flattens anonymous namespaces
        // across files and turns same-named constexprs into real
        // redefinition errors (caught by a real local build, not guessed).
        constexpr const char* HudFontPath = "fonts/default-ui.font";
        // Same steel palette as MainMenuPresentation, so the HUD backdrop
        // reads as the same production rather than a mismatched UI skin.
        constexpr float HudSteelR = 0.09f, HudSteelG = 0.12f, HudSteelB = 0.14f;
    }

    HudPresentation::~HudPresentation()
    {
        Shutdown();
    }

    void HudPresentation::Initialize()
    {
        if (m_canvasId.IsValid())
        {
            return;
        }

        UiCanvasManagerBus::BroadcastResult(m_canvasId, &UiCanvasManagerBus::Events::CreateCanvas);
        if (!m_canvasId.IsValid())
        {
            return;
        }

        // Translucent backdrop behind the stat block for legibility against
        // a bright sky background (the debug-drawn text it replaces had no
        // such backing and was only marginally legible in the same spot).
        AZ::Entity* backdropEntity = nullptr;
        UiCanvasBus::EventResult(
            backdropEntity, m_canvasId, &UiCanvasBus::Events::CreateChildElement, AZStd::string("HudBackdrop"));
        if (backdropEntity != nullptr)
        {
            const AZ::EntityId backdropId = backdropEntity->GetId();
            backdropEntity->Deactivate();
            backdropEntity->CreateComponent(LyShine::UiTransform2dComponentUuid);
            backdropEntity->CreateComponent(LyShine::UiImageComponentUuid);
            backdropEntity->Activate();
            UiTransform2dBus::Event(
                backdropId, &UiTransform2dBus::Events::SetAnchors,
                UiTransform2dInterface::Anchors(0.0f, 0.0f, 0.30f, 0.115f), false, false);
            UiTransform2dBus::Event(
                backdropId, &UiTransform2dBus::Events::SetOffsets, UiTransform2dInterface::Offsets(0.0f, 0.0f, 0.0f, 0.0f));
            UiImageBus::Event(backdropId, &UiImageBus::Events::SetColor, AZ::Color(HudSteelR, HudSteelG, HudSteelB, 0.55f));
        }

        m_healthLabel = CreateHudLabel("HudHealthLabel", 0.012f, 0.010f, 0.28f, 0.045f, 20.0f);
        m_weaponAmmoLabel = CreateHudLabel("HudWeaponAmmoLabel", 0.012f, 0.045f, 0.29f, 0.078f, 18.0f);
        m_objectiveLabel = CreateHudLabel("HudObjectiveLabel", 0.012f, 0.078f, 0.29f, 0.110f, 15.0f);
        BuildCrosshair();

        // Same reasoning as MainMenuPresentation::Initialize() - LyShine's
        // rect computation is not automatic on creation, an explicit
        // recompute is required (root-caused via real diagnostic evidence
        // there, not guessed).
        UiCanvasBus::Event(m_canvasId, &UiCanvasBus::Events::RecomputeChangedLayouts);
    }

    void HudPresentation::Shutdown()
    {
        if (m_canvasId.IsValid())
        {
            UiCanvasManagerBus::Broadcast(&UiCanvasManagerBus::Events::UnloadCanvas, m_canvasId);
        }
        m_canvasId.SetInvalid();
        m_healthLabel.SetInvalid();
        m_weaponAmmoLabel.SetInvalid();
        m_objectiveLabel.SetInvalid();
        m_hitFeedbackMarker.SetInvalid();
    }

    AZ::EntityId HudPresentation::CreateCrosshairPanel(
        const char* name, float anchorLeft, float anchorTop, float anchorRight, float anchorBottom,
        float offsetLeft, float offsetTop, float offsetRight, float offsetBottom, float r, float g, float b, float a)
    {
        AZ::Entity* entity = nullptr;
        UiCanvasBus::EventResult(entity, m_canvasId, &UiCanvasBus::Events::CreateChildElement, AZStd::string(name));
        if (entity == nullptr)
        {
            return AZ::EntityId();
        }
        const AZ::EntityId elementId = entity->GetId();
        entity->Deactivate();
        entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
        entity->CreateComponent(LyShine::UiImageComponentUuid);
        entity->Activate();

        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetAnchors,
            UiTransform2dInterface::Anchors(anchorLeft, anchorTop, anchorRight, anchorBottom), false, false);
        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetOffsets,
            UiTransform2dInterface::Offsets(offsetLeft, offsetTop, offsetRight, offsetBottom));
        UiImageBus::Event(elementId, &UiImageBus::Events::SetColor, AZ::Color(r, g, b, a));
        return elementId;
    }

    void HudPresentation::BuildCrosshair()
    {
        // Same normalized 0..1 screen positions the old
        // AzFramework::DebugDisplayRequests::DrawLine2d crosshair used
        // (four segments around canvas center), only the rendering
        // mechanism changed. Anchors carry the long axis exactly as before;
        // a 2px offset on the short axis gives each segment real thickness
        // (a debug-drawn line has no LyShine equivalent with zero width).
        CreateCrosshairPanel(
            "HudCrosshairLeft", 0.485f, 0.5f, 0.495f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.95f, 0.95f, 0.95f, 1.0f);
        CreateCrosshairPanel(
            "HudCrosshairRight", 0.505f, 0.5f, 0.515f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.95f, 0.95f, 0.95f, 1.0f);
        CreateCrosshairPanel(
            "HudCrosshairTop", 0.5f, 0.48f, 0.5f, 0.492f, -1.0f, 0.0f, 1.0f, 0.0f, 0.95f, 0.95f, 0.95f, 1.0f);
        CreateCrosshairPanel(
            "HudCrosshairBottom", 0.5f, 0.508f, 0.5f, 0.52f, -1.0f, 0.0f, 1.0f, 0.0f, 0.95f, 0.95f, 0.95f, 1.0f);

        // Hit-feedback marker: a small red frame around the crosshair,
        // replacing the debug-drawn DrawWireCircle2d that used to appear on
        // the same condition. Built disabled; SetCrosshairHitFeedback()
        // toggles it.
        m_hitFeedbackMarker = CreateCrosshairPanel(
            "HudHitFeedbackMarker", 0.478f, 0.472f, 0.522f, 0.528f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.25f, 0.2f, 0.0f);
        if (m_hitFeedbackMarker.IsValid())
        {
            // Frame look: full-alpha border achieved by four thin edges
            // would need four more elements; a single translucent-red panel
            // is the pragmatic real substitute for the wire-circle ring
            // with no custom texture asset available.
            UiImageBus::Event(m_hitFeedbackMarker, &UiImageBus::Events::SetColor, AZ::Color(1.0f, 0.25f, 0.2f, 0.35f));
            UiElementBus::Event(m_hitFeedbackMarker, &UiElementBus::Events::SetIsEnabled, false);
        }
    }

    void HudPresentation::SetCrosshairHitFeedback(bool active)
    {
        if (!m_hitFeedbackMarker.IsValid())
        {
            return;
        }
        UiElementBus::Event(m_hitFeedbackMarker, &UiElementBus::Events::SetIsEnabled, active);
    }

    bool HudPresentation::IsHitFeedbackMarkerVisible() const
    {
        if (!m_hitFeedbackMarker.IsValid())
        {
            return false;
        }
        bool enabled = false;
        UiElementBus::EventResult(enabled, m_hitFeedbackMarker, &UiElementBus::Events::IsEnabled);
        return enabled;
    }

    AZ::EntityId HudPresentation::CreateHudLabel(
        const char* name, float anchorLeft, float anchorTop, float anchorRight, float anchorBottom, float fontSize)
    {
        AZ::Entity* entity = nullptr;
        UiCanvasBus::EventResult(entity, m_canvasId, &UiCanvasBus::Events::CreateChildElement, AZStd::string(name));
        if (entity == nullptr)
        {
            return AZ::EntityId();
        }
        const AZ::EntityId elementId = entity->GetId();
        // The exact fix from MainMenuPresentation's zero-rect bug:
        // CreateChildElement returns an already-active entity, and O3DE
        // requires deactivating before adding further components.
        entity->Deactivate();
        entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
        entity->CreateComponent(LyShine::UiTextComponentUuid);
        entity->Activate();

        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetAnchors,
            UiTransform2dInterface::Anchors(anchorLeft, anchorTop, anchorRight, anchorBottom), false, false);
        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetOffsets, UiTransform2dInterface::Offsets(0.0f, 0.0f, 0.0f, 0.0f));
        UiTextBus::Event(elementId, &UiTextBus::Events::SetFont, LyShine::PathnameType(HudFontPath));
        UiTextBus::Event(elementId, &UiTextBus::Events::SetFontSize, fontSize);
        UiTextBus::Event(elementId, &UiTextBus::Events::SetColor, AZ::Color(0.92f, 0.94f, 0.96f, 1.0f));
        // Left/top aligned, not centered like MainMenuPresentation's
        // titles/buttons - this is a corner stat readout, not a centered
        // menu element.
        UiTextBus::Event(elementId, &UiTextBus::Events::SetTextAlignment, IDraw2d::HAlign::Left, IDraw2d::VAlign::Center);
        return elementId;
    }

    void HudPresentation::Update(const char* healthText, const char* weaponAmmoText, const char* objectiveText)
    {
        if (!IsReady())
        {
            return;
        }
        UiTextBus::Event(m_healthLabel, &UiTextBus::Events::SetText, AZStd::string(healthText));
        UiTextBus::Event(m_weaponAmmoLabel, &UiTextBus::Events::SetText, AZStd::string(weaponAmmoText));
        UiTextBus::Event(m_objectiveLabel, &UiTextBus::Events::SetText, AZStd::string(objectiveText));
    }

    AZStd::string HudPresentation::GetLabelText(const char* labelName) const
    {
        AZ::Entity* labelEntity = nullptr;
        UiCanvasBus::EventResult(labelEntity, m_canvasId, &UiCanvasBus::Events::FindElementByName, AZStd::string(labelName));
        if (labelEntity == nullptr)
        {
            return AZStd::string();
        }
        AZStd::string text;
        UiTextBus::EventResult(text, labelEntity->GetId(), &UiTextBus::Events::GetText);
        return text;
    }

    void HudPresentation::LogDiagnostics() const
    {
        AZ::Vector2 canvasSize(0.0f, 0.0f);
        UiCanvasBus::EventResult(canvasSize, m_canvasId, &UiCanvasBus::Events::GetCanvasSize);
        UiTransformInterface::Rect healthRect{ 0.0f, 0.0f, 0.0f, 0.0f };
        UiTransformBus::Event(m_healthLabel, &UiTransformBus::Events::GetCanvasSpaceRectNoScaleRotate, healthRect);
        AZ_Printf(
            "STWGameplay",
            "HUD_DIAGNOSTIC canvas_size=(%.1f,%.1f) health_label_rect=(l=%.1f,r=%.1f,t=%.1f,b=%.1f)\n",
            static_cast<double>(canvasSize.GetX()), static_cast<double>(canvasSize.GetY()),
            static_cast<double>(healthRect.left), static_cast<double>(healthRect.right),
            static_cast<double>(healthRect.top), static_cast<double>(healthRect.bottom));
    }
}
