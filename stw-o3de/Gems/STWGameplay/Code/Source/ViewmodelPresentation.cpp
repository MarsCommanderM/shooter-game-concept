#include <STWGameplay/ViewmodelPresentation.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        bool IsFiniteViewmodel(float value) { return std::isfinite(value); }

        float ExpBlend(float rate, float deltaTime)
        {
            return 1.0f - std::exp(-rate * deltaTime);
        }
    }

    AZ::Vector3 ViewmodelPresentation::GetPoseOffset() const
    {
        const AZ::Vector3 hip(HipPoseRight, HipPoseForward, HipPoseUp);
        const AZ::Vector3 ads(AdsPoseRight, AdsPoseForward, AdsPoseUp);
        const AZ::Vector3 sprint(SprintPoseRight, SprintPoseForward, SprintPoseUp);
        const AZ::Vector3 base = hip + (ads - hip) * m_adsBlend;
        return base + (sprint - hip) * m_sprintBlend;
    }

    float ViewmodelPresentation::GetCameraFovDegrees() const
    {
        return HipCameraFovDegrees + (AdsCameraFovDegrees - HipCameraFovDegrees) * m_adsBlend;
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

        // Exact exponential decay makes recovery independent of how a time interval is split
        // across frames. Recovery happens before a new impulse, so the firing tick exposes the
        // complete kick immediately.
        const float recover = ExpBlend(RecoilRecovery, deltaTime);
        m_recoilOffset -= m_recoilOffset * recover;
        m_recoilPitch -= m_recoilPitch * recover;

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

        // Repeated authoritative shots accumulate, with a hard presentation-only bound.
        if (m_recoilOffset.GetLength() > MaxRecoil)
        {
            m_recoilOffset = m_recoilOffset.GetNormalized() * MaxRecoil;
        }
        m_recoilPitch = AZStd::clamp(m_recoilPitch, -MaxRecoil, MaxRecoil);

        // ADS-ready presentation architecture only. No input is currently bound to this signal,
        // so production remains at the stable hip pose until an authoritative adapter opts in.
        const float adsTarget = input.m_adsRequested ? 1.0f : 0.0f;
        m_adsBlend += (adsTarget - m_adsBlend) * ExpBlend(AdsBlendRate, deltaTime);
        m_adsBlend = AZStd::clamp(m_adsBlend, 0.0f, 1.0f);
        if (std::abs(adsTarget - m_adsBlend) <= AdsEndpointEpsilon)
        {
            m_adsBlend = adsTarget;
        }

        const float sprintTarget = (input.m_sprinting && m_adsBlend < 1.0f) ? 1.0f : 0.0f;
        m_sprintBlend += (sprintTarget - m_sprintBlend) * ExpBlend(SprintBlendRate, deltaTime);
        m_sprintBlend = AZStd::clamp(m_sprintBlend, 0.0f, 1.0f);
        if (m_adsBlend >= 1.0f)
        {
            m_sprintBlend = 0.0f;
        }
        if (std::abs(sprintTarget - m_sprintBlend) <= AdsEndpointEpsilon)
        {
            m_sprintBlend = sprintTarget;
        }

        // Movement-driven bob is target-smoothed and ADS attenuated. The phase is wrapped so
        // prolonged movement cannot accumulate floating-point drift.
        AZ::Vector3 targetSway = AZ::Vector3::CreateZero();
        if (input.m_moving)
        {
            const float amplitude = input.m_sprinting ? BobAmplitudeSprint : BobAmplitudeMove;
            const float frequency = input.m_sprinting ? BobFrequencySprint : BobFrequencyMove;
            m_bobPhase += frequency * deltaTime;
            m_bobPhase = std::fmod(m_bobPhase, 2.0f * AZ::Constants::Pi);
            const float adsBobScale = 1.0f - m_adsBlend * (1.0f - AdsBobMultiplier);
            targetSway = AZ::Vector3(
                std::sin(m_bobPhase) * amplitude * adsBobScale,
                0.0f,
                std::abs(std::sin(m_bobPhase * 2.0f)) * amplitude * 0.5f * adsBobScale);
        }
        const float bobBlend = ExpBlend(BobReturn, deltaTime);
        m_bobOffset += (targetSway - m_bobOffset) * bobBlend;
        if (!input.m_moving && m_bobOffset.GetLengthSq() < 0.00000001f)
        {
            m_bobOffset = AZ::Vector3::CreateZero();
        }

        if (!IsFiniteViewmodel(input.m_lookX) || !IsFiniteViewmodel(input.m_lookY))
        {
            return false;
        }
        const float adsSwayScale = 1.0f - m_adsBlend * (1.0f - AdsSwayMultiplier);
        const AZ::Vector3 targetLookSway(
            -input.m_lookX * LookSwayScale * adsSwayScale,
            0.0f,
            input.m_lookY * LookSwayScale * adsSwayScale);
        const float swayBlend = ExpBlend(SwayReturn, deltaTime);
        m_swayOffset += (targetLookSway - m_swayOffset) * swayBlend;
        if (m_swayOffset.GetLength() > MaxSway)
        {
            m_swayOffset = m_swayOffset.GetNormalized() * MaxSway;
        }

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
