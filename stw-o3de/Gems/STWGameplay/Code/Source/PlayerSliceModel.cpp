#include <STWGameplay/PlayerSliceModel.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <cmath>

namespace STWGameplay
{
    namespace
    {
        const AZStd::array<EquipmentProfile, PlayerSliceModel::EquipmentProfileCount> s_equipmentProfiles = {
            EquipmentProfile{ EquipmentProfileId::STW_SMG_01, EquipmentCategory::Smg, EquipmentSlot::Primary,
                30, 150, 0, 0, 0.075f, 1.75f, 60.0f, 16.0f, "STW_SMG_01",
                "assets/weapons/stw_smg_01/stw_smg_01.obj.azmodel",
                "assets/weapons/stw_smg_01/stw_smg_01.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_RIFLE_02, EquipmentCategory::Rifle, EquipmentSlot::Secondary,
                12, 48, 0, 0, 0.180f, 1.75f, 75.0f, 24.0f, "STW_RIFLE_02",
                "assets/weapons/stw_rifle_02/stw_rifle_02.obj.azmodel",
                "assets/weapons/stw_rifle_02/stw_rifle_02.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_RIFLE_03, EquipmentCategory::Rifle, EquipmentSlot::Primary,
                20, 100, 0, 0, 0.110f, 1.90f, 80.0f, 22.0f, "STW_RIFLE_03",
                "assets/weapons/stw_rifle_03/stw_rifle_03.obj.azmodel",
                "assets/weapons/stw_rifle_03/stw_rifle_03.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_LMG_04, EquipmentCategory::Lmg, EquipmentSlot::Primary,
                60, 180, 0, 0, 0.100f, 2.80f, 65.0f, 14.0f, "STW_LMG_04",
                "assets/weapons/stw_lmg_04/stw_lmg_04.obj.azmodel",
                "assets/weapons/stw_lmg_04/stw_lmg_04.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_SIDEARM_01, EquipmentCategory::Sidearm, EquipmentSlot::Secondary,
                15, 90, 0, 0, 0.140f, 1.35f, 55.0f, 20.0f, "STW_SIDEARM_01",
                "assets/weapons/stw_sidearm_01/stw_sidearm_01.obj.azmodel",
                "assets/weapons/stw_sidearm_01/stw_sidearm_01.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_LAUNCHER_01, EquipmentCategory::Launcher, EquipmentSlot::Primary,
                1, 4, 0, 0, 0.900f, 2.40f, 90.0f, 80.0f, "STW_LAUNCHER_01",
                "assets/weapons/stw_launcher_01/stw_launcher_01.obj.azmodel",
                "assets/weapons/stw_launcher_01/stw_launcher_01.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_TACTICAL_FLASH_01, EquipmentCategory::Flash, EquipmentSlot::Tactical,
                0, 0, 2, 2, 0.500f, 0.0f, 0.0f, 0.0f, "STW_TACTICAL_FLASH_01",
                "assets/weapons/stw_tactical_flash_01/stw_tactical_flash_01.obj.azmodel",
                "assets/weapons/stw_tactical_flash_01/stw_tactical_flash_01.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_TACTICAL_SMOKE_01, EquipmentCategory::Smoke, EquipmentSlot::Tactical,
                0, 0, 2, 2, 0.500f, 0.0f, 0.0f, 0.0f, "STW_TACTICAL_SMOKE_01",
                "assets/weapons/stw_tactical_smoke_01/stw_tactical_smoke_01.obj.azmodel",
                "assets/weapons/stw_tactical_smoke_01/stw_tactical_smoke_01.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_LETHAL_FRAG_01, EquipmentCategory::Frag, EquipmentSlot::Lethal,
                0, 0, 2, 2, 0.750f, 0.0f, 0.0f, 0.0f, "STW_LETHAL_FRAG_01",
                "assets/weapons/stw_lethal_frag_01/stw_lethal_frag_01.obj.azmodel",
                "assets/weapons/stw_lethal_frag_01/stw_lethal_frag_01.azmaterial" },
            EquipmentProfile{ EquipmentProfileId::STW_MELEE_01, EquipmentCategory::Melee, EquipmentSlot::Melee,
                0, 0, 0, 0, 0.450f, 0.0f, 2.5f, 50.0f, "STW_MELEE_01",
                "assets/weapons/stw_melee_01/stw_melee_01.obj.azmodel",
                "assets/weapons/stw_melee_01/stw_melee_01.azmaterial" }
        };

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

    PlayerSliceModel::PlayerSliceModel()
    {
        ResetWeapons();
    }

    const EquipmentProfile& PlayerSliceModel::GetEquipmentProfile(EquipmentProfileId profileId)
    {
        const size_t index = static_cast<size_t>(profileId);
        return s_equipmentProfiles[index < EquipmentProfileCount ? index : 0];
    }

    const WeaponProfile& PlayerSliceModel::GetWeaponProfile(WeaponId weaponId)
    {
        return GetEquipmentProfile(weaponId);
    }

    bool PlayerSliceModel::IsValidEquipmentSlot(EquipmentSlot slot)
    {
        return static_cast<size_t>(slot) < EquipmentSlotCount;
    }

    bool PlayerSliceModel::IsSlotCompatible(EquipmentSlot slot, EquipmentProfileId profileId)
    {
        const size_t profileIndex = static_cast<size_t>(profileId);
        return IsValidEquipmentSlot(slot) && profileIndex < EquipmentProfileCount
            && GetEquipmentProfile(profileId).m_allowedSlot == slot;
    }

    size_t PlayerSliceModel::GetActiveEquipmentIndex() const
    {
        return static_cast<size_t>(m_loadoutProfiles[static_cast<size_t>(m_activeEquipmentSlot)]);
    }

    const EquipmentState& PlayerSliceModel::GetEquipment(EquipmentSlot slot) const
    {
        if (!IsValidEquipmentSlot(slot))
        {
            return m_equipment[GetActiveEquipmentIndex()];
        }
        const size_t profileIndex = static_cast<size_t>(m_loadoutProfiles[static_cast<size_t>(slot)]);
        return m_equipment[profileIndex < EquipmentProfileCount ? profileIndex : 0];
    }

    EquipmentProfileId PlayerSliceModel::GetLoadoutProfile(EquipmentSlot slot) const
    {
        return IsValidEquipmentSlot(slot)
            ? m_loadoutProfiles[static_cast<size_t>(slot)]
            : m_loadoutProfiles[static_cast<size_t>(EquipmentSlot::Primary)];
    }

    EquipmentProfileId PlayerSliceModel::GetActiveEquipmentProfileId() const
    {
        return GetLoadoutProfile(m_activeEquipmentSlot);
    }

    const EquipmentProfile& PlayerSliceModel::GetActiveEquipmentProfile() const
    {
        return GetEquipmentProfile(GetActiveEquipmentProfileId());
    }

    bool PlayerSliceModel::Update(float deltaTime, const PlayerInput& input)
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
        m_presentation.m_equipmentUsed = false;
        m_presentation.m_equipmentChanged = false;
        m_presentation.m_activeEquipmentProfile = profileBeforeInput;
        m_jumpImpulseThisTick = 0.0f;
        m_player.m_mantleRequested = false;
        m_presentation.m_fireCueRemaining = AZStd::max(0.0f, m_presentation.m_fireCueRemaining - deltaTime);
        m_presentation.m_hitCueRemaining = AZStd::max(0.0f, m_presentation.m_hitCueRemaining - deltaTime);
        WeaponState& activeWeapon = m_equipment[GetActiveEquipmentIndex()];
        activeWeapon.m_cooldownRemaining = AZStd::max(0.0f, activeWeapon.m_cooldownRemaining - deltaTime);
        m_enemies.Update(deltaTime, m_player.m_position, m_player.m_alive);
        if (m_player.m_alive)
        {
            for (size_t index = 0; index < m_enemies.GetEnemyCount(); ++index)
            {
                EnemyCombatModel& enemy = m_enemies.GetInstanceByIndex(index).m_combat;
                if (enemy.TryAttackPlayer())
                {
                    ApplyDamage(enemy.GetProfile().m_attackDamage);
                }
            }
        }

        if (activeWeapon.m_reloading)
        {
            activeWeapon.m_reloadRemaining -= deltaTime;
            if (activeWeapon.m_reloadRemaining <= 0.0f)
            {
                FinishReload();
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

        if (newJumpPress && m_player.m_grounded && !m_player.m_slideActive && !m_player.m_mantleActive
            && !mantleOwnsTick)
        {
            m_jumpImpulseThisTick = JumpImpulseSpeed;
            ++m_player.m_jumpEvents;
        }

        m_player.m_yaw += input.m_lookX * LookSensitivity;
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
        WeaponState& equipment = m_equipment[GetActiveEquipmentIndex()];
        const WeaponProfile& profile = GetActiveEquipmentProfile();
        const bool hasMagazineResource = profile.m_magazineCapacity > 0;
        const bool hasChargeResource = profile.m_chargeCapacity > 0;
        if (!m_player.m_alive || equipment.m_reloading || equipment.m_cooldownRemaining > 0.0f
            || (hasMagazineResource && equipment.m_magazine <= 0)
            || (hasChargeResource && equipment.m_charges <= 0))
        {
            return false;
        }
        if (hasMagazineResource)
        {
            --equipment.m_magazine;
            m_presentation.m_shotFired = true;
        }
        if (hasChargeResource)
        {
            --equipment.m_charges;
        }
        equipment.m_cooldownRemaining = profile.m_fireInterval;
        m_presentation.m_equipmentUsed = true;
        m_presentation.m_fireCueRemaining = 0.06f;
        EnemyId hitEnemyId = InvalidEnemyId;
        float hitDistance = profile.m_range;
        for (size_t index = 0; index < m_enemies.GetEnemyCount(); ++index)
        {
            const EnemyState& enemy = m_enemies.GetInstanceByIndex(index).m_combat.GetState();
            float projectedDistance = 0.0f;
            if (enemy.m_alive && RayHitsEnemy(enemy, GetEyePosition(), GetAimDirection(), projectedDistance)
                && projectedDistance < hitDistance)
            {
                hitEnemyId = enemy.m_id;
                hitDistance = projectedDistance;
            }
        }
        if (hitEnemyId != InvalidEnemyId && m_enemies.ApplyDamage(hitEnemyId, profile.m_damage))
        {
            m_presentation.m_hit = true;
            m_presentation.m_hitEnemyId = hitEnemyId;
            m_presentation.m_hitCueRemaining = 0.12f;
        }
        return true;
    }

    bool PlayerSliceModel::StartReload()
    {
        WeaponState& equipment = m_equipment[GetActiveEquipmentIndex()];
        const WeaponProfile& profile = GetActiveEquipmentProfile();
        if (!m_player.m_alive || profile.m_magazineCapacity <= 0 || equipment.m_reloading
            || equipment.m_magazine >= profile.m_magazineCapacity || equipment.m_reserve <= 0)
        {
            return false;
        }
        equipment.m_reloading = true;
        equipment.m_reloadRemaining = profile.m_reloadDuration;
        return true;
    }

    bool PlayerSliceModel::RequestWeaponSwitch()
    {
        const EquipmentSlot targetSlot = m_activeEquipmentSlot == EquipmentSlot::Primary
            ? EquipmentSlot::Secondary : EquipmentSlot::Primary;
        return RequestEquipmentSwitch(targetSlot);
    }

    bool PlayerSliceModel::RequestEquipmentSwitch(EquipmentSlot slot)
    {
        if (!m_player.m_alive || !IsValidEquipmentSlot(slot) || slot == m_activeEquipmentSlot)
        {
            return false;
        }
        const WeaponState& activeEquipment = m_equipment[GetActiveEquipmentIndex()];
        if (activeEquipment.m_reloading)
        {
            return false;
        }
        m_activeEquipmentSlot = slot;
        return true;
    }

    bool PlayerSliceModel::SetLoadoutProfile(EquipmentSlot slot, EquipmentProfileId profileId)
    {
        const size_t profileIndex = static_cast<size_t>(profileId);
        if (!IsSlotCompatible(slot, profileId))
        {
            return false;
        }
        for (size_t index = 0; index < EquipmentSlotCount; ++index)
        {
            if (static_cast<EquipmentSlot>(index) != slot && m_loadoutProfiles[index] == profileId)
            {
                return false;
            }
        }
        m_loadoutProfiles[static_cast<size_t>(slot)] = profileId;
        m_equipment[profileIndex].m_profileId = profileId;
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
        ResetWeapons();
        m_activeEquipmentSlot = EquipmentSlot::Primary;
        m_weaponSwitchWasHeld = false;
        m_requestedEquipmentSlotWasHeld = -1;
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

    bool PlayerSliceModel::RayHitsEnemy(const EnemyState& target, const AZ::Vector3& origin,
        const AZ::Vector3& direction, float& projectedDistance) const
    {
        const AZ::Vector3 toTarget = target.m_position - origin;
        const float projected = toTarget.Dot(direction);
        if (projected < 0.0f || projected > GetActiveEquipmentProfile().m_range)
        {
            return false;
        }
        projectedDistance = projected;
        const AZ::Vector3 closest = origin + direction * projected;
        return (closest - target.m_position).GetLengthSq() <= target.m_radius * target.m_radius;
    }

    void PlayerSliceModel::FinishReload()
    {
        WeaponState& weapon = m_equipment[GetActiveEquipmentIndex()];
        const WeaponProfile& profile = GetActiveEquipmentProfile();
        const int needed = profile.m_magazineCapacity - weapon.m_magazine;
        const int transferred = AZStd::min(needed, weapon.m_reserve);
        weapon.m_magazine += transferred;
        weapon.m_reserve -= transferred;
        weapon.m_reloading = false;
        weapon.m_reloadRemaining = 0.0f;
    }

    void PlayerSliceModel::ResetWeapons()
    {
        m_equipment = {};
        m_loadoutProfiles = {
            EquipmentProfileId::STW_SMG_01,
            EquipmentProfileId::STW_RIFLE_02,
            EquipmentProfileId::STW_TACTICAL_FLASH_01,
            EquipmentProfileId::STW_LETHAL_FRAG_01,
            EquipmentProfileId::STW_MELEE_01
        };
        for (size_t index = 0; index < EquipmentProfileCount; ++index)
        {
            const EquipmentProfileId profileId = static_cast<EquipmentProfileId>(index);
            const EquipmentProfile& profile = GetEquipmentProfile(profileId);
            m_equipment[index].m_profileId = profileId;
            m_equipment[index].m_magazine = profile.m_magazineCapacity;
            m_equipment[index].m_reserve = profile.m_initialReserve;
            m_equipment[index].m_charges = profile.m_initialCharges;
        }
        m_activeEquipmentSlot = EquipmentSlot::Primary;
    }
}
