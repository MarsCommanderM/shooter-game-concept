#include <STWGameplay/PlayerMovementModel.h>

#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        bool IsFiniteNonNegative(float value)
        {
            return std::isfinite(value) && value >= 0.0f;
        }

        AZ::Vector3 ClampPlanarDirection(const AZ::Vector3& direction)
        {
            AZ::Vector3 planar(direction.GetX(), direction.GetY(), 0.0f);
            const float lengthSq = planar.GetLengthSq();
            if (lengthSq <= 1.0e-8f)
            {
                return AZ::Vector3::CreateZero();
            }
            if (lengthSq > 1.0f)
            {
                planar.Normalize();
            }
            return planar;
        }

        AZ::Vector3 MoveTowards(const AZ::Vector3& current, const AZ::Vector3& target, float maxDistanceDelta)
        {
            const AZ::Vector3 difference = target - current;
            const float distanceSq = difference.GetLengthSq();
            if (maxDistanceDelta <= 0.0f || distanceSq <= maxDistanceDelta * maxDistanceDelta)
            {
                return target;
            }
            return current + difference.GetNormalized() * maxDistanceDelta;
        }
    }

    bool PlayerMovementModel::Update(float deltaTime, const PlayerMovementInput& input)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f || !input.m_direction.IsFinite()
            || !IsValidConfig())
        {
            return false;
        }

        if (!input.m_alive)
        {
            m_state.m_velocity = AZ::Vector3::CreateZero();
            m_state.m_planarAcceleration = AZ::Vector3::CreateZero();
            return true;
        }

        const AZ::Vector3 direction = ClampPlanarDirection(input.m_direction);
        const bool hasInput = direction.GetLengthSq() > 1.0e-8f;
        const float requestedSpeed = input.m_sprint ? m_config.m_sprintSpeed : m_config.m_walkSpeed;
        const float targetSpeed = input.m_grounded
            ? requestedSpeed
            : AZStd::min(requestedSpeed, m_config.m_maxAirSpeed);
        const AZ::Vector3 targetVelocity = direction * targetSpeed;

        float responseRate = 0.0f;
        if (!hasInput)
        {
            responseRate = input.m_grounded
                ? m_config.m_groundDeceleration + m_config.m_groundFriction
                : m_config.m_airDeceleration;
        }
        else if (m_state.m_velocity.Dot(targetVelocity) < 0.0f)
        {
            // Crossing through zero first makes a reverse input responsive without adding a
            // second acceleration pass or allowing the two directions to compound speed.
            responseRate = input.m_grounded ? m_config.m_groundDeceleration : m_config.m_airDeceleration;
        }
        else
        {
            responseRate = input.m_grounded ? m_config.m_groundAcceleration : m_config.m_airAcceleration;
        }

        const AZ::Vector3 previousVelocity = m_state.m_velocity;
        m_state.m_velocity = MoveTowards(m_state.m_velocity, targetVelocity, responseRate * deltaTime);
        m_state.m_velocity.SetZ(0.0f);

        if (!input.m_grounded && m_state.m_velocity.GetLengthSq()
            > m_config.m_maxAirSpeed * m_config.m_maxAirSpeed)
        {
            m_state.m_velocity = m_state.m_velocity.GetNormalized() * m_config.m_maxAirSpeed;
        }
        m_state.m_planarAcceleration = deltaTime > 0.0f
            ? (m_state.m_velocity - previousVelocity) / deltaTime
            : AZ::Vector3::CreateZero();
        return true;
    }

    void PlayerMovementModel::Reset()
    {
        m_state = {};
    }

    bool PlayerMovementModel::IsValidConfig() const
    {
        return IsFiniteNonNegative(m_config.m_walkSpeed)
            && IsFiniteNonNegative(m_config.m_sprintSpeed)
            && IsFiniteNonNegative(m_config.m_groundAcceleration)
            && IsFiniteNonNegative(m_config.m_groundDeceleration)
            && IsFiniteNonNegative(m_config.m_groundFriction)
            && IsFiniteNonNegative(m_config.m_airAcceleration)
            && IsFiniteNonNegative(m_config.m_airDeceleration)
            && IsFiniteNonNegative(m_config.m_maxAirSpeed);
    }
}
