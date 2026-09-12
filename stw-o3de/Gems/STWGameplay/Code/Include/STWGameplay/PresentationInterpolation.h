#pragma once

#include <AzCore/Math/Vector3.h>

#include <cmath>
#include <limits>

namespace STWGameplay
{
    //! Copy-only source values used to evaluate a render-frame presentation pose.
    //! No field in this type is an authority for gameplay, physics, or reconciliation.
    struct PresentationFrameState final
    {
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
    };

    //! Owns only the previous/current presentation source samples.
    //! Advance() is the explicit simulation/synchronization boundary; Evaluate() is read-only.
    class PresentationInterpolation final
    {
    public:
        bool HasState() const { return m_hasState; }

        bool Advance(const PresentationFrameState& sourceState)
        {
            if (!IsFinite(sourceState))
            {
                return false;
            }

            if (!m_hasState)
            {
                Reset(sourceState);
                return true;
            }

            m_previousState = m_currentState;
            m_currentState = sourceState;
            return true;
        }

        //! Hard-snap both samples to a discontinuity-safe source state.
        void Reset(const PresentationFrameState& sourceState)
        {
            if (!IsFinite(sourceState))
            {
                m_previousState = {};
                m_currentState = {};
                m_hasState = false;
                return;
            }

            m_previousState = sourceState;
            m_currentState = sourceState;
            m_hasState = true;
        }

        PresentationFrameState Evaluate(float alpha) const
        {
            if (!m_hasState)
            {
                return {};
            }

            const float safeAlpha = SanitizeAlpha(alpha);
            PresentationFrameState result;
            result.m_position = m_previousState.m_position
                + (m_currentState.m_position - m_previousState.m_position) * safeAlpha;
            result.m_yaw = InterpolateAngle(m_previousState.m_yaw, m_currentState.m_yaw, safeAlpha);
            result.m_pitch = InterpolateAngle(m_previousState.m_pitch, m_currentState.m_pitch, safeAlpha);
            return result;
        }

        const PresentationFrameState& GetPreviousState() const { return m_previousState; }
        const PresentationFrameState& GetCurrentState() const { return m_currentState; }

        static float SanitizeAlpha(float alpha)
        {
            if (!std::isfinite(alpha) || alpha <= 0.0f)
            {
                return alpha == std::numeric_limits<float>::infinity() ? 1.0f : 0.0f;
            }
            return alpha >= 1.0f ? 1.0f : alpha;
        }

        static float InterpolateAngle(float previous, float current, float alpha)
        {
            if (!std::isfinite(previous) || !std::isfinite(current))
            {
                return 0.0f;
            }

            constexpr float Pi = 3.14159265358979323846f;
            constexpr float TwoPi = 2.0f * Pi;
            const float delta = NormalizeAngle(current - previous, Pi, TwoPi);
            return NormalizeAngle(previous + delta * SanitizeAlpha(alpha), Pi, TwoPi);
        }

    private:
        static bool IsFinite(const PresentationFrameState& state)
        {
            return state.m_position.IsFinite()
                && std::isfinite(state.m_yaw)
                && std::isfinite(state.m_pitch);
        }

        static float NormalizeAngle(float angle, float pi, float twoPi)
        {
            float wrapped = std::fmod(angle + pi, twoPi);
            if (wrapped < 0.0f)
            {
                wrapped += twoPi;
            }
            return wrapped - pi;
        }

        bool m_hasState = false;
        PresentationFrameState m_previousState;
        PresentationFrameState m_currentState;
    };
}
