#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/base.h>

namespace STWGameplay
{
    struct CombatFeedbackInput
    {
        bool m_shotFired = false;
        bool m_hitConfirmed = false;
        AZ::Vector3 m_impactPosition = AZ::Vector3::CreateZero();
    };

    //! Presentation-only combat feedback driven by copied authoritative event signals.
    class CombatFeedbackPresentation
    {
    public:
        static constexpr float FireFlashDuration = 0.06f;
        static constexpr float EnemyHitDuration = 0.12f;
        static constexpr float ImpactDuration = 0.10f;
        static constexpr float FirePulseScale = 0.16f;
        static constexpr float EnemyHitScaleAmount = 0.10f;
        static constexpr float ImpactPulseScale = 0.12f;

        bool Update(float deltaTime, const CombatFeedbackInput& input);

        bool IsFireFlashVisible() const { return m_fireTimer > 0.0f; }
        bool IsEnemyHitVisible() const { return m_enemyHitTimer > 0.0f; }
        bool IsImpactVisible() const { return m_impactTimer > 0.0f; }
        float GetFireIntensity() const;
        float GetEnemyHitScale() const;
        float GetImpactScale() const;
        const AZ::Vector3& GetImpactPosition() const { return m_impactPosition; }
        AZ::u32 GetFireFeedbackCount() const { return m_fireFeedbackCount; }
        AZ::u32 GetHitFeedbackCount() const { return m_hitFeedbackCount; }
        AZ::u32 GetImpactFeedbackCount() const { return m_impactFeedbackCount; }

    private:
        float m_fireTimer = 0.0f;
        float m_enemyHitTimer = 0.0f;
        float m_impactTimer = 0.0f;
        AZ::Vector3 m_impactPosition = AZ::Vector3::CreateZero();
        AZ::u32 m_fireFeedbackCount = 0;
        AZ::u32 m_hitFeedbackCount = 0;
        AZ::u32 m_impactFeedbackCount = 0;
    };
}
