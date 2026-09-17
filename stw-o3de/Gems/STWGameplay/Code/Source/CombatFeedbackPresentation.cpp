#include <STWGameplay/CombatFeedbackPresentation.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    void CombatFeedbackPresentation::Reset()
    {
        m_fireTimer = 0.0f;
        m_enemyHitTimer = 0.0f;
        m_impactTimer = 0.0f;
        m_impactPosition = AZ::Vector3::CreateZero();
    }

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

    float CombatFeedbackPresentation::GetRenderableFireScale() const
    {
        // Hidden Atom meshes must keep a well-conditioned transform. A tiny uniform
        // scale such as 0.001 produces a determinant below O3DE's inverse-matrix
        // tolerance even though it is technically non-zero. Visibility already owns
        // whether the feedback renders, so keep the hidden handle at identity scale.
        if (!IsFireFlashVisible())
        {
            return 1.0f;
        }
        return AZStd::max(MinimumRenderableScale, FirePulseScale * GetFireIntensity());
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

    float CombatFeedbackPresentation::GetRenderableImpactScale() const
    {
        if (!IsImpactVisible())
        {
            return 1.0f;
        }
        return AZStd::max(MinimumRenderableScale, GetImpactScale());
    }
}
