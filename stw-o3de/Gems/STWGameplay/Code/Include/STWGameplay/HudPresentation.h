#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/std/string/string.h>

namespace STWGameplay
{
    //! Presentation-only real-time HUD (health/weapon/ammo/state/objective),
    //! replacing the equivalent AzFramework::DebugDisplayRequestBus
    //! Draw2dTextLabel calls that used to live in
    //! STWGameplaySystemComponent::DrawPresentation() with a real LyShine
    //! canvas - the same programmatic construction technique proven in
    //! MainMenuPresentation (CreateChildElement + Deactivate() before
    //! CreateComponent(uuid), the exact fix for the zero-rect bug found
    //! there). A separate canvas from MainMenuPresentation's, not a 5th
    //! screen on it: this codebase's existing convention is one small
    //! *Presentation class per concern (EnvironmentPresentation,
    //! ArenaPresentation, BodycamCameraPresentation, ...), and the HUD is
    //! always-visible/independent of which menu screen is showing, unlike
    //! the menu's own screens which are mutually exclusive.
    //! Muzzle flash remains debug-drawn - it is a world-space combat VFX
    //! positioned near the viewmodel, not a 2D HUD stat, genuinely out of
    //! scope here (see DrawPresentation() for what's left there). Crosshair
    //! and hit-feedback marker ARE real LyShine elements (see
    //! SetCrosshairHitFeedback()).
    class HudPresentation final
    {
    public:
        ~HudPresentation();

        void Initialize();
        void Shutdown();
        bool IsReady() const { return m_canvasId.IsValid(); }

        //! Refreshes every displayed value for real, every frame - called
        //! from DrawPresentation() with the exact same text it used to hand
        //! to Draw2dTextLabel (same azsnprintf branching for charge/
        //! magazine/no-ammo weapon types), so the HUD's real content is
        //! unchanged, only its rendering mechanism is.
        void Update(const char* healthText, const char* weaponAmmoText, const char* objectiveText);

        //! Shows/hides the real hit-feedback marker (a small red frame
        //! around the crosshair) - replaces the debug-drawn
        //! DrawWireCircle2d that used to fire on the same condition
        //! (PresentationState::m_hitCueRemaining > 0 or
        //! ViewmodelPresentation::IsHitFeedbackActive()).
        void SetCrosshairHitFeedback(bool active);
        //! Reads the hit-feedback marker's real enabled state back through
        //! UiElementBus, for round-trip verification.
        bool IsHitFeedbackMarkerVisible() const;

        //! Reads a label's displayed text back through UiTextBus, for
        //! round-trip verification against what Update() actually set -
        //! same reasoning as MainMenuPresentation::GetControlLabel().
        AZStd::string GetLabelText(const char* labelName) const;
        //! Diagnostic only: prints the canvas's real logical size and the
        //! health label's actual computed on-screen rect via AZ_Printf,
        //! same reasoning as MainMenuPresentation::LogDiagnostics().
        void LogDiagnostics() const;

    private:
        AZ::EntityId CreateHudLabel(
            const char* name, float anchorLeft, float anchorTop, float anchorRight, float anchorBottom,
            float fontSize);
        AZ::EntityId CreateCrosshairPanel(
            const char* name, float anchorLeft, float anchorTop, float anchorRight, float anchorBottom,
            float offsetLeft, float offsetTop, float offsetRight, float offsetBottom, float r, float g, float b,
            float a);
        void BuildCrosshair();

        AZ::EntityId m_canvasId;
        AZ::EntityId m_healthLabel;
        AZ::EntityId m_weaponAmmoLabel;
        AZ::EntityId m_objectiveLabel;
        AZ::EntityId m_hitFeedbackMarker;
    };
}
