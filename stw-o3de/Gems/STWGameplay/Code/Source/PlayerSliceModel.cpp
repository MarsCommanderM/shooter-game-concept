#include <STWGameplay/PlayerSliceModel.h>

#include <cmath>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>

namespace STWGameplay
{
    namespace
    {
        bool IsFinite(float value) { return std::isfinite(value); }

        constexpr float PlayerTwoPi = 2.0f * AZ::Constants::Pi;

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

    PlayerSliceModel::PlayerSliceModel()
    {
        m_ownedEnemyCollection.emplace();
        m_enemyCollection = &m_ownedEnemyCollection.value();
    }

    PlayerSliceModel::PlayerSliceModel(EnemyCollectionModel& enemyCollection)
        : m_enemyCollection(&enemyCollection)
    {
    }

    bool PlayerSliceModel::Update(float deltaTime, const PlayerInput& input)
    {
        return UpdateInternal(deltaTime, input, true);
    }

    bool PlayerSliceModel::UpdateNetworkPlayer(float deltaTime, const PlayerInput& input)
    {
        return UpdateInternal(deltaTime, input, false);
    }

    bool PlayerSliceModel::UpdateInternal(float deltaTime, const PlayerInput& input, bool updateEnemySimulation)
    {
        if (!IsFinite(deltaTime) || deltaTime < 0.0f || !IsFinite(input.m_forward) || !IsFinite(input.m_strafe)
            || !IsFinite(input.m_lookX) || !IsFinite(input.m_lookY))
        {
            return false;
        }

        const EquipmentProfileId profileBeforeInput = GetActiveEquipmentProfileId();
        m_presentation.m_shotFired = false;
        m_presentation.m_hit = false;
        m_presentation.m_hitEnemyId = InvalidEnemyId;
        m_presentation.m_hitDestructible = false;
        m_presentation.m_hitDestructibleIndex = DestructibleObjectModel::MaxObjectCount;
        m_presentation.m_equipmentUsed = false;
        m_presentation.m_equipmentChanged = false;
        m_presentation.m_activeEquipmentProfile = profileBeforeInput;
        m_jumpImpulseThisTick = 0.0f;
        m_player.m_mantleRequested = false;
        m_presentation.m_fireCueRemaining = AZStd::max(0.0f, m_presentation.m_fireCueRemaining - deltaTime);
        m_presentation.m_hitCueRemaining = AZStd::max(0.0f, m_presentation.m_hitCueRemaining - deltaTime);
        m_invulnerabilityRemaining = AZStd::max(0.0f, m_invulnerabilityRemaining - deltaTime);
        if (!m_weapons.Update(deltaTime))
        {
            return false;
        }
        if (updateEnemySimulation)
        {
            GetEnemies().Update(deltaTime, m_player.m_position, m_player.m_alive);
            if (m_player.m_alive && m_applyLocalEnemyDamage)
            {
                for (size_t index = 0; index < GetEnemies().GetEnemyCount(); ++index)
                {
                    EnemyCombatModel& enemy = GetEnemies().GetInstanceByIndex(index).m_combat;
                    if (enemy.TryAttackPlayer())
                    {
                        ApplyDamage(enemy.GetProfile().m_attackDamage);
                    }
                }
            }
        }

        const bool newJumpPress = input.m_jump && !m_jumpWasHeld;
        const bool newCrouchPress = input.m_crouch && !m_crouchWasHeld;
        const bool newMantlePress = input.m_mantle && !m_mantleWasHeld;
        const bool newWeaponSwitch = input.m_switchWeapon && !m_weaponSwitchWasHeld;
        const bool newEquipmentSlotRequest = input.m_requestedEquipmentSlot >= 0
            && input.m_requestedEquipmentSlot != m_requestedEquipmentSlotWasHeld;
        m_jumpWasHeld = input.m_jump;
        m_crouchWasHeld = input.m_crouch;
        m_mantleWasHeld = input.m_mantle;
        m_weaponSwitchWasHeld = input.m_switchWeapon;
        m_requestedEquipmentSlotWasHeld = input.m_requestedEquipmentSlot;

        if (!m_player.m_alive)
        {
            m_movement.Reset();
            m_player.m_slideActive = false;
            m_player.m_slideElapsed = 0.0f;
            m_player.m_slideSpeed = 0.0f;
            m_player.m_slideDirection = AZ::Vector3::CreateZero();
            m_player.m_mantleActive = false;
            m_player.m_mantleElapsed = 0.0f;
            m_player.m_mantleDirection = AZ::Vector3::CreateZero();
            m_presentation.m_activeEquipmentProfile = GetActiveEquipmentProfileId();
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
        const bool mantleOwnsTick = m_player.m_mantleRequested;
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
            && !m_player.m_slideActive && !m_player.m_mantleActive && !mantleOwnsTick;
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

        PlayerMovementInput movementInput;
        movementInput.m_direction = planarDirection;
        movementInput.m_sprint = input.m_sprint;
        movementInput.m_grounded = m_player.m_grounded;
        movementInput.m_alive = m_player.m_alive;
        m_movement.Update(deltaTime, movementInput);

        if (newJumpPress && m_player.m_grounded && !m_player.m_slideActive && !m_player.m_mantleActive
            && !mantleOwnsTick)
        {
            m_jumpImpulseThisTick = JumpImpulseSpeed;
            ++m_player.m_jumpEvents;
        }

        m_player.m_yaw = std::fmod(m_player.m_yaw + input.m_lookX * LookSensitivity, PlayerTwoPi);
        if (m_player.m_yaw > AZ::Constants::Pi)
        {
            m_player.m_yaw -= PlayerTwoPi;
        }
        else if (m_player.m_yaw < -AZ::Constants::Pi)
        {
            m_player.m_yaw += PlayerTwoPi;
        }
        m_player.m_pitch = AZStd::clamp(m_player.m_pitch - input.m_lookY * LookSensitivity, -PitchLimit, PitchLimit);

        if (newEquipmentSlotRequest)
        {
            RequestEquipmentSwitch(static_cast<EquipmentSlot>(input.m_requestedEquipmentSlot));
        }
        else if (newWeaponSwitch)
        {
            RequestWeaponSwitch();
        }
        if (input.m_reload)
        {
            StartReload();
        }
        if (input.m_fire)
        {
            TryFire();
        }
        const EquipmentProfileId profileAfterInput = GetActiveEquipmentProfileId();
        m_presentation.m_activeEquipmentProfile = profileAfterInput;
        m_presentation.m_equipmentChanged = profileAfterInput != profileBeforeInput;
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
        WeaponUseResult use;
        if (!m_weapons.TryUse(m_player.m_alive, use))
        {
            return false;
        }

        // Update() owns per-tick presentation clearing. Do not clear an earlier
        // accepted event here when a caller invokes TryFire directly.
        if (use.m_shotFired)
        {
            m_presentation.m_shotFired = true;
        }
        m_presentation.m_equipmentUsed = true;
        m_presentation.m_fireCueRemaining = 0.06f;
        EnemyId hitEnemyId = InvalidEnemyId;
        float hitDistance = use.m_range;
        for (size_t index = 0; index < GetEnemies().GetEnemyCount(); ++index)
        {
            const EnemyState& enemy = GetEnemies().GetInstanceByIndex(index).m_combat.GetState();
            float projectedDistance = 0.0f;
            if (enemy.m_alive
                && RayHitsEnemy(enemy, GetEyePosition(), GetAimDirection(), use.m_range, projectedDistance)
                && projectedDistance < hitDistance)
            {
                hitEnemyId = enemy.m_id;
                hitDistance = projectedDistance;
            }
        }
        // Destructible cover objects resolve against the same shot: whichever
        // is genuinely closer along the ray wins, same as real weapon fire
        // would - not "prefer enemies" or "prefer cover" by fiat. Passing the
        // enemy hit distance (or full weapon range if no enemy was hit) as
        // the search's own maxRange means a returned index is only ever the
        // strictly-closer case.
        float destructibleDistance = 0.0f;
        const size_t destructibleIndex =
            m_destructibles.RayHitsObject(GetEyePosition(), GetAimDirection(), hitDistance, destructibleDistance);

        if (destructibleIndex < DestructibleObjectModel::MaxObjectCount)
        {
            m_destructibles.ApplyDamage(destructibleIndex, use.m_damage);
            m_presentation.m_hit = true;
            m_presentation.m_hitDestructible = true;
            m_presentation.m_hitDestructibleIndex = destructibleIndex;
            m_presentation.m_hitCueRemaining = 0.12f;
        }
        else if (hitEnemyId != InvalidEnemyId && GetEnemies().ApplyDamage(hitEnemyId, use.m_damage))
        {
            m_presentation.m_hit = true;
            m_presentation.m_hitEnemyId = hitEnemyId;
            m_presentation.m_hitCueRemaining = 0.12f;
        }
        else if (m_matchRuleset != nullptr && m_matchRuleset->IsActive())
        {
            // PvP resolution only when no enemy/destructible was already the
            // closer hit above (the same "whichever is genuinely closer
            // wins" rule the enemy-vs-destructible check already applies) -
            // ResolvePvpHit itself is only ever given other TEAMMATE-
            // excluded, alive players by MatchRulesetModel, so no
            // additional self/friendly-fire check is needed here.
            float pvpDistance = 0.0f;
            const AZ::EntityId pvpTarget = m_matchRuleset->ResolvePvpHit(
                m_networkEntityId, GetEyePosition(), GetAimDirection(), hitDistance, pvpDistance);
            if (pvpTarget.IsValid())
            {
                m_lastPvpHitTarget = pvpTarget;
                m_lastPvpHitDamage = use.m_damage;
                m_lastPvpHitRange = use.m_range;
                m_presentation.m_hit = true;
                m_presentation.m_hitCueRemaining = 0.12f;
            }
        }
        return true;
    }

    AZ::EntityId PlayerSliceModel::ConsumeLastPvpHitTarget(float& outDamage, float& outRange)
    {
        const AZ::EntityId target = m_lastPvpHitTarget;
        outDamage = m_lastPvpHitDamage;
        outRange = m_lastPvpHitRange;
        m_lastPvpHitTarget = AZ::EntityId();
        m_lastPvpHitDamage = 0.0f;
        m_lastPvpHitRange = 0.0f;
        return target;
    }

    bool PlayerSliceModel::StartReload()
    {
        return m_weapons.StartReload(m_player.m_alive);
    }

    bool PlayerSliceModel::RequestWeaponSwitch()
    {
        return m_weapons.RequestWeaponSwitch(m_player.m_alive);
    }

    bool PlayerSliceModel::RequestEquipmentSwitch(EquipmentSlot slot)
    {
        return m_weapons.RequestEquipmentSwitch(slot, m_player.m_alive);
    }

    bool PlayerSliceModel::SetLoadoutProfile(EquipmentSlot slot, EquipmentProfileId profileId)
    {
        return m_weapons.SetLoadoutProfile(slot, profileId);
    }

    bool PlayerSliceModel::ApplyDamage(float damage)
    {
        if (!m_player.m_alive || !IsFinite(damage) || damage <= 0.0f || m_invulnerabilityRemaining > 0.0f)
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
        m_weapons.ResetLoadout();
        m_weaponSwitchWasHeld = false;
        m_requestedEquipmentSlotWasHeld = -1;
        m_movement.Reset();
        m_presentation = {};
        m_invulnerabilityRemaining = RespawnInvulnerabilityDuration;
    }

    AZ::Vector3 PlayerSliceModel::GetEyePosition() const
    {
        return m_player.m_position + AZ::Vector3(0.0f, 0.0f, EyeHeight);
    }

    void PlayerSliceModel::SetAimAngles(float yaw, float pitch)
    {
        if (!std::isfinite(yaw) || !std::isfinite(pitch))
        {
            return;
        }
        m_player.m_yaw = yaw;
        m_player.m_pitch = pitch;
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

    AZ::Vector3 PlayerSliceModel::GetMovementVelocity() const
    {
        if (!m_player.m_alive)
        {
            return AZ::Vector3::CreateZero();
        }

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
            velocity = m_movement.GetVelocity();
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

    bool PlayerSliceModel::ApplyAuthoritativeCorrection(const AuthoritativePlayerSnapshot& snapshot)
    {
        if (!snapshot.m_position.IsFinite()
            || !std::isfinite(snapshot.m_yaw)
            || !std::isfinite(snapshot.m_pitch)
            || !std::isfinite(snapshot.m_health)
            || !std::isfinite(snapshot.m_cooldownRemaining)
            || !std::isfinite(snapshot.m_reloadRemaining))
        {
            return false;
        }
        if (!m_weapons.RestoreAuthoritativeReadback(
                snapshot.m_activeEquipmentSlot,
                snapshot.m_activeEquipmentProfile,
                snapshot.m_magazine,
                snapshot.m_reserve,
                snapshot.m_charges,
                snapshot.m_cooldownRemaining,
                snapshot.m_reloadRemaining,
                snapshot.m_reloading))
        {
            return false;
        }

        m_player.m_position = snapshot.m_position;
        m_player.m_grounded = snapshot.m_grounded;
        m_player.m_yaw = snapshot.m_yaw;
        m_player.m_pitch = snapshot.m_pitch;
        m_player.m_health = snapshot.m_health;
        m_player.m_alive = snapshot.m_alive;
        m_player.m_crouchDesired = snapshot.m_crouchDesired;
        m_player.m_slideActive = snapshot.m_slideActive;
        m_player.m_mantleRequested = snapshot.m_mantleRequested;
        m_player.m_mantleActive = snapshot.m_mantleActive;
        m_player.m_deathEvents = snapshot.m_deathEvents;
        m_player.m_respawnEvents = snapshot.m_respawnEvents;
        return true;
    }

    bool PlayerSliceModel::RayHitsEnemy(const EnemyState& target, const AZ::Vector3& origin,
        const AZ::Vector3& direction, float maximumRange, float& projectedDistance) const
    {
        const AZ::Vector3 toTarget = target.m_position - origin;
        const float projected = toTarget.Dot(direction);
        if (projected < 0.0f || projected > maximumRange)
        {
            return false;
        }
        projectedDistance = projected;
        const AZ::Vector3 closest = origin + direction * projected;
        return (closest - target.m_position).GetLengthSq() <= target.m_radius * target.m_radius;
    }

}
