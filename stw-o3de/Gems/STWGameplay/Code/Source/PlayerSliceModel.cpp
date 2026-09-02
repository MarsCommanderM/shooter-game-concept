#include <STWGameplay/PlayerSliceModel.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        bool IsFinite(float value) { return std::isfinite(value); }

        AZ::Vector3 GetPlanarDirection(float yaw, const PlayerInput& input)
        {
            const float forwardAmount = AZStd::clamp(input.m_forward, -1.0f, 1.0f);
            const float strafeAmount = AZStd::clamp(input.m_strafe, -1.0f, 1.0f);
            AZ::Vector3 direction(
                std::sin(yaw) * forwardAmount + std::cos(yaw) * strafeAmount,
                std::cos(yaw) * forwardAmount - std::sin(yaw) * strafeAmount,
                0.0f);
            if (direction.GetLengthSq() > 1.0f)
            {
                direction.Normalize();
            }
            return direction;
        }
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
        m_jumpImpulseThisTick = 0.0f;
        m_player.m_mantleRequested = false;
        m_presentation.m_fireCueRemaining = AZStd::max(0.0f, m_presentation.m_fireCueRemaining - deltaTime);
        m_presentation.m_hitCueRemaining = AZStd::max(0.0f, m_presentation.m_hitCueRemaining - deltaTime);
        m_weapon.m_cooldownRemaining = AZStd::max(0.0f, m_weapon.m_cooldownRemaining - deltaTime);
        m_enemy.Update(deltaTime, m_player.m_position, m_player.m_alive);
        if (m_player.m_alive && m_enemy.TryAttackPlayer())
        {
            ApplyDamage(EnemyCombatModel::AttackDamage);
        }

        if (m_weapon.m_reloading)
        {
            m_weapon.m_reloadRemaining -= deltaTime;
            if (m_weapon.m_reloadRemaining <= 0.0f)
            {
                FinishReload();
            }
        }

        const bool newJumpPress = input.m_jump && !m_jumpWasHeld;
        const bool newCrouchPress = input.m_crouch && !m_crouchWasHeld;
        const bool newMantlePress = input.m_mantle && !m_mantleWasHeld;
        m_jumpWasHeld = input.m_jump;
        m_crouchWasHeld = input.m_crouch;
        m_mantleWasHeld = input.m_mantle;

        if (!m_player.m_alive)
        {
            m_player.m_slideActive = false;
            m_player.m_slideElapsed = 0.0f;
            m_player.m_slideSpeed = 0.0f;
            m_player.m_slideDirection = AZ::Vector3::CreateZero();
            m_player.m_mantleActive = false;
            m_player.m_mantleElapsed = 0.0f;
            m_player.m_mantleDirection = AZ::Vector3::CreateZero();
            return true;
        }

        const AZ::Vector3 planarDirection = GetPlanarDirection(m_player.m_yaw, input);
        const bool meaningfulPlanarInput = planarDirection.GetLengthSq()
            >= SlideInputThreshold * SlideInputThreshold;
        if (newMantlePress && m_player.m_grounded && meaningfulPlanarInput && !m_player.m_slideActive
            && !m_player.m_mantleActive)
        {
            m_player.m_mantleRequested = true;
        }
        if (m_player.m_mantleActive)
        {
            if (!m_player.m_grounded || !meaningfulPlanarInput)
            {
                m_player.m_mantleActive = false;
            }
            else
            {
                m_player.m_mantleElapsed = AZStd::min(MantleDuration, m_player.m_mantleElapsed + deltaTime);
                if (m_player.m_mantleElapsed >= MantleDuration)
                {
                    m_player.m_mantleActive = false;
                }
            }
            if (!m_player.m_mantleActive)
            {
                m_player.m_mantleElapsed = 0.0f;
                m_player.m_mantleDirection = AZ::Vector3::CreateZero();
            }
        }
        const bool slideStarted = newCrouchPress && m_player.m_grounded && meaningfulPlanarInput
            && !m_player.m_slideActive;
        if (slideStarted)
        {
            m_player.m_slideActive = true;
            m_player.m_slideElapsed = 0.0f;
            m_player.m_slideSpeed = SlideStartSpeed;
            m_player.m_slideDirection = planarDirection.GetNormalized();
            ++m_player.m_slideEvents;
        }
        else if (m_player.m_slideActive)
        {
            if (!m_player.m_grounded || !meaningfulPlanarInput)
            {
                m_player.m_slideActive = false;
            }
            else
            {
                m_player.m_slideElapsed = AZStd::min(SlideDuration, m_player.m_slideElapsed + deltaTime);
                const float decay = m_player.m_slideElapsed / SlideDuration;
                m_player.m_slideSpeed = SlideStartSpeed + (SlideEndSpeed - SlideStartSpeed) * decay;
                if (m_player.m_slideElapsed >= SlideDuration || m_player.m_slideSpeed <= SlideEndSpeed)
                {
                    m_player.m_slideActive = false;
                }
            }
            if (!m_player.m_slideActive)
            {
                m_player.m_slideSpeed = 0.0f;
                m_player.m_slideDirection = AZ::Vector3::CreateZero();
            }
        }

        // The model owns crouch intent only. PhysX decides whether the requested physical
        // controller transition can be applied, and airborne input cannot introduce one.
        if (m_player.m_grounded)
        {
            m_player.m_crouchDesired = input.m_crouch || m_player.m_slideActive || m_player.m_mantleActive;
        }

        if (newJumpPress && m_player.m_grounded && !m_player.m_slideActive && !m_player.m_mantleActive)
        {
            m_jumpImpulseThisTick = JumpImpulseSpeed;
            ++m_player.m_jumpEvents;
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

    void PlayerSliceModel::BeginMantle(const AZ::Vector3& direction)
    {
        if (!m_player.m_mantleRequested || m_player.m_mantleActive || !direction.IsFinite())
        {
            return;
        }
        const AZ::Vector3 planarDirection(direction.GetX(), direction.GetY(), 0.0f);
        if (planarDirection.GetLengthSq() < 0.01f)
        {
            m_player.m_mantleRequested = false;
            return;
        }
        m_player.m_mantleRequested = false;
        m_player.m_mantleActive = true;
        m_player.m_mantleElapsed = 0.0f;
        m_player.m_mantleDirection = planarDirection.GetNormalized();
        ++m_player.m_mantleEvents;
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

    bool PlayerSliceModel::ApplyDamage(float damage)
    {
        if (!m_player.m_alive || !IsFinite(damage) || damage <= 0.0f)
        {
            return false;
        }
        m_player.m_health = AZStd::max(0.0f, m_player.m_health - damage);
        ++m_player.m_damageEvents;
        if (m_player.m_health <= 0.0f)
        {
            m_player.m_alive = false;
            ++m_player.m_deathEvents;
        }
        return true;
    }

    void PlayerSliceModel::ResetPlayer()
    {
        const int damageEvents = m_player.m_damageEvents;
        const int deathEvents = m_player.m_deathEvents;
        const int respawnEvents = m_player.m_respawnEvents + 1;
        m_player = {};
        m_player.m_damageEvents = damageEvents;
        m_player.m_deathEvents = deathEvents;
        m_player.m_respawnEvents = respawnEvents;
        m_weapon = {};
        m_presentation = {};
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
        if (!m_player.m_alive)
        {
            return AZ::Vector3::CreateZero();
        }
        AZ::Vector3 movement = GetPlanarDirection(m_player.m_yaw, input);
        AZ::Vector3 velocity;
        if (m_player.m_mantleActive)
        {
            velocity = m_player.m_mantleDirection * MantleSpeed;
        }
        else if (m_player.m_slideActive)
        {
            velocity = m_player.m_slideDirection * m_player.m_slideSpeed;
        }
        else
        {
            velocity = movement * (input.m_sprint ? SprintSpeed : WalkSpeed);
        }
        velocity.SetZ(m_jumpImpulseThisTick);
        return velocity;
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
