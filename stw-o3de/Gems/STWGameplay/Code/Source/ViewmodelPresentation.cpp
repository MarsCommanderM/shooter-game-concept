#include <STWGameplay/ViewmodelPresentation.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        bool IsFiniteViewmodel(float value) { return std::isfinite(value); }
    }

    bool ViewmodelPresentation::Update(float deltaTime, const PresentationInput& input)
    {
        if (!IsFiniteViewmodel(deltaTime) || deltaTime < 0.0f)
        {
            return false;
        }

        // Cue timers decay first so a same-frame event re-arms them below.
        m_fireCue = AZStd::max(0.0f, m_fireCue - deltaTime);
        m_muzzleFlash = AZStd::max(0.0f, m_muzzleFlash - deltaTime);
        m_hitFeedback = AZStd::max(0.0f, m_hitFeedback - deltaTime);

        // Exactly one presentation reaction per authoritative valid shot. Because m_shotFired
        // is only ever set by an authoritative TryFire, a rejected/cooldown shot produces
        // nothing here.
        if (input.m_shotFired)
        {
            ++m_fireEvents;
            m_fireCue = FireCueDuration;
            m_muzzleFlash = MuzzleFlashDuration;
            m_recoilOffset += AZ::Vector3(0.0f, -RecoilKick, RecoilKick * 0.5f);
            m_recoilPitch += RecoilPitchKick;
        }
        if (input.m_hit)
        {
            m_hitFeedback = HitFeedbackDuration;
        }

        // Reload presentation begins on the rising edge of the authoritative reload state.
        if (input.m_reloading && !m_wasReloading)
        {
            ++m_reloadStarts;
        }
        m_wasReloading = input.m_reloading;

        // Deterministic recoil recovery toward neutral; bounded so it can never run away.
        const float recover = AZStd::clamp(RecoilRecovery * deltaTime, 0.0f, 1.0f);
        m_recoilOffset -= m_recoilOffset * recover;
        m_recoilPitch -= m_recoilPitch * recover;
        if (m_recoilOffset.GetLength() > MaxRecoil)
        {
            m_recoilOffset = m_recoilOffset.GetNormalized() * MaxRecoil;
        }
        m_recoilPitch = AZStd::clamp(m_recoilPitch, -MaxRecoil, MaxRecoil);

        // Movement-driven sway/bob, sprint-distinct, frame-rate independent, amplitude bounded.
        AZ::Vector3 targetSway = AZ::Vector3::CreateZero();
        if (input.m_moving)
        {
            const float amplitude = input.m_sprinting ? BobAmplitudeSprint : BobAmplitudeMove;
            const float frequency = input.m_sprinting ? BobFrequencySprint : BobFrequencyMove;
            m_bobPhase += frequency * deltaTime;
            targetSway = AZ::Vector3(
                std::sin(m_bobPhase) * amplitude,
                0.0f,
                std::abs(std::sin(m_bobPhase * 2.0f)) * amplitude * 0.5f);
        }
        else
        {
            m_bobPhase = 0.0f;
        }
        const float swayBlend = AZStd::clamp(SwayReturn * deltaTime, 0.0f, 1.0f);
        m_swayOffset += (targetSway - m_swayOffset) * swayBlend;

        // Locomotion/action state, precedence Reload > Fire > Sprint > Move > Idle. Reload wins
        // over Fire because authoritative firing is rejected while reloading; the short Fire cue
        // temporarily overrides locomotion, then transitions cleanly back.
        if (input.m_reloading)
        {
            m_state = ViewmodelState::Reload;
        }
        else if (m_fireCue > 0.0f)
        {
            m_state = ViewmodelState::Fire;
        }
        else if (input.m_sprinting)
        {
            m_state = ViewmodelState::Sprint;
        }
        else if (input.m_moving)
        {
            m_state = ViewmodelState::Move;
        }
        else
        {
            m_state = ViewmodelState::Idle;
        }

        return true;
    }
}
