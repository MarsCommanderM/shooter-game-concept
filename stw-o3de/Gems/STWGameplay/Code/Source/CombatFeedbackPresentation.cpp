#include <STWGameplay/CombatFeedbackPresentation.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    bool CombatFeedbackPresentation::Update(float deltaTime, const CombatFeedbackInput& input)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f
            || (input.m_hitConfirmed && !input.m_impactPosition.IsFinite()))
        {
            return false;
        }

        m_fireTimer = AZStd::max(0.0f, m_fireTimer - deltaTime);
        m_enemyHitTimer = AZStd::max(0.0f, m_enemyHitTimer - deltaTime);
        m_impactTimer = AZStd::max(0.0f, m_impactTimer - deltaTime);

        if (input.m_shotFired)
        {
            m_fireTimer = FireFlashDuration;
            ++m_fireFeedbackCount;
        }
        if (input.m_hitConfirmed)
        {
            m_enemyHitTimer = EnemyHitDuration;
            m_impactTimer = ImpactDuration;
            m_impactPosition = input.m_impactPosition;
            ++m_hitFeedbackCount;
            ++m_impactFeedbackCount;
        }
        return true;
    }

    float CombatFeedbackPresentation::GetFireIntensity() const
    {
        return AZStd::clamp(m_fireTimer / FireFlashDuration, 0.0f, 1.0f);
    }

    float CombatFeedbackPresentation::GetEnemyHitScale() const
    {
        const float intensity = AZStd::clamp(m_enemyHitTimer / EnemyHitDuration, 0.0f, 1.0f);
        return 1.0f + EnemyHitScaleAmount * intensity;
    }

    float CombatFeedbackPresentation::GetImpactScale() const
    {
        const float intensity = AZStd::clamp(m_impactTimer / ImpactDuration, 0.0f, 1.0f);
        return ImpactPulseScale * intensity;
    }
}
