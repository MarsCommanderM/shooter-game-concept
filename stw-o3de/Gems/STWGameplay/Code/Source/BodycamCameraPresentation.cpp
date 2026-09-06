#include <STWGameplay/BodycamCameraPresentation.h>

#include <AzCore/Math/MathUtils.h>

#include <algorithm>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        constexpr float LookSensitivityRadians = 0.0025f;
        constexpr float MotionEpsilon = 0.0001f;
        constexpr float MaximumDeltaTime = 0.10f;
        constexpr float MaximumLookInput = 32.0f;
        constexpr float MaximumSpeed = 8.5f;
        constexpr float TwoPi = 6.28318530717958647692f;
    }

    BodycamPresentationTuning BodycamCameraPresentation::GetStandardTuning()
    {
        return BodycamPresentationTuning{};
    }

    BodycamPresentationTuning BodycamCameraPresentation::GetReducedMotionTuning()
    {
        BodycamPresentationTuning tuning;
        tuning.m_lookInertiaScale = 0.20f;
        tuning.m_rollLimitRadians = 0.014f;
        tuning.m_lookRollRadians = 0.003f;
        tuning.m_locomotionVerticalMeters = 0.012f;
        tuning.m_locomotionLateralMeters = 0.007f;
        tuning.m_landingVerticalMeters = 0.018f;
        tuning.m_airborneVerticalMeters = 0.007f;
        tuning.m_mantleVerticalMeters = 0.022f;
        tuning.m_adsMotionScale = 0.20f;
        return tuning;
    }

    bool BodycamCameraPresentation::IsReducedMotionProfileEffective()
    {
        const BodycamPresentationTuning standard = GetStandardTuning();
        const BodycamPresentationTuning reduced = GetReducedMotionTuning();
        return reduced.m_lookInertiaScale < standard.m_lookInertiaScale * 0.5f
            && reduced.m_rollLimitRadians < standard.m_rollLimitRadians * 0.5f
            && reduced.m_locomotionVerticalMeters < standard.m_locomotionVerticalMeters * 0.5f
            && reduced.m_landingVerticalMeters < standard.m_landingVerticalMeters * 0.5f;
    }

    void BodycamCameraPresentation::SetProfile(BodycamPresentationProfile profile)
    {
        m_profile = profile;
        ResetToNeutral();
    }

    float BodycamCameraPresentation::ClampUnit(float value)
    {
        return std::clamp(value, -1.0f, 1.0f);
    }

    float BodycamCameraPresentation::ExponentialApproach(
        float current, float target, float response, float deltaTime)
    {
        if (!std::isfinite(current) || !std::isfinite(target))
        {
            return 0.0f;
        }
        const float safeDeltaTime = std::clamp(deltaTime, 0.0f, MaximumDeltaTime);
        const float weight = 1.0f - std::exp(-std::max(response, 0.0f) * safeDeltaTime);
        return current + (target - current) * weight;
    }

    BodycamPresentationTuning BodycamCameraPresentation::GetTuning() const
    {
        return m_profile == BodycamPresentationProfile::ReducedMotion
            ? GetReducedMotionTuning() : GetStandardTuning();
    }

    void BodycamCameraPresentation::ResetToNeutral()
    {
        m_lookOffset = AZ::Vector3::CreateZero();
        m_cameraPositionOffset = AZ::Vector3::CreateZero();
        m_cameraRotationOffset = AZ::Vector3::CreateZero();
        m_roll = 0.0f;
        m_locomotionBlend = 0.0f;
        m_locomotionPhase = 0.0f;
        m_landingPulse = 0.0f;
        m_havePreviousState = false;
        m_resetToNeutralObserved = true;
    }

    bool BodycamCameraPresentation::IsNearNeutral(float epsilon) const
    {
        return m_cameraPositionOffset.GetLengthSq() <= epsilon * epsilon
            && m_cameraRotationOffset.GetLengthSq() <= epsilon * epsilon;
    }

    void BodycamCameraPresentation::Update(float deltaTime, const BodycamPresentationInput& input)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            return;
        }

        const bool respawned = m_havePreviousState
            && ((input.m_respawnEvents != m_previousRespawnEvents)
                || (!m_previousAlive && input.m_alive));
        if (!input.m_alive || respawned)
        {
            ResetToNeutral();
            m_havePreviousState = true;
            m_previousAlive = input.m_alive;
            m_previousGrounded = input.m_grounded;
            m_previousRespawnEvents = input.m_respawnEvents;
            return;
        }

        if (!m_havePreviousState)
        {
            m_havePreviousState = true;
            m_previousAlive = input.m_alive;
            m_previousGrounded = input.m_grounded;
            m_previousRespawnEvents = input.m_respawnEvents;
        }

        UpdateResponse(deltaTime, input);
        m_previousAlive = input.m_alive;
        m_previousGrounded = input.m_grounded;
        m_previousRespawnEvents = input.m_respawnEvents;
    }

    void BodycamCameraPresentation::UpdateResponse(
        float deltaTime, const BodycamPresentationInput& input)
    {
        const BodycamPresentationTuning tuning = GetTuning();
        const float safeDeltaTime = std::clamp(deltaTime, 0.0f, MaximumDeltaTime);
        const float lookX = std::clamp(input.m_lookX, -MaximumLookInput, MaximumLookInput);
        const float lookY = std::clamp(input.m_lookY, -MaximumLookInput, MaximumLookInput);
        const float adsScale = input.m_ads ? tuning.m_adsMotionScale : 1.0f;
        const float speedBlend = ClampUnit(std::max(input.m_speed, 0.0f) / MaximumSpeed);
        const float locomotionScale = input.m_sliding ? tuning.m_slideMotionScale
            : (input.m_crouched ? tuning.m_crouchMotionScale : 1.0f);
        const float targetLocomotion = ClampUnit(speedBlend * locomotionScale);

        const AZ::Vector3 lookTarget(
            -lookY * LookSensitivityRadians * tuning.m_lookInertiaScale * adsScale,
            lookX * LookSensitivityRadians * tuning.m_lookInertiaScale * adsScale,
            0.0f);
        m_lookOffset = AZ::Vector3(
            ExponentialApproach(m_lookOffset.GetX(), lookTarget.GetX(), tuning.m_lookResponse, safeDeltaTime),
            ExponentialApproach(m_lookOffset.GetY(), lookTarget.GetY(), tuning.m_lookResponse, safeDeltaTime),
            0.0f);
        m_lookOffset *= std::exp(-tuning.m_lookRecovery * safeDeltaTime * (input.m_ads ? 1.35f : 1.0f));
        m_lookInertiaObserved = m_lookInertiaObserved
            || (std::abs(lookX) + std::abs(lookY) > 0.0f && m_lookOffset.GetLength() > MotionEpsilon);

        m_locomotionBlend = ExponentialApproach(
            m_locomotionBlend, targetLocomotion, tuning.m_locomotionResponse, safeDeltaTime);
        if (m_locomotionBlend > MotionEpsilon)
        {
            m_locomotionPhase = std::fmod(
                m_locomotionPhase + safeDeltaTime * tuning.m_locomotionFrequency
                    * (input.m_sprinting ? 1.25f : 1.0f), TwoPi);
            m_locomotionResponseObserved = true;
        }
        else
        {
            m_locomotionPhase = std::fmod(m_locomotionPhase, TwoPi);
        }

        if (!m_previousGrounded && input.m_grounded)
        {
            m_landingPulse = 1.0f;
            m_verticalResponseObserved = true;
        }
        m_landingPulse = std::max(0.0f, m_landingPulse - safeDeltaTime * 4.0f);

        const float lateral = ClampUnit(input.m_lateralInput);
        const float movementRoll = -lateral * tuning.m_rollLimitRadians * m_locomotionBlend;
        const float lookRoll = lookX * LookSensitivityRadians * tuning.m_lookRollRadians * 10.0f;
        const float targetRoll = std::clamp((movementRoll + lookRoll) * adsScale,
            -tuning.m_rollLimitRadians, tuning.m_rollLimitRadians);
        m_roll = ExponentialApproach(m_roll, targetRoll, tuning.m_rollResponse, safeDeltaTime);
        m_roll = std::clamp(m_roll, -tuning.m_rollLimitRadians, tuning.m_rollLimitRadians);
        m_rollResponseObserved = m_rollResponseObserved || std::abs(m_roll) > MotionEpsilon;

        const float locomotionWave = std::sin(m_locomotionPhase);
        const float lateralWave = std::sin(m_locomotionPhase * 0.5f);
        const float verticalOffset = locomotionWave * tuning.m_locomotionVerticalMeters * m_locomotionBlend * adsScale
            - m_landingPulse * tuning.m_landingVerticalMeters * adsScale
            + (!input.m_grounded ? -tuning.m_airborneVerticalMeters * adsScale : 0.0f)
            + (input.m_mantling
                    ? std::sin(std::clamp(input.m_mantleProgress, 0.0f, 1.0f) * 3.14159265f)
                        * tuning.m_mantleVerticalMeters * adsScale
                    : 0.0f)
            + (input.m_crouched ? -0.012f * adsScale : 0.0f);
        const float lateralOffset = lateralWave * tuning.m_locomotionLateralMeters * m_locomotionBlend * adsScale;
        m_cameraPositionOffset = AZ::Vector3(lateralOffset, 0.0f, verticalOffset);
        m_cameraRotationOffset = AZ::Vector3(m_lookOffset.GetX(), m_lookOffset.GetY(), m_roll);

        if (input.m_ads && adsScale < 1.0f)
        {
            m_adsSuppressionObserved = true;
        }
        m_verticalResponseObserved = m_verticalResponseObserved
            || std::abs(verticalOffset) > MotionEpsilon || input.m_mantling || !input.m_grounded;
    }
}
