#include <STWGameplay/PlayerSliceModel.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        bool IsFinite(float value) { return std::isfinite(value); }
    }

    bool PlayerSliceModel::Update(float deltaTime, const PlayerInput& input)
    {
        if (!IsFinite(deltaTime) || deltaTime < 0.0f || !IsFinite(input.m_forward) || !IsFinite(input.m_strafe)
            || !IsFinite(input.m_lookX) || !IsFinite(input.m_lookY))
        {
            return false;
        }

        m_presentation.m_shotFired = false;
        m_presentation.m_hit = false;
        m_presentation.m_fireCueRemaining = AZStd::max(0.0f, m_presentation.m_fireCueRemaining - deltaTime);
        m_presentation.m_hitCueRemaining = AZStd::max(0.0f, m_presentation.m_hitCueRemaining - deltaTime);
        m_weapon.m_cooldownRemaining = AZStd::max(0.0f, m_weapon.m_cooldownRemaining - deltaTime);
        m_enemy.Update(deltaTime, m_player.m_position);

        if (m_weapon.m_reloading)
        {
            m_weapon.m_reloadRemaining -= deltaTime;
            if (m_weapon.m_reloadRemaining <= 0.0f)
            {
                FinishReload();
            }
        }

        m_player.m_yaw += input.m_lookX * LookSensitivity;
        m_player.m_pitch = AZStd::clamp(m_player.m_pitch - input.m_lookY * LookSensitivity, -PitchLimit, PitchLimit);

        if (input.m_reload)
        {
            StartReload();
        }
        if (input.m_fire)
        {
            TryFire();
        }
        return true;
    }

    bool PlayerSliceModel::TryFire()
    {
        if (!m_player.m_alive || m_weapon.m_reloading || m_weapon.m_cooldownRemaining > 0.0f || m_weapon.m_magazine <= 0)
        {
            return false;
        }
        --m_weapon.m_magazine;
        m_weapon.m_cooldownRemaining = FireInterval;
        m_presentation.m_shotFired = true;
        m_presentation.m_fireCueRemaining = 0.06f;
        if (m_enemy.GetState().m_alive && RayHitsTarget(GetEyePosition(), GetAimDirection()))
        {
            m_enemy.ApplyDamage(WeaponDamage);
            m_presentation.m_hit = true;
            m_presentation.m_hitCueRemaining = 0.12f;
        }
        return true;
    }

    bool PlayerSliceModel::StartReload()
    {
        if (!m_player.m_alive || m_weapon.m_reloading || m_weapon.m_magazine >= 30 || m_weapon.m_reserve <= 0)
        {
            return false;
        }
        m_weapon.m_reloading = true;
        m_weapon.m_reloadRemaining = ReloadDuration;
        return true;
    }

    AZ::Vector3 PlayerSliceModel::GetEyePosition() const
    {
        return m_player.m_position + AZ::Vector3(0.0f, 0.0f, EyeHeight);
    }

    AZ::Vector3 PlayerSliceModel::GetAimDirection() const
    {
        const float cosPitch = std::cos(m_player.m_pitch);
        return AZ::Vector3(
            std::sin(m_player.m_yaw) * cosPitch,
            std::cos(m_player.m_yaw) * cosPitch,
            std::sin(m_player.m_pitch)).GetNormalized();
    }

    AZ::Vector3 PlayerSliceModel::GetDesiredVelocity(const PlayerInput& input) const
    {
        const float forwardAmount = AZStd::clamp(input.m_forward, -1.0f, 1.0f);
        const float strafeAmount = AZStd::clamp(input.m_strafe, -1.0f, 1.0f);
        AZ::Vector3 movement(
            std::sin(m_player.m_yaw) * forwardAmount + std::cos(m_player.m_yaw) * strafeAmount,
            std::cos(m_player.m_yaw) * forwardAmount - std::sin(m_player.m_yaw) * strafeAmount,
            0.0f);
        if (movement.GetLengthSq() > 1.0f)
        {
            movement.Normalize();
        }
        return movement * (input.m_sprint ? SprintSpeed : WalkSpeed);
    }

    void PlayerSliceModel::SetPlayerPosition(const AZ::Vector3& position)
    {
        if (position.IsFinite())
        {
            m_player.m_position = position;
        }
    }

    void PlayerSliceModel::SynchronizePhysicalState(const AZ::Vector3& position, bool grounded)
    {
        if (position.IsFinite())
        {
            m_player.m_position = position;
            m_player.m_grounded = grounded;
        }
    }

    bool PlayerSliceModel::RayHitsTarget(const AZ::Vector3& origin, const AZ::Vector3& direction) const
    {
        const EnemyState& target = m_enemy.GetState();
        const AZ::Vector3 toTarget = target.m_position - origin;
        const float projected = toTarget.Dot(direction);
        if (projected < 0.0f || projected > WeaponRange)
        {
            return false;
        }
        const AZ::Vector3 closest = origin + direction * projected;
        return (closest - target.m_position).GetLengthSq() <= target.m_radius * target.m_radius;
    }

    void PlayerSliceModel::FinishReload()
    {
        const int needed = 30 - m_weapon.m_magazine;
        const int transferred = AZStd::min(needed, m_weapon.m_reserve);
        m_weapon.m_magazine += transferred;
        m_weapon.m_reserve -= transferred;
        m_weapon.m_reloading = false;
        m_weapon.m_reloadRemaining = 0.0f;
    }
}
