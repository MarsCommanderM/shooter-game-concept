#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/function/function_template.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/utils.h>

namespace STWGameplay
{
    enum class MainMenuScreen
    {
        Main,
        Campaign,
        Settings,
        Multiplayer
    };

    //! Normalized parent-relative anchor rectangle (0..1) plus a pixel offset
    //! rectangle from those anchors - mirrors LyShine's own
    //! UiTransform2dInterface::Anchors/Offsets pair, kept as plain floats
    //! here so this header does not need to include LyShine's Bus headers.
    struct MenuRect
    {
        float m_anchorLeft = 0.0f;
        float m_anchorTop = 0.0f;
        float m_anchorRight = 0.0f;
        float m_anchorBottom = 0.0f;
        float m_offsetLeft = 0.0f;
        float m_offsetTop = 0.0f;
        float m_offsetRight = 0.0f;
        float m_offsetBottom = 0.0f;
    };

    //! Presentation-only main menu. Built entirely in code via LyShine's
    //! programmatic API (UiCanvasManagerBus::CreateCanvas + UiCanvasBus::
    //! CreateChildElement + AZ::Entity::CreateComponent<T>()) - the same
    //! entity/component construction pattern PhysXArenaRuntime already uses
    //! for static colliders, not a hand-authored .uicanvas asset. A .uicanvas
    //! is a verbose AZ::SerializeContext ObjectStream XML (a real 6-button
    //! example canvas in the engine is ~212 KB with dense GUID cross-
    //! references) - far too easy to silently corrupt by hand, with no way
    //! to validate it short of loading it, unlike code that either compiles
    //! and runs or doesn't.
    class MainMenuPresentation final
    {
    public:
        ~MainMenuPresentation();

        void Initialize();
        void Shutdown();
        bool IsReady() const { return m_canvasId.IsValid(); }

        void ShowScreen(MainMenuScreen screen);
        MainMenuScreen GetActiveScreen() const { return m_activeScreen; }

        //! Exposed for automated acceptance verification only - never
        //! written to by gameplay, read-only reflection of what was built.
        size_t GetButtonCount() const { return m_buttonCount; }
        bool WasEveryButtonClickTested() const;
        void TestClick(const char* buttonName);
        //! Diagnostic only: prints the canvas's real logical size and one
        //! button's actual computed on-screen rect via AZ_Printf, so a
        //! visual-vs-acceptance discrepancy can be root-caused from gate
        //! log evidence instead of guessed at.
        void LogDiagnostics() const;

    private:
        AZ::EntityId BuildScreenRoot(const char* name);
        AZ::EntityId CreatePanel(AZ::EntityId parent, const char* name,
            const MenuRect& layout, float r, float g, float b, float a);
        AZ::EntityId CreateLabel(AZ::EntityId parent, const char* name, const char* text,
            const MenuRect& layout, float fontSize, bool bold);
        AZ::EntityId CreateButton(AZ::EntityId parent, const char* name, const char* label,
            const MenuRect& layout, AZStd::function<void()> onClick);

        void BuildMainScreen();
        void BuildSettingsScreen();
        void BuildMultiplayerScreen();
        void BuildCampaignScreen();
        void SetScreenVisible(AZ::EntityId screenRoot, bool visible);

        AZ::EntityId m_canvasId;
        AZ::EntityId m_mainScreenRoot;
        AZ::EntityId m_settingsScreenRoot;
        AZ::EntityId m_multiplayerScreenRoot;
        AZ::EntityId m_campaignScreenRoot;
        MainMenuScreen m_activeScreen = MainMenuScreen::Main;
        size_t m_buttonCount = 0;
        AZStd::vector<AZStd::string> m_buttonNames;
        AZStd::vector<AZStd::string> m_clickedButtonNames;
        //! Test-only invocation path: TestClick() calls the stored callback
        //! directly rather than reaching into UiButtonBus's internal click
        //! simulation (unverified signature) - this only depends on code
        //! this class itself owns.
        AZStd::vector<AZStd::pair<AZStd::string, AZStd::function<void()>>> m_buttonCallbacks;
    };
}
