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
    //! Crosshair, muzzle flash, and the hit-feedback ring remain
    //! debug-drawn for now - separate combat-feedback VFX work, not this
    //! pass's scope (see DrawPresentation() for what's left there).
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

        AZ::EntityId m_canvasId;
        AZ::EntityId m_healthLabel;
        AZ::EntityId m_weaponAmmoLabel;
        AZ::EntityId m_objectiveLabel;
    };
}
