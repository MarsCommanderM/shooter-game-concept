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
        Multiplayer,
        //! Shared Niederlage/Sieg overlay - one screen, not two, since the
        //! only real difference between defeat and victory content is text
        //! (title/message), set via SetEndGameContent(). A single "WEITER"
        //! button always dismisses back to Main; the caller
        //! (STWGameplaySystemComponent) decides what "continue" means for
        //! whichever real game state is actually active at click time.
        EndGame
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

        //! Shows the given screen AND makes the whole canvas visible again
        //! (see SetCanvasEnabled()) - calling ShowScreen always means
        //! "the player should see the menu now", the one exception being
        //! the explicit SPIELEN action, which is the only thing that hides
        //! the canvas again.
        void ShowScreen(MainMenuScreen screen);
        MainMenuScreen GetActiveScreen() const { return m_activeScreen; }
        //! Hides or shows the ENTIRE canvas (every screen at once), via the
        //! real UiCanvasBus::SetEnabled - not per-element visibility, the
        //! same mechanism LyShine itself uses to take a whole canvas out of
        //! rendering/input. Exposed so the caller (STWGameplaySystemComponent)
        //! can hide the menu the instant SPIELEN is clicked, matching the
        //! standard "menu blocks the game until Play is pressed" convention
        //! the original request assumed and this system did not previously
        //! implement - the menu canvas stayed permanently enabled and never
        //! actually gated real play.
        void SetCanvasEnabled(bool enabled);
        bool IsCanvasEnabled() const;
        //! Forces LyShine to recompute element rects immediately rather than
        //! waiting for its own lazy/automatic pass - see Initialize()'s
        //! comment for why this is not optional.
        void RecomputeLayout() const;

        //! Exposed for automated acceptance verification only - never
        //! written to by gameplay, read-only reflection of what was built.
        size_t GetButtonCount() const { return m_buttonCount; }
        bool WasEveryButtonClickTested() const;
        void TestClick(const char* buttonName);
        //! Drives a slider's real UiSliderBus::SetValue (so the fill/handle
        //! visuals update for real, exactly like a drag would), then invokes
        //! the callback CreateSlider registered with the clamped value
        //! UiSliderBus itself reports back via GetValue() - not a value the
        //! caller made up.
        void TestSliderChange(const char* sliderName, float value);
        //! Reads a slider's real current value back through UiSliderBus, for
        //! round-trip verification (e.g. "did the value the UI reports match
        //! what the subsystem it drives actually received").
        float GetSliderValue(const char* sliderName) const;

        //! Registers the callback invoked when a control-rebind button is
        //! clicked, with the actionId passed to CreateControlRow(). May be
        //! set before or after Initialize() - each rebind button reads this
        //! member at click time, not at build time.
        void SetControlsRebindHandler(AZStd::function<void(const char*)> handler);
        //! Registers the callback invoked when the Kontrast slider moves,
        //! same reason and same "may be set before or after Initialize()"
        //! rule as SetControlsRebindHandler().
        void SetContrastChangeHandler(AZStd::function<void(float)> handler);
        //! Registers the callback invoked when the real SPIELEN (Play)
        //! button on the Main screen is clicked - same "may be set before
        //! or after Initialize()" rule as the other handler setters. This
        //! is the one real entry point from menu into actual play; the
        //! caller (STWGameplaySystemComponent) is the one that actually
        //! knows what "start playing" means for real game state (unblocking
        //! input, showing the HUD), this class only reports the click.
        void SetPlayHandler(AZStd::function<void()> handler);
        //! Updates a button's displayed label text in place - used both to
        //! sync the initial key names after Initialize() and to reflect a
        //! real rebind the caller just captured.
        void SetControlLabel(const char* buttonName, const char* text);
        //! Reads a rebind button's displayed key name back through
        //! UiTextBus, for round-trip verification against what the caller
        //! actually set with SetControlLabel().
        AZStd::string GetControlLabel(const char* buttonName) const;
        //! Sets the End-Game overlay's title/message text (e.g. "NIEDERLAGE"
        //! / "Du bist gefallen." or "SIEG" / "Encounter abgeschlossen.") and
        //! switches to that screen. Content-only; ShowScreen() itself still
        //! decides what is enabled/visible.
        void SetEndGameContent(const char* title, const char* message);
        //! Registers the callback invoked by the End-Game screen's single
        //! "WEITER" button. May be set before or after Initialize(), same
        //! rule as the other handler setters.
        void SetEndGameContinueHandler(AZStd::function<void()> handler);
        //! Reads the End-Game screen's title back through UiTextBus, for
        //! round-trip verification against what SetEndGameContent() set.
        AZStd::string GetEndGameTitle() const;
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
        //! Builds a real, working LyShine slider (UiSliderComponentUuid) with
        //! its own Track/Fill/Manipulator child elements wired via
        //! UiSliderBus::SetTrackEntity/SetFillEntity/SetManipulatorEntity -
        //! not a decorative bar. onChange is invoked with the real, clamped
        //! GetValue() whenever the value changes, from real user drag
        //! interaction or from TestSliderChange().
        AZ::EntityId CreateSlider(AZ::EntityId parent, const char* name, const MenuRect& layout,
            float minValue, float maxValue, float initialValue, AZStd::function<void(float)> onChange);
        //! Builds one row of the STEUERUNG (key rebind) list: an action-name
        //! label plus a button that shows the currently-bound key and, when
        //! clicked, invokes the rebind handler registered via
        //! SetControlsRebindHandler() with actionId - this class owns no
        //! input-device knowledge itself, the caller (STWGameplaySystemComponent,
        //! which already owns AzFramework::InputChannelEventListener) does the
        //! actual capture and reports the result back via SetControlLabel().
        void CreateControlRow(AZ::EntityId parent, const char* actionId, const char* actionLabel,
            const char* buttonName, float top, float height);

        void BuildMainScreen();
        void BuildSettingsScreen();
        void BuildMultiplayerScreen();
        void BuildCampaignScreen();
        void BuildEndGameScreen();
        void SetScreenVisible(AZ::EntityId screenRoot, bool visible);

        AZ::EntityId m_canvasId;
        AZ::EntityId m_mainScreenRoot;
        AZ::EntityId m_settingsScreenRoot;
        AZ::EntityId m_multiplayerScreenRoot;
        AZ::EntityId m_campaignScreenRoot;
        AZ::EntityId m_endGameScreenRoot;
        MainMenuScreen m_activeScreen = MainMenuScreen::Main;
        size_t m_buttonCount = 0;
        AZStd::vector<AZStd::string> m_buttonNames;
        AZStd::vector<AZStd::string> m_clickedButtonNames;
        //! Test-only invocation path: TestClick() calls the stored callback
        //! directly rather than reaching into UiButtonBus's internal click
        //! simulation (unverified signature) - this only depends on code
        //! this class itself owns.
        AZStd::vector<AZStd::pair<AZStd::string, AZStd::function<void()>>> m_buttonCallbacks;
        //! Same TestClick()-style test-only invocation path, for sliders.
        AZStd::vector<AZStd::pair<AZStd::string, AZStd::function<void(float)>>> m_sliderCallbacks;
        AZStd::function<void(const char*)> m_controlsRebindHandler;
        AZStd::function<void(float)> m_contrastChangeHandler;
        AZStd::function<void()> m_endGameContinueHandler;
        AZStd::function<void()> m_playHandler;
    };
}
