#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/base.h>

namespace STWGameplay
{
    //! Read-only authoritative signals the first-person presentation reacts to.
    //! Every field is derived from PlayerSliceModel / WeaponState / PlayerInput; the
    //! presentation only consumes them and never writes gameplay state back. A rejected
    //! shot never sets m_shotFired, so it can never produce a fire presentation event.
    struct PresentationInput
    {
        bool m_shotFired = false;  //!< exactly one authoritative valid shot occurred this tick
        bool m_hit = false;        //!< an authoritative hit was registered this tick
        bool m_reloading = false;  //!< authoritative reload is in progress
        bool m_moving = false;     //!< player has locomotion intent
        bool m_sprinting = false;  //!< player is sprinting (implies moving)
    };

    enum class ViewmodelState
    {
        Idle,
        Move,
        Sprint,
        Fire,
        Reload
    };

    //! Deterministic first-person viewmodel presentation model. Pure gameplay-free logic:
    //! it owns only visual/feel state (locomotion state, bounded recoil, sway/bob, muzzle and
    //! hit cue timers) and reacts to authoritative events. It contains no ammo, magazine,
    //! reserve, target or health data and therefore structurally cannot alter gameplay.
    class ViewmodelPresentation
    {
    public:
        // Presentation-only tunables (no gameplay authority).
        static constexpr float FireCueDuration = 0.09f;    //!< s the Fire state overrides locomotion
        static constexpr float MuzzleFlashDuration = 0.05f; //!< s muzzle flash stays lit
        static constexpr float HitFeedbackDuration = 0.10f; //!< s hit marker stays lit
        static constexpr float RecoilKick = 0.05f;          //!< m local viewmodel kick per shot
        static constexpr float RecoilPitchKick = 0.06f;     //!< rad visual pitch kick per shot
        static constexpr float RecoilRecovery = 14.0f;      //!< 1/s recovery rate toward neutral
        static constexpr float MaxRecoil = 0.18f;           //!< m / rad recoil magnitude bound
        static constexpr float BobAmplitudeMove = 0.020f;   //!< m walk bob amplitude
        static constexpr float BobAmplitudeSprint = 0.045f; //!< m sprint bob amplitude
        static constexpr float BobFrequencyMove = 8.0f;     //!< rad/s walk bob frequency
        static constexpr float BobFrequencySprint = 12.0f;  //!< rad/s sprint bob frequency
        static constexpr float SwayReturn = 10.0f;          //!< 1/s sway blend rate

        //! Advance the presentation by one frame. Returns false and leaves state untouched on
        //! an invalid delta (transactional), matching PlayerSliceModel's contract.
        bool Update(float deltaTime, const PresentationInput& input);

        ViewmodelState GetState() const { return m_state; }
        const AZ::Vector3& GetRecoilOffset() const { return m_recoilOffset; }
        float GetRecoilPitch() const { return m_recoilPitch; }
        const AZ::Vector3& GetSwayOffset() const { return m_swayOffset; }
        bool IsMuzzleFlashActive() const { return m_muzzleFlash > 0.0f; }
        bool IsHitFeedbackActive() const { return m_hitFeedback > 0.0f; }
        AZ::u32 GetFireEventCount() const { return m_fireEvents; }
        AZ::u32 GetReloadStartCount() const { return m_reloadStarts; }

    private:
        ViewmodelState m_state = ViewmodelState::Idle;
        AZ::Vector3 m_recoilOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_swayOffset = AZ::Vector3::CreateZero();
        float m_recoilPitch = 0.0f;
        float m_bobPhase = 0.0f;
        float m_fireCue = 0.0f;
        float m_muzzleFlash = 0.0f;
        float m_hitFeedback = 0.0f;
        bool m_wasReloading = false;
        AZ::u32 m_fireEvents = 0;
        AZ::u32 m_reloadStarts = 0;
    };
}
