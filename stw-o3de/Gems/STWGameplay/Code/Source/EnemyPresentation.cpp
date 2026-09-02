#include <STWGameplay/EnemyPresentation.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    bool EnemyPresentation::Update(float deltaTime, const EnemyState& state)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            return false;
        }
        if (state.m_respawnEvents > m_lastRespawnEvents)
        {
            const int resets = state.m_respawnEvents - m_lastRespawnEvents;
            Reset();
            m_resetReactions += resets;
        }
        if (state.m_attackEvents > m_lastAttackEvents)
        {
            m_attackImpulse = 1.0f;
            m_attackReactions += state.m_attackEvents - m_lastAttackEvents;
        }
        if (state.m_damageEvents > m_lastDamageEvents)
        {
            m_damageImpulse = 1.0f;
            m_damageReactions += state.m_damageEvents - m_lastDamageEvents;
        }
        if (state.m_deathEvents > m_lastDeathEvents)
        {
            m_deathReactions += state.m_deathEvents - m_lastDeathEvents;
        }
        m_lastAttackEvents = state.m_attackEvents;
        m_lastDamageEvents = state.m_damageEvents;
        m_lastDeathEvents = state.m_deathEvents;
        m_lastRespawnEvents = state.m_respawnEvents;
        m_state = state.m_behaviorState;

        m_phase = std::fmod(m_phase + deltaTime * (m_state == EnemyBehaviorState::Chase ? 9.0f : 2.0f),
            2.0f * AZ::Constants::Pi);
        m_attackImpulse = AZStd::max(0.0f, m_attackImpulse - deltaTime * 7.0f);
        m_damageImpulse = AZStd::max(0.0f, m_damageImpulse - deltaTime * 9.0f);
        m_deathBlend = m_state == EnemyBehaviorState::Dead
            ? AZStd::min(1.0f, m_deathBlend + deltaTime * 2.5f)
            : 0.0f;

        const float idleLift = (m_state == EnemyBehaviorState::Idle || m_state == EnemyBehaviorState::Detect)
            ? std::sin(m_phase) * 0.025f : 0.0f;
        const float chaseBob = m_state == EnemyBehaviorState::Chase ? std::abs(std::sin(m_phase)) * 0.07f : 0.0f;
        m_offset = AZ::Vector3(m_damageImpulse * 0.08f, -m_attackImpulse * 0.16f, idleLift + chaseBob - m_deathBlend * 0.65f);
        m_pitch = (m_state == EnemyBehaviorState::Chase ? -0.12f : 0.0f)
            + m_attackImpulse * 0.18f + m_deathBlend * 1.35f;
        const float breathe = 1.0f + idleLift * 0.35f;
        m_scale = AZ::Vector3(breathe, breathe, 1.0f - m_deathBlend * 0.18f);
        return true;
    }

    void EnemyPresentation::Reset()
    {
        m_state = EnemyBehaviorState::Idle;
        m_offset = AZ::Vector3::CreateZero();
        m_scale = AZ::Vector3::CreateOne();
        m_pitch = 0.0f;
        m_phase = 0.0f;
        m_attackImpulse = 0.0f;
        m_damageImpulse = 0.0f;
        m_deathBlend = 0.0f;
    }

    AZ::Transform EnemyPresentation::GetLocalTransform() const
    {
        return AZ::Transform::CreateFromQuaternionAndTranslation(
            AZ::Quaternion::CreateRotationX(m_pitch), m_offset);
    }
}
