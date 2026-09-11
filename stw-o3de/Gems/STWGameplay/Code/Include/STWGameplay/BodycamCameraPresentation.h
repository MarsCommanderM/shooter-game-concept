#pragma once

#include <AzCore/Math/Vector3.h>

namespace STWGameplay
{
    enum class BodycamPresentationProfile
    {
        Standard = 0,
        ReducedMotion
    };

    struct BodycamPresentationTuning
    {
        float m_lookInertiaScale = 0.55f;
        float m_lookResponse = 14.0f;
        float m_lookRecovery = 9.0f;
        float m_rollLimitRadians = 0.045f;
        float m_lookRollRadians = 0.010f;
        float m_rollResponse = 10.0f;
        float m_locomotionResponse = 9.0f;
        float m_locomotionFrequency = 9.0f;
        float m_locomotionVerticalMeters = 0.035f;
        float m_locomotionLateralMeters = 0.018f;
        float m_landingVerticalMeters = 0.050f;
        float m_airborneVerticalMeters = 0.018f;
        float m_mantleVerticalMeters = 0.060f;
        float m_adsMotionScale = 0.30f;
        float m_crouchMotionScale = 0.65f;
        float m_slideMotionScale = 1.15f;
        float m_accelerationLagMeters = 0.018f;
        float m_accelerationVerticalMeters = 0.006f;
        float m_accelerationResponse = 11.0f;
        float m_accelerationMaximum = 40.0f;
        float m_recoilPitchRadians = 0.014f;
        float m_recoilBackMeters = 0.010f;
        float m_recoilRecovery = 18.0f;
        float m_recoilMaximumPitchRadians = 0.040f;
        float m_recoilMaximumBackMeters = 0.030f;
        float m_hipFovDegrees = 60.0f;
        float m_adsFovDegrees = 52.0f;
    };

    struct BodycamPresentationInput
    {
        float m_lookX = 0.0f;
        float m_lookY = 0.0f;
        float m_speed = 0.0f;
        float m_lateralInput = 0.0f;
        bool m_sprinting = false;
        bool m_ads = false;
        bool m_grounded = true;
        bool m_crouched = false;
        bool m_sliding = false;
        bool m_mantling = false;
        float m_mantleProgress = 0.0f;
        bool m_alive = true;
        int m_respawnEvents = 0;
        AZ::Vector3 m_planarAcceleration = AZ::Vector3::CreateZero();
        // Copied from the authoritative accepted-shot presentation event, never raw trigger input.
        bool m_shotFired = false;
        float m_adsBlend = 0.0f;
    };

    //! Presentation-only body response layered onto the authoritative active camera.
    //! This class consumes a synchronized snapshot and never writes gameplay state.
    class BodycamCameraPresentation final
    {
    public:
        static BodycamPresentationTuning GetStandardTuning();
        static BodycamPresentationTuning GetReducedMotionTuning();
        static bool IsReducedMotionProfileEffective();

        void SetProfile(BodycamPresentationProfile profile);
        BodycamPresentationProfile GetProfile() const { return m_profile; }
        void Update(float deltaTime, const BodycamPresentationInput& input);
        void ResetToNeutral();

        const AZ::Vector3& GetCameraPositionOffset() const { return m_cameraPositionOffset; }
        const AZ::Vector3& GetCameraRotationOffset() const { return m_cameraRotationOffset; }
        float GetRollRadians() const { return m_cameraRotationOffset.GetZ(); }
        float GetLocomotionResponse() const { return m_locomotionBlend; }
        float GetLookInertiaMagnitude() const { return m_lookOffset.GetLength(); }
        float GetCameraFovDegrees() const { return m_cameraFovDegrees; }
        const AZ::Vector3& GetAccelerationOffset() const { return m_accelerationOffset; }
        float GetRecoilPitchRadians() const { return m_recoilPitch; }
        float GetRecoilBackMeters() const { return m_recoilBack; }
        float GetMotionMagnitude() const
        {
            return m_cameraPositionOffset.GetLength() + m_cameraRotationOffset.GetLength();
        }
        bool IsNearNeutral(float epsilon = 0.0001f) const;

        bool WasLookInertiaObserved() const { return m_lookInertiaObserved; }
        bool WasLocomotionResponseObserved() const { return m_locomotionResponseObserved; }
        bool WasRollResponseObserved() const { return m_rollResponseObserved; }
        bool WasVerticalResponseObserved() const { return m_verticalResponseObserved; }
        bool WasAdsSuppressionObserved() const { return m_adsSuppressionObserved; }
        bool WasResetToNeutralObserved() const { return m_resetToNeutralObserved; }
        bool WasAccelerationResponseObserved() const { return m_accelerationResponseObserved; }
        bool WasRecoilResponseObserved() const { return m_recoilResponseObserved; }

    private:
        static float ExponentialApproach(float current, float target, float response, float deltaTime);
        static float ClampUnit(float value);

        BodycamPresentationTuning GetTuning() const;
        void UpdateResponse(float deltaTime, const BodycamPresentationInput& input);

        BodycamPresentationProfile m_profile = BodycamPresentationProfile::Standard;
        AZ::Vector3 m_lookOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_accelerationOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_cameraPositionOffset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_cameraRotationOffset = AZ::Vector3::CreateZero();
        float m_recoilPitch = 0.0f;
        float m_recoilBack = 0.0f;
        float m_cameraFovDegrees = 60.0f;
        float m_roll = 0.0f;
        float m_locomotionBlend = 0.0f;
        float m_locomotionPhase = 0.0f;
        float m_landingPulse = 0.0f;
        bool m_havePreviousState = false;
        bool m_previousAlive = true;
        bool m_previousGrounded = true;
        int m_previousRespawnEvents = 0;
        bool m_lookInertiaObserved = false;
        bool m_locomotionResponseObserved = false;
        bool m_rollResponseObserved = false;
        bool m_verticalResponseObserved = false;
        bool m_adsSuppressionObserved = false;
        bool m_resetToNeutralObserved = false;
        bool m_accelerationResponseObserved = false;
        bool m_recoilResponseObserved = false;
    };
}
