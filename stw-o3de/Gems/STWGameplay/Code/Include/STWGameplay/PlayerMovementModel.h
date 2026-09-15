#pragma once

#include <AzCore/Math/Vector3.h>

namespace STWGameplay
{
    //! Tunable, renderer- and physics-independent parameters for horizontal player motion.
    struct PlayerMovementConfig
    {
        float m_walkSpeed = 4.5f;
        float m_sprintSpeed = 7.5f;
        float m_groundAcceleration = 32.0f;
        float m_groundDeceleration = 40.0f;
        float m_groundFriction = 10.0f;
        float m_airAcceleration = 8.0f;
        float m_airDeceleration = 3.0f;
        float m_maxAirSpeed = 7.5f;
    };

    //! Inputs consumed by the deterministic horizontal movement simulation.
    struct PlayerMovementInput
    {
        AZ::Vector3 m_direction = AZ::Vector3::CreateZero();
        bool m_sprint = false;
        bool m_grounded = false;
        bool m_alive = true;
    };

    struct PlayerMovementState
    {
        AZ::Vector3 m_velocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_planarAcceleration = AZ::Vector3::CreateZero();
    };

    //! Owns only the requested planar velocity. Collision and the physical transform remain
    //! owned by PhysXPlayerRuntime.
    class PlayerMovementModel final
    {
    public:
        PlayerMovementModel() = default;
        explicit PlayerMovementModel(const PlayerMovementConfig& config)
            : m_config(config)
        {
        }

        //! Advances the planar velocity by one deterministic gameplay tick.
        //! Invalid input leaves the current velocity unchanged and returns false.
        bool Update(float deltaTime, const PlayerMovementInput& input);

        void Reset();

        const PlayerMovementState& GetMovementState() const { return m_state; }
        const AZ::Vector3& GetVelocity() const { return m_state.m_velocity; }
        const PlayerMovementConfig& GetConfig() const { return m_config; }

    private:
        bool IsValidConfig() const;

        PlayerMovementConfig m_config;
        PlayerMovementState m_state;
    };
}
