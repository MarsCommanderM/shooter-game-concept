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
#include <LyShine/Bus/UiSliderBus.h>

#include <MiniAudio/MiniAudioBus.h>

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
        m_sliderCallbacks.clear();
        m_controlsRebindHandler = nullptr;
        m_contrastChangeHandler = nullptr;
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
        // CreateChildElement returns an already-active entity (it
        // deactivates internally only to add its own UiElementComponent,
        // then reactivates before returning) - O3DE convention requires
        // deactivating before adding further components.
        entity->Deactivate();
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
        entity->Deactivate();
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
        entity->Deactivate();
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
        entity->Deactivate();
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

    AZ::EntityId MainMenuPresentation::CreateSlider(
        AZ::EntityId parent, const char* name, const MenuRect& layout,
        float minValue, float maxValue, float initialValue, AZStd::function<void(float)> onChange)
    {
        AZ::Entity* entity = nullptr;
        UiElementBus::EventResult(entity, parent, &UiElementBus::Events::CreateChildElement, AZStd::string(name));
        if (entity == nullptr)
        {
            return AZ::EntityId();
        }
        const AZ::EntityId sliderId = entity->GetId();
        entity->Deactivate();
        entity->CreateComponent(LyShine::UiTransform2dComponentUuid);
        entity->CreateComponent(LyShine::UiImageComponentUuid);
        entity->CreateComponent(LyShine::UiSliderComponentUuid);
        entity->Activate();

        UiTransform2dBus::Event(
            sliderId, &UiTransform2dBus::Events::SetAnchors,
            UiTransform2dInterface::Anchors(layout.m_anchorLeft, layout.m_anchorTop, layout.m_anchorRight, layout.m_anchorBottom),
            false, false);
        UiTransform2dBus::Event(
            sliderId, &UiTransform2dBus::Events::SetOffsets,
            UiTransform2dInterface::Offsets(layout.m_offsetLeft, layout.m_offsetTop, layout.m_offsetRight, layout.m_offsetBottom));
        UiImageBus::Event(sliderId, &UiImageBus::Events::SetColor, AZ::Color(SteelR, SteelG, SteelB, 0.95f));

        // Fill: left-anchored, its right anchor is driven by
        // UiSliderComponent::SetValue() itself (see UiSliderComponent.cpp,
        // SetValue() overwrites anchors.m_left/m_right using the fraction of
        // value between min/max) - the {0,0,0,1} rect here is only the
        // starting shape before the first SetValue() call below.
        AZStd::string fillName = AZStd::string(name) + "_Fill";
        const AZ::EntityId fillId = CreatePanel(
            sliderId, fillName.c_str(), MenuRect{ 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
            HazardR, HazardG, HazardB, 0.95f);

        // Manipulator/handle: a small fixed-size square whose anchor point
        // (not offsets) is moved to the value fraction by SetValue(), same
        // mechanism as the fill.
        AZStd::string handleName = AZStd::string(name) + "_Handle";
        const AZ::EntityId handleId = CreatePanel(
            sliderId, handleName.c_str(), MenuRect{ 0.0f, 0.0f, 0.0f, 1.0f, -8.0f, -4.0f, 8.0f, 4.0f },
            0.92f, 0.94f, 0.96f, 1.0f);

        // The slider's own rect defines the range of movement - no separate
        // track child needed, UiSliderBus only reads the track entity's
        // transform to compute drag distance/position.
        UiSliderBus::Event(sliderId, &UiSliderBus::Events::SetTrackEntity, sliderId);
        UiSliderBus::Event(sliderId, &UiSliderBus::Events::SetFillEntity, fillId);
        UiSliderBus::Event(sliderId, &UiSliderBus::Events::SetManipulatorEntity, handleId);
        UiSliderBus::Event(sliderId, &UiSliderBus::Events::SetMinValue, minValue);
        UiSliderBus::Event(sliderId, &UiSliderBus::Events::SetMaxValue, maxValue);
        UiSliderBus::Event(sliderId, &UiSliderBus::Events::SetValue, initialValue);

        AZStd::string sliderName(name);
        UiSliderBus::Event(
            sliderId, &UiSliderBus::Events::SetValueChangedCallback,
            [onChange](AZ::EntityId, float value)
            {
                if (onChange)
                {
                    onChange(value);
                }
            });

        m_sliderCallbacks.push_back({ sliderName, onChange });

        // SetValue() above does not invoke the callback (verified against
        // UiSliderComponent::SetValue() - it only updates fill/manipulator
        // visuals; callbacks only fire from DoChangedActions()/
        // DoChangingActions(), which real drag interaction reaches but a
        // manual SetValue() does not). Call it once explicitly here so the
        // driven subsystem (e.g. MiniAudio) is synced to the slider's actual
        // starting value instead of relying on the two defaults happening to
        // already match.
        float actualInitialValue = 0.0f;
        UiSliderBus::EventResult(actualInitialValue, sliderId, &UiSliderBus::Events::GetValue);
        if (onChange)
        {
            onChange(actualInitialValue);
        }

        return sliderId;
    }

    void MainMenuPresentation::CreateControlRow(
        AZ::EntityId parent, const char* actionId, const char* actionLabel, const char* buttonName,
        float top, float height)
    {
        MenuRect nameLayout{ 0.20f, top, 0.50f, top + height, 0.0f, 0.0f, 0.0f, 0.0f };
        AZStd::string nameLabelName = AZStd::string(buttonName) + "_ActionName";
        CreateLabel(parent, nameLabelName.c_str(), actionLabel, nameLayout, 16.0f, false);

        MenuRect keyLayout{ 0.52f, top, 0.80f, top + height, 0.0f, 0.0f, 0.0f, 0.0f };
        AZStd::string id(actionId);
        CreateButton(
            parent, buttonName, "...", keyLayout,
            [this, id]()
            {
                if (m_controlsRebindHandler)
                {
                    m_controlsRebindHandler(id.c_str());
                }
            });
    }

    void MainMenuPresentation::SetControlsRebindHandler(AZStd::function<void(const char*)> handler)
    {
        m_controlsRebindHandler = handler;
    }

    void MainMenuPresentation::SetContrastChangeHandler(AZStd::function<void(float)> handler)
    {
        m_contrastChangeHandler = handler;
    }

    void MainMenuPresentation::SetControlLabel(const char* buttonName, const char* text)
    {
        AZ::Entity* labelEntity = nullptr;
        AZStd::string labelName = AZStd::string(buttonName) + "_Label";
        UiCanvasBus::EventResult(labelEntity, m_canvasId, &UiCanvasBus::Events::FindElementByName, labelName);
        if (labelEntity == nullptr)
        {
            return;
        }
        UiTextBus::Event(labelEntity->GetId(), &UiTextBus::Events::SetText, AZStd::string(text));
    }

    AZStd::string MainMenuPresentation::GetControlLabel(const char* buttonName) const
    {
        AZ::Entity* labelEntity = nullptr;
        AZStd::string labelName = AZStd::string(buttonName) + "_Label";
        UiCanvasBus::EventResult(labelEntity, m_canvasId, &UiCanvasBus::Events::FindElementByName, labelName);
        if (labelEntity == nullptr)
        {
            return AZStd::string();
        }
        AZStd::string text;
        UiTextBus::EventResult(text, labelEntity->GetId(), &UiTextBus::Events::GetText);
        return text;
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
            MenuRect{ 0.15f, 0.11f, 0.85f, 0.17f, 0.0f, 0.0f, 0.0f, 0.0f }, 32.0f, true);

        // Real sound volume control: a working UiSliderComponent wired to
        // MiniAudio's actual global volume (MiniAudioRequestBus::
        // SetGlobalVolume). Kontrast is still a section label only in this
        // pass - see MainMenuPresentation.h class comment and the
        // stw-main-menu memory note for what remains.
        CreateLabel(
            m_settingsScreenRoot, "SoundSectionLabel", "SOUND",
            MenuRect{ 0.20f, 0.20f, 0.50f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f }, 20.0f, true);
        CreateSlider(
            m_settingsScreenRoot, "VolumeSlider",
            MenuRect{ 0.52f, 0.205f, 0.80f, 0.245f, 0.0f, 0.0f, 0.0f, 0.0f },
            0.0f, 100.0f, 100.0f,
            [](float value)
            {
                MiniAudio::MiniAudioRequestBus::Broadcast(
                    &MiniAudio::MiniAudioRequestBus::Events::SetGlobalVolume, value / 100.0f);
            });

        // Real, working key rebind: each row's button shows the currently
        // bound key and, when clicked, tells STWGameplaySystemComponent (the
        // only class that actually owns AzFramework::InputChannelEventListener)
        // to capture the next real key/button press for that action. This
        // class stays input-device-agnostic; it only renders what it is told.
        CreateLabel(
            m_settingsScreenRoot, "ControlsSectionLabel", "STEUERUNG",
            MenuRect{ 0.20f, 0.28f, 0.50f, 0.32f, 0.0f, 0.0f, 0.0f, 0.0f }, 20.0f, true);
        constexpr float RowStart = 0.335f;
        constexpr float RowHeight = 0.045f;
        constexpr float RowStep = 0.053f;
        struct ControlRowSpec
        {
            const char* m_actionId;
            const char* m_label;
            const char* m_buttonName;
        };
        constexpr ControlRowSpec rows[8] = {
            { "Forward", "VORWAERTS", "RebindForwardButton" },
            { "Back", "RUECKWAERTS", "RebindBackButton" },
            { "Left", "LINKS", "RebindLeftButton" },
            { "Right", "RECHTS", "RebindRightButton" },
            { "Jump", "SPRINGEN", "RebindJumpButton" },
            { "Crouch", "DUCKEN", "RebindCrouchButton" },
            { "Sprint", "SPRINTEN", "RebindSprintButton" },
            { "Reload", "NACHLADEN", "RebindReloadButton" },
        };
        for (int index = 0; index < 8; ++index)
        {
            const float top = RowStart + index * RowStep;
            CreateControlRow(
                m_settingsScreenRoot, rows[index].m_actionId, rows[index].m_label, rows[index].m_buttonName,
                top, RowHeight);
        }

        // Real, working contrast control: wired to EnvironmentPresentation's
        // actual HDRColorGradingSettingsInterface (SetColorGradingContrast),
        // the same post-process pipeline already driving the rest of the
        // scene's look - not a separate/fake preview. Range/default derived
        // from the real ACES contrast formula in HDRColorGradingCommon.azsl
        // (contrastAdjustment = amount*0.01+1.0) and EnvironmentPresentation's
        // own existing baseline (GetColorGradingContrast()==0.10), not
        // guessed. Broader video settings (resolution/fullscreen/VSync) are
        // not implemented - see MainMenuPresentation.h and the
        // stw-main-menu memory note for what remains.
        CreateLabel(
            m_settingsScreenRoot, "VideoSectionLabel", "KONTRAST",
            MenuRect{ 0.20f, 0.765f, 0.50f, 0.805f, 0.0f, 0.0f, 0.0f, 0.0f }, 20.0f, true);
        CreateSlider(
            m_settingsScreenRoot, "ContrastSlider",
            MenuRect{ 0.52f, 0.77f, 0.80f, 0.80f, 0.0f, 0.0f, 0.0f, 0.0f },
            -50.0f, 50.0f, 0.10f,
            [this](float value)
            {
                if (m_contrastChangeHandler)
                {
                    m_contrastChangeHandler(value);
                }
            });

        CreateButton(
            m_settingsScreenRoot, "SettingsBackButton", "ZURUECK",
            MenuRect{ 0.40f, 0.83f, 0.60f, 0.89f, 0.0f, 0.0f, 0.0f, 0.0f },
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
        UiTransformInterface::Rect screenRootRect{ 0.0f, 0.0f, 0.0f, 0.0f };
        UiTransformBus::Event(m_mainScreenRoot, &UiTransformBus::Events::GetCanvasSpaceRectNoScaleRotate, screenRootRect);

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
            "main_screen_enabled=%d screen_root_rect=(l=%.1f,r=%.1f,t=%.1f,b=%.1f) "
            "quit_button_found=%d quit_button_enabled=%d "
            "quit_button_rect=(l=%.1f,r=%.1f,t=%.1f,b=%.1f)\n",
            static_cast<double>(canvasSize.GetX()), static_cast<double>(canvasSize.GetY()),
            static_cast<double>(authoredCanvasSize.GetX()), static_cast<double>(authoredCanvasSize.GetY()),
            mainScreenEnabled ? 1 : 0,
            static_cast<double>(screenRootRect.left), static_cast<double>(screenRootRect.right),
            static_cast<double>(screenRootRect.top), static_cast<double>(screenRootRect.bottom),
            quitFound ? 1 : 0, quitEnabled ? 1 : 0,
            static_cast<double>(quitRect.left), static_cast<double>(quitRect.right),
            static_cast<double>(quitRect.top), static_cast<double>(quitRect.bottom));
    }

    void MainMenuPresentation::TestSliderChange(const char* sliderName, float value)
    {
        AZ::Entity* sliderEntity = nullptr;
        UiCanvasBus::EventResult(
            sliderEntity, m_canvasId, &UiCanvasBus::Events::FindElementByName, AZStd::string(sliderName));
        if (sliderEntity == nullptr)
        {
            return;
        }
        const AZ::EntityId sliderId = sliderEntity->GetId();

        // Real UiSliderBus::SetValue() - updates the fill/handle visuals for
        // real, exactly as a drag would, then reads back the clamped/
        // stepped value UiSliderBus itself computed (not the raw input)
        // before invoking the callback, mirroring what UiSliderComponent's
        // own DoChangedActions() does after a real drag release.
        UiSliderBus::Event(sliderId, &UiSliderBus::Events::SetValue, value);
        float actualValue = 0.0f;
        UiSliderBus::EventResult(actualValue, sliderId, &UiSliderBus::Events::GetValue);

        for (const auto& entry : m_sliderCallbacks)
        {
            if (entry.first == sliderName)
            {
                if (entry.second)
                {
                    entry.second(actualValue);
                }
                return;
            }
        }
    }

    float MainMenuPresentation::GetSliderValue(const char* sliderName) const
    {
        AZ::Entity* sliderEntity = nullptr;
        UiCanvasBus::EventResult(
            sliderEntity, m_canvasId, &UiCanvasBus::Events::FindElementByName, AZStd::string(sliderName));
        if (sliderEntity == nullptr)
        {
            return 0.0f;
        }
        float value = 0.0f;
        UiSliderBus::EventResult(value, sliderEntity->GetId(), &UiSliderBus::Events::GetValue);
        return value;
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
