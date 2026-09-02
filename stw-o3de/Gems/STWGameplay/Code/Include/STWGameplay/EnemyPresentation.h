#pragma once

#include <AzCore/Math/Transform.h>
#include <STWGameplay/EnemyCombatModel.h>

namespace STWGameplay
{
    //! Gameplay-free visual adapter for STW_ENEMY_01. It consumes an immutable snapshot and
    //! produces only a local Atom transform; it cannot mutate model or PhysX authority.
    class EnemyPresentation
    {
    public:
        bool Update(float deltaTime, const EnemyState& state);
        void Reset();

        EnemyBehaviorState GetState() const { return m_state; }
        AZ::Transform GetLocalTransform() const;
        AZ::Vector3 GetScale() const { return m_scale; }
        int GetAttackReactionCount() const { return m_attackReactions; }
        int GetDamageReactionCount() const { return m_damageReactions; }
        int GetDeathReactionCount() const { return m_deathReactions; }
        int GetResetReactionCount() const { return m_resetReactions; }

    private:
        EnemyBehaviorState m_state = EnemyBehaviorState::Idle;
        AZ::Vector3 m_offset = AZ::Vector3::CreateZero();
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        float m_pitch = 0.0f;
        float m_phase = 0.0f;
        float m_attackImpulse = 0.0f;
        float m_damageImpulse = 0.0f;
        float m_deathBlend = 0.0f;
        int m_lastAttackEvents = 0;
        int m_lastDamageEvents = 0;
        int m_lastDeathEvents = 0;
        int m_lastRespawnEvents = 0;
        int m_attackReactions = 0;
        int m_damageReactions = 0;
        int m_deathReactions = 0;
        int m_resetReactions = 0;
    };
}
