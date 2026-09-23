#include <STWGameplay/MainMenuPresentation.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/std/algorithm.h>

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
#include <LyShine/Bus/UiButtonBus.h>

namespace STWGameplay
{
    namespace
    {
        constexpr const char* FontPath = "fonts/default-ui.font";

        // Steel/hazard palette matching the yard's own StandardPBR sources
        // (tools/blender/generate_industrial_yard.py's "steel"/"hazard"
        // definitions), so the menu reads as the same production, not a
        // generic UI skin bolted on top of it.
        constexpr float SteelR = 0.09f, SteelG = 0.12f, SteelB = 0.14f;
        constexpr float SteelLightR = 0.16f, SteelLightG = 0.20f, SteelLightB = 0.24f;
        constexpr float HazardR = 0.85f, HazardG = 0.45f, HazardB = 0.015f;
    }

    MainMenuPresentation::~MainMenuPresentation()
    {
        Shutdown();
    }

    void MainMenuPresentation::Initialize()
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

        m_mainScreenRoot = BuildScreenRoot("MainScreen");
        m_settingsScreenRoot = BuildScreenRoot("SettingsScreen");
        m_multiplayerScreenRoot = BuildScreenRoot("MultiplayerScreen");
        m_campaignScreenRoot = BuildScreenRoot("CampaignScreen");

        BuildMainScreen();
        BuildSettingsScreen();
        BuildMultiplayerScreen();
        BuildCampaignScreen();

        ShowScreen(MainMenuScreen::Main);

        // Diagnostic evidence (MAIN_MENU_DIAGNOSTIC, gate run for b6c4f2b)
        // showed every element correctly built and enabled but with a
        // GetCanvasSpaceRectNoScaleRotate of exactly (0,0,0,0) - a clean
        // "never computed" zero, not a garbage value. UiCanvasBus exposes
        // RecomputeChangedLayouts() as an explicit public step for exactly
        // this, so layout is not purely automatic on creation.
        UiCanvasBus::Event(m_canvasId, &UiCanvasBus::Events::RecomputeChangedLayouts);
    }

    void MainMenuPresentation::Shutdown()
    {
        if (m_canvasId.IsValid())
        {
            UiCanvasManagerBus::Broadcast(&UiCanvasManagerBus::Events::UnloadCanvas, m_canvasId);
        }
        m_canvasId.SetInvalid();
        m_mainScreenRoot.SetInvalid();
        m_settingsScreenRoot.SetInvalid();
        m_multiplayerScreenRoot.SetInvalid();
        m_campaignScreenRoot.SetInvalid();
        m_activeScreen = MainMenuScreen::Main;
        m_buttonCount = 0;
        m_buttonNames.clear();
        m_clickedButtonNames.clear();
    }

    AZ::EntityId MainMenuPresentation::BuildScreenRoot(const char* name)
    {
        MenuRect fullScreen{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        AZ::Entity* entity = nullptr;
        UiCanvasBus::EventResult(entity, m_canvasId, &UiCanvasBus::Events::CreateChildElement, AZStd::string(name));
        if (entity == nullptr)
        {
            return AZ::EntityId();
        }
        const AZ::EntityId root = entity->GetId();
        entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
        entity->Activate();
        UiTransform2dBus::Event(
            root, &UiTransform2dBus::Events::SetAnchors,
            UiTransform2dInterface::Anchors(fullScreen.m_anchorLeft, fullScreen.m_anchorTop,
                fullScreen.m_anchorRight, fullScreen.m_anchorBottom),
            false, false);
        UiTransform2dBus::Event(
            root, &UiTransform2dBus::Events::SetOffsets,
            UiTransform2dInterface::Offsets(fullScreen.m_offsetLeft, fullScreen.m_offsetTop,
                fullScreen.m_offsetRight, fullScreen.m_offsetBottom));
        return root;
    }

    AZ::EntityId MainMenuPresentation::CreatePanel(
        AZ::EntityId parent, const char* name, const MenuRect& layout, float r, float g, float b, float a)
    {
        AZ::Entity* entity = nullptr;
        UiElementBus::EventResult(entity, parent, &UiElementBus::Events::CreateChildElement, AZStd::string(name));
        if (entity == nullptr)
        {
            return AZ::EntityId();
        }
        const AZ::EntityId elementId = entity->GetId();
        entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
        entity->CreateComponent(LyShine::UiImageComponentUuid);
        entity->Activate();

        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetAnchors,
            UiTransform2dInterface::Anchors(layout.m_anchorLeft, layout.m_anchorTop, layout.m_anchorRight, layout.m_anchorBottom),
            false, false);
        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetOffsets,
            UiTransform2dInterface::Offsets(layout.m_offsetLeft, layout.m_offsetTop, layout.m_offsetRight, layout.m_offsetBottom));
        UiImageBus::Event(elementId, &UiImageBus::Events::SetColor, AZ::Color(r, g, b, a));
        return elementId;
    }

    AZ::EntityId MainMenuPresentation::CreateLabel(
        AZ::EntityId parent, const char* name, const char* text, const MenuRect& layout, float fontSize, bool bold)
    {
        AZ::Entity* entity = nullptr;
        UiElementBus::EventResult(entity, parent, &UiElementBus::Events::CreateChildElement, AZStd::string(name));
        if (entity == nullptr)
        {
            return AZ::EntityId();
        }
        const AZ::EntityId elementId = entity->GetId();
        entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
        entity->CreateComponent(LyShine::UiTextComponentUuid);
        entity->Activate();

        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetAnchors,
            UiTransform2dInterface::Anchors(layout.m_anchorLeft, layout.m_anchorTop, layout.m_anchorRight, layout.m_anchorBottom),
            false, false);
        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetOffsets,
            UiTransform2dInterface::Offsets(layout.m_offsetLeft, layout.m_offsetTop, layout.m_offsetRight, layout.m_offsetBottom));
        UiTextBus::Event(elementId, &UiTextBus::Events::SetText, AZStd::string(text));
        UiTextBus::Event(elementId, &UiTextBus::Events::SetFont, LyShine::PathnameType(FontPath));
        UiTextBus::Event(elementId, &UiTextBus::Events::SetFontSize, fontSize);
        UiTextBus::Event(elementId, &UiTextBus::Events::SetColor, AZ::Color(0.92f, 0.94f, 0.96f, 1.0f));
        UiTextBus::Event(
            elementId, &UiTextBus::Events::SetTextAlignment, IDraw2d::HAlign::Center, IDraw2d::VAlign::Center);
        if (bold)
        {
            UiTextBus::Event(elementId, &UiTextBus::Events::SetCharacterSpacing, 1.5f);
        }
        return elementId;
    }

    AZ::EntityId MainMenuPresentation::CreateButton(
        AZ::EntityId parent, const char* name, const char* label, const MenuRect& layout, AZStd::function<void()> onClick)
    {
        AZ::Entity* entity = nullptr;
        UiElementBus::EventResult(entity, parent, &UiElementBus::Events::CreateChildElement, AZStd::string(name));
        if (entity == nullptr)
        {
            return AZ::EntityId();
        }
        const AZ::EntityId elementId = entity->GetId();
        entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
        entity->CreateComponent(LyShine::UiImageComponentUuid);
        entity->CreateComponent(LyShine::UiButtonComponentUuid);
        entity->Activate();

        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetAnchors,
            UiTransform2dInterface::Anchors(layout.m_anchorLeft, layout.m_anchorTop, layout.m_anchorRight, layout.m_anchorBottom),
            false, false);
        UiTransform2dBus::Event(
            elementId, &UiTransform2dBus::Events::SetOffsets,
            UiTransform2dInterface::Offsets(layout.m_offsetLeft, layout.m_offsetTop, layout.m_offsetRight, layout.m_offsetBottom));
        // Metallic steel panel, matching the yard's own steel material tone -
        // a real button texture/sprite is a later, separate art pass.
        UiImageBus::Event(elementId, &UiImageBus::Events::SetColor, AZ::Color(SteelLightR, SteelLightG, SteelLightB, 0.92f));

        AZStd::string buttonName(name);
        UiButtonBus::Event(
            elementId, &UiButtonBus::Events::SetOnClickCallback,
            [this, buttonName, onClick](AZ::EntityId, AZ::Vector2)
            {
                m_clickedButtonNames.push_back(buttonName);
                if (onClick)
                {
                    onClick();
                }
            });

        MenuRect labelLayout{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        AZStd::string labelName = buttonName + "_Label";
        CreateLabel(elementId, labelName.c_str(), label, labelLayout, 20.0f, true);

        m_buttonNames.push_back(buttonName);
        m_buttonCallbacks.push_back({ buttonName, onClick });
        ++m_buttonCount;
        return elementId;
    }

    void MainMenuPresentation::SetScreenVisible(AZ::EntityId screenRoot, bool visible)
    {
        if (screenRoot.IsValid())
        {
            UiElementBus::Event(screenRoot, &UiElementBus::Events::SetIsEnabled, visible);
        }
    }

    void MainMenuPresentation::ShowScreen(MainMenuScreen screen)
    {
        m_activeScreen = screen;
        SetScreenVisible(m_mainScreenRoot, screen == MainMenuScreen::Main);
        SetScreenVisible(m_settingsScreenRoot, screen == MainMenuScreen::Settings);
        SetScreenVisible(m_multiplayerScreenRoot, screen == MainMenuScreen::Multiplayer);
        SetScreenVisible(m_campaignScreenRoot, screen == MainMenuScreen::Campaign);
    }

    void MainMenuPresentation::BuildMainScreen()
    {
        // Deliberately no full-screen background panel here: the live 3D
        // scene behind the canvas (the industrial yard, already rendering
        // via EnvironmentPresentation/ArenaPresentation) IS the "battle
        // scene in the background" - a static image would fight it, and a
        // render-to-texture camera rig is a separate, later piece of work.
        CreateLabel(
            m_mainScreenRoot, "Title", "S.T.W.",
            MenuRect{ 0.0f, 0.06f, 1.0f, 0.18f, 0.0f, 0.0f, 0.0f, 0.0f }, 64.0f, true);

        // One row of buttons, matching the classic FPS main-menu layout the
        // user asked for: Kampagne, Multiplayer, Einstellungen, Beenden.
        constexpr float ButtonWidth = 0.20f;
        constexpr float Gap = 0.02f;
        constexpr float RowStart = 0.5f - (4.0f * ButtonWidth + 3.0f * Gap) * 0.5f;
        constexpr float RowY = 0.74f;
        constexpr float RowHeight = 0.07f;

        const char* labels[4] = { "KAMPAGNE", "MULTIPLAYER", "EINSTELLUNGEN", "BEENDEN" };
        const char* names[4] = { "CampaignButton", "MultiplayerButton", "SettingsButton", "QuitButton" };
        for (int index = 0; index < 4; ++index)
        {
            const float left = RowStart + index * (ButtonWidth + Gap);
            const float right = left + ButtonWidth;
            MenuRect buttonRect{ left, RowY, right, RowY + RowHeight, 0.0f, 0.0f, 0.0f, 0.0f };
            switch (index)
            {
            case 0:
                CreateButton(m_mainScreenRoot, names[index], labels[index], buttonRect,
                    [this]() { ShowScreen(MainMenuScreen::Campaign); });
                break;
            case 1:
                CreateButton(m_mainScreenRoot, names[index], labels[index], buttonRect,
                    [this]() { ShowScreen(MainMenuScreen::Multiplayer); });
                break;
            case 2:
                CreateButton(m_mainScreenRoot, names[index], labels[index], buttonRect,
                    [this]() { ShowScreen(MainMenuScreen::Settings); });
                break;
            default:
                CreateButton(m_mainScreenRoot, names[index], labels[index], buttonRect, AZStd::function<void()>());
                break;
            }
        }
    }

    void MainMenuPresentation::BuildSettingsScreen()
    {
        CreatePanel(
            m_settingsScreenRoot, "SettingsBackdrop",
            MenuRect{ 0.15f, 0.10f, 0.85f, 0.90f, 0.0f, 0.0f, 0.0f, 0.0f }, SteelR, SteelG, SteelB, 0.90f);
        CreateLabel(
            m_settingsScreenRoot, "SettingsTitle", "EINSTELLUNGEN",
            MenuRect{ 0.15f, 0.12f, 0.85f, 0.20f, 0.0f, 0.0f, 0.0f, 0.0f }, 36.0f, true);

        // Section labels only in this pass - real sound/control value
        // binding (MiniAudio master volume, AzFramework::InputChannelId
        // rebind) is verified O3DE-API-feasible but is its own separate,
        // separately-verified step, not stubbed or faked here.
        CreateLabel(
            m_settingsScreenRoot, "SoundSectionLabel", "SOUND",
            MenuRect{ 0.20f, 0.28f, 0.50f, 0.34f, 0.0f, 0.0f, 0.0f, 0.0f }, 22.0f, true);
        CreateLabel(
            m_settingsScreenRoot, "ControlsSectionLabel", "STEUERUNG",
            MenuRect{ 0.20f, 0.42f, 0.50f, 0.48f, 0.0f, 0.0f, 0.0f, 0.0f }, 22.0f, true);
        CreateLabel(
            m_settingsScreenRoot, "VideoSectionLabel", "KONTRAST / VIDEO",
            MenuRect{ 0.20f, 0.56f, 0.50f, 0.62f, 0.0f, 0.0f, 0.0f, 0.0f }, 22.0f, true);

        CreateButton(
            m_settingsScreenRoot, "SettingsBackButton", "ZURUECK",
            MenuRect{ 0.40f, 0.80f, 0.60f, 0.87f, 0.0f, 0.0f, 0.0f, 0.0f },
            [this]() { ShowScreen(MainMenuScreen::Main); });
    }

    void MainMenuPresentation::BuildMultiplayerScreen()
    {
        CreatePanel(
            m_multiplayerScreenRoot, "MultiplayerBackdrop",
            MenuRect{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f }, SteelR, SteelG, SteelB, 0.55f);
        CreateLabel(
            m_multiplayerScreenRoot, "MultiplayerTitle", "MULTIPLAYER",
            MenuRect{ 0.0f, 0.06f, 1.0f, 0.16f, 0.0f, 0.0f, 0.0f, 0.0f }, 48.0f, true);

        // The five modes the user named: Team Deathmatch, Domination,
        // Headquarters, Defense, Sabotage. Button shell + navigation only in
        // this pass - each mode's actual ruleset (score limits, objective
        // logic, win conditions) is separate, much larger gameplay work.
        const char* names[5] = {
            "ModeTeamDeathmatch", "ModeDomination", "ModeHeadquarters", "ModeDefense", "ModeSabotage"
        };
        const char* labels[5] = { "TEAM DEATHMATCH", "HERRSCHAFT", "HAUPTQUARTIER", "VERTEIDIGUNG", "SABOTAGE" };
        constexpr float RowY = 0.30f;
        constexpr float RowHeight = 0.08f;
        constexpr float RowGap = 0.10f;
        for (int index = 0; index < 5; ++index)
        {
            const float top = RowY + index * RowGap;
            MenuRect buttonRect{ 0.30f, top, 0.70f, top + RowHeight, 0.0f, 0.0f, 0.0f, 0.0f };
            CreateButton(m_multiplayerScreenRoot, names[index], labels[index], buttonRect, AZStd::function<void()>());
        }

        CreateButton(
            m_multiplayerScreenRoot, "MultiplayerBackButton", "ZURUECK",
            MenuRect{ 0.40f, 0.88f, 0.60f, 0.95f, 0.0f, 0.0f, 0.0f, 0.0f },
            [this]() { ShowScreen(MainMenuScreen::Main); });
    }

    void MainMenuPresentation::BuildCampaignScreen()
    {
        CreatePanel(
            m_campaignScreenRoot, "CampaignBackdrop",
            MenuRect{ 0.20f, 0.30f, 0.80f, 0.60f, 0.0f, 0.0f, 0.0f, 0.0f }, SteelR, SteelG, SteelB, 0.90f);
        CreateLabel(
            m_campaignScreenRoot, "CampaignTitle", "KAMPAGNE",
            MenuRect{ 0.20f, 0.32f, 0.80f, 0.40f, 0.0f, 0.0f, 0.0f, 0.0f }, 32.0f, true);
        CreateLabel(
            m_campaignScreenRoot, "CampaignBody", "Noch keine Mission verfuegbar.",
            MenuRect{ 0.20f, 0.42f, 0.80f, 0.50f, 0.0f, 0.0f, 0.0f, 0.0f }, 18.0f, false);
        CreateButton(
            m_campaignScreenRoot, "CampaignBackButton", "ZURUECK",
            MenuRect{ 0.40f, 0.52f, 0.60f, 0.58f, 0.0f, 0.0f, 0.0f, 0.0f },
            [this]() { ShowScreen(MainMenuScreen::Main); });
    }

    bool MainMenuPresentation::WasEveryButtonClickTested() const
    {
        if (m_buttonNames.empty())
        {
            return false;
        }
        for (const AZStd::string& name : m_buttonNames)
        {
            if (AZStd::find(m_clickedButtonNames.begin(), m_clickedButtonNames.end(), name) == m_clickedButtonNames.end())
            {
                return false;
            }
        }
        return true;
    }

    void MainMenuPresentation::RecomputeLayout() const
    {
        if (m_canvasId.IsValid())
        {
            UiCanvasBus::Event(m_canvasId, &UiCanvasBus::Events::RecomputeChangedLayouts);
        }
    }

    void MainMenuPresentation::LogDiagnostics() const
    {
        AZ::Vector2 canvasSize(0.0f, 0.0f);
        UiCanvasBus::EventResult(canvasSize, m_canvasId, &UiCanvasBus::Events::GetCanvasSize);
        AZ::Vector2 authoredCanvasSize(0.0f, 0.0f);
        UiCanvasBus::EventResult(authoredCanvasSize, m_canvasId, &UiCanvasBus::Events::GetAuthoredCanvasSize);

        bool mainScreenEnabled = false;
        UiElementBus::EventResult(mainScreenEnabled, m_mainScreenRoot, &UiElementBus::Events::IsEnabled);

        AZ::Entity* quitButton = nullptr;
        UiCanvasBus::EventResult(quitButton, m_canvasId, &UiCanvasBus::Events::FindElementByName, AZStd::string("QuitButton"));
        UiTransformInterface::Rect quitRect{ 0.0f, 0.0f, 0.0f, 0.0f };
        bool quitFound = quitButton != nullptr;
        bool quitEnabled = false;
        if (quitButton != nullptr)
        {
            UiTransformBus::Event(quitButton->GetId(), &UiTransformBus::Events::GetCanvasSpaceRectNoScaleRotate, quitRect);
            UiElementBus::EventResult(quitEnabled, quitButton->GetId(), &UiElementBus::Events::IsEnabled);
        }

        AZ_Printf(
            "STWGameplay",
            "MAIN_MENU_DIAGNOSTIC canvas_size=(%.1f,%.1f) authored_canvas_size=(%.1f,%.1f) "
            "main_screen_enabled=%d quit_button_found=%d quit_button_enabled=%d "
            "quit_button_rect=(l=%.1f,r=%.1f,t=%.1f,b=%.1f)\n",
            static_cast<double>(canvasSize.GetX()), static_cast<double>(canvasSize.GetY()),
            static_cast<double>(authoredCanvasSize.GetX()), static_cast<double>(authoredCanvasSize.GetY()),
            mainScreenEnabled ? 1 : 0, quitFound ? 1 : 0, quitEnabled ? 1 : 0,
            static_cast<double>(quitRect.left), static_cast<double>(quitRect.right),
            static_cast<double>(quitRect.top), static_cast<double>(quitRect.bottom));
    }

    void MainMenuPresentation::TestClick(const char* buttonName)
    {
        // Invokes the same callback CreateButton registered with LyShine's
        // UiButtonComponent, directly - depends only on this class's own
        // bookkeeping, not on an unverified UiButtonBus click-simulation
        // signature.
        for (const auto& entry : m_buttonCallbacks)
        {
            if (entry.first == buttonName)
            {
                m_clickedButtonNames.push_back(entry.first);
                if (entry.second)
                {
                    entry.second();
                }
                return;
            }
        }
    }
}
