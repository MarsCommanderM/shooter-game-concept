#include <STWGameplay/WeaponModel.h>

#include <AzCore/std/algorithm.h>

#include <cmath>

namespace STWGameplay
{
    namespace
    {
        const AZStd::array<EquipmentProfile, WeaponModel::EquipmentProfileCount> EquipmentProfiles =
        {
            EquipmentProfile{
                EquipmentProfileId::STW_SMG_01,
                EquipmentCategory::Smg,
                EquipmentSlot::Primary,
                30, 150, 0, 0,
                0.075f, 1.75f, 60.0f, 16.0f,
                "STW_SMG_01",
                "assets/weapons/stw_smg_01/stw_smg_01.obj.azmodel",
                "assets/weapons/stw_smg_01/stw_smg_01.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_RIFLE_02,
                EquipmentCategory::Rifle,
                EquipmentSlot::Secondary,
                12, 48, 0, 0,
                0.180f, 1.75f, 75.0f, 24.0f,
                "STW_RIFLE_02",
                "assets/weapons/stw_rifle_02/stw_rifle_02.obj.azmodel",
                "assets/weapons/stw_rifle_02/stw_rifle_02.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_RIFLE_03,
                EquipmentCategory::Rifle,
                EquipmentSlot::Primary,
                20, 100, 0, 0,
                0.110f, 1.90f, 80.0f, 22.0f,
                "STW_RIFLE_03",
                "assets/weapons/stw_rifle_03/stw_rifle_03.obj.azmodel",
                "assets/weapons/stw_rifle_03/stw_rifle_03.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_LMG_04,
                EquipmentCategory::Lmg,
                EquipmentSlot::Primary,
                60, 180, 0, 0,
                0.100f, 2.80f, 65.0f, 14.0f,
                "STW_LMG_04",
                "assets/weapons/stw_lmg_04/stw_lmg_04.obj.azmodel",
                "assets/weapons/stw_lmg_04/stw_lmg_04.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_SIDEARM_01,
                EquipmentCategory::Sidearm,
                EquipmentSlot::Secondary,
                15, 90, 0, 0,
                0.140f, 1.35f, 55.0f, 20.0f,
                "STW_SIDEARM_01",
                "assets/weapons/stw_sidearm_01/stw_sidearm_01.obj.azmodel",
                "assets/weapons/stw_sidearm_01/stw_sidearm_01.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_LAUNCHER_01,
                EquipmentCategory::Launcher,
                EquipmentSlot::Primary,
                1, 4, 0, 0,
                0.900f, 2.40f, 90.0f, 80.0f,
                "STW_LAUNCHER_01",
                "assets/weapons/stw_launcher_01/stw_launcher_01.obj.azmodel",
                "assets/weapons/stw_launcher_01/stw_launcher_01.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_TACTICAL_FLASH_01,
                EquipmentCategory::Flash,
                EquipmentSlot::Tactical,
                0, 0, 2, 2,
                0.500f, 0.0f, 0.0f, 0.0f,
                "STW_TACTICAL_FLASH_01",
                "assets/weapons/stw_tactical_flash_01/stw_tactical_flash_01.obj.azmodel",
                "assets/weapons/stw_tactical_flash_01/stw_tactical_flash_01.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_TACTICAL_SMOKE_01,
                EquipmentCategory::Smoke,
                EquipmentSlot::Tactical,
                0, 0, 2, 2,
                0.500f, 0.0f, 0.0f, 0.0f,
                "STW_TACTICAL_SMOKE_01",
                "assets/weapons/stw_tactical_smoke_01/stw_tactical_smoke_01.obj.azmodel",
                "assets/weapons/stw_tactical_smoke_01/stw_tactical_smoke_01.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_LETHAL_FRAG_01,
                EquipmentCategory::Frag,
                EquipmentSlot::Lethal,
                0, 0, 2, 2,
                0.750f, 0.0f, 0.0f, 0.0f,
                "STW_LETHAL_FRAG_01",
                "assets/weapons/stw_lethal_frag_01/stw_lethal_frag_01.obj.azmodel",
                "assets/weapons/stw_lethal_frag_01/stw_lethal_frag_01.azmaterial"
            },
            EquipmentProfile{
                EquipmentProfileId::STW_MELEE_01,
                EquipmentCategory::Melee,
                EquipmentSlot::Melee,
                0, 0, 0, 0,
                0.450f, 0.0f, 2.5f, 50.0f,
                "STW_MELEE_01",
                "assets/weapons/stw_melee_01/stw_melee_01.obj.azmodel",
                "assets/weapons/stw_melee_01/stw_melee_01.azmaterial"
            }
        };
    }

    WeaponModel::WeaponModel()
    {
        ResetLoadout();
    }

    bool WeaponModel::Update(float deltaTime)
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            return false;
        }

        // Preserve existing STW behavior exactly: only the active equipment's
        // timers advance, so switching away preserves its cooldown/reload state.
        EquipmentState& active = m_equipment[GetActiveEquipmentIndex()];
        active.m_cooldownRemaining = AZStd::max(0.0f, active.m_cooldownRemaining - deltaTime);
        if (active.m_reloading)
        {
            active.m_reloadRemaining -= deltaTime;
            if (active.m_reloadRemaining <= 0.0f)
            {
                FinishReload();
            }
        }
        return true;
    }

    bool WeaponModel::TryUse(bool playerAlive, WeaponUseResult& result)
    {
        result = {};
        EquipmentState& equipment = m_equipment[GetActiveEquipmentIndex()];
        const EquipmentProfile& profile = GetActiveEquipmentProfile();
        const bool hasMagazineResource = profile.m_magazineCapacity > 0;
        const bool hasChargeResource = profile.m_chargeCapacity > 0;
        if (!playerAlive || equipment.m_reloading || equipment.m_cooldownRemaining > 0.0f
            || (hasMagazineResource && equipment.m_magazine <= 0)
            || (hasChargeResource && equipment.m_charges <= 0))
        {
            return false;
        }

        if (hasMagazineResource)
        {
            --equipment.m_magazine;
        }
        if (hasChargeResource)
        {
            --equipment.m_charges;
        }
        equipment.m_cooldownRemaining = profile.m_fireInterval;

        result.m_accepted = true;
        result.m_shotFired = hasMagazineResource;
        result.m_equipmentUsed = true;
        result.m_slot = m_activeEquipmentSlot;
        result.m_profileId = profile.m_profileId;
        result.m_category = profile.m_category;
        result.m_range = profile.m_range;
        result.m_damage = profile.m_damage;
        result.m_eventId = m_nextUseEventId++;
        m_lastAcceptedUseEventId = result.m_eventId;

        // Zero is reserved as "no event". Keep use IDs monotonic across reset.
        if (m_nextUseEventId == 0)
        {
            m_nextUseEventId = 1;
        }
        return true;
    }

    bool WeaponModel::StartReload(bool playerAlive)
    {
        EquipmentState& equipment = m_equipment[GetActiveEquipmentIndex()];
        const EquipmentProfile& profile = GetActiveEquipmentProfile();
        if (!playerAlive || profile.m_magazineCapacity <= 0 || equipment.m_reloading
            || equipment.m_magazine >= profile.m_magazineCapacity || equipment.m_reserve <= 0)
        {
            return false;
        }

        equipment.m_reloading = true;
        equipment.m_reloadRemaining = profile.m_reloadDuration;
        return true;
    }

    bool WeaponModel::RequestWeaponSwitch(bool playerAlive)
    {
        const EquipmentSlot targetSlot = m_activeEquipmentSlot == EquipmentSlot::Primary
            ? EquipmentSlot::Secondary : EquipmentSlot::Primary;
        return RequestEquipmentSwitch(targetSlot, playerAlive);
    }

    bool WeaponModel::RequestEquipmentSwitch(EquipmentSlot slot, bool playerAlive)
    {
        if (!playerAlive || !IsValidEquipmentSlot(slot) || slot == m_activeEquipmentSlot)
        {
            return false;
        }

        const EquipmentState& activeEquipment = m_equipment[GetActiveEquipmentIndex()];
        if (activeEquipment.m_reloading)
        {
            return false;
        }
        m_activeEquipmentSlot = slot;
        return true;
    }

    bool WeaponModel::SetLoadoutProfile(EquipmentSlot slot, EquipmentProfileId profileId)
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

    void WeaponModel::ResetLoadout()
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
            EquipmentState& state = m_equipment[index];
            state.m_profileId = profileId;
            state.m_magazine = profile.m_magazineCapacity;
            state.m_reserve = profile.m_initialReserve;
            state.m_charges = profile.m_initialCharges;
        }

        m_activeEquipmentSlot = EquipmentSlot::Primary;
        // Event IDs intentionally do not rewind here. A respawn/reset must not
        // cause a later accepted event to reuse an old identifier.
    }

    const EquipmentState& WeaponModel::GetEquipment(EquipmentSlot slot) const
    {
        if (!IsValidEquipmentSlot(slot))
        {
            return m_equipment[GetActiveEquipmentIndex()];
        }

        const size_t profileIndex = static_cast<size_t>(m_loadoutProfiles[static_cast<size_t>(slot)]);
        return m_equipment[profileIndex < EquipmentProfileCount ? profileIndex : 0];
    }

    const EquipmentState& WeaponModel::GetEquipment(EquipmentProfileId profileId) const
    {
        const size_t index = static_cast<size_t>(profileId);
        return m_equipment[index < EquipmentProfileCount ? index : 0];
    }

    EquipmentProfileId WeaponModel::GetLoadoutProfile(EquipmentSlot slot) const
    {
        return IsValidEquipmentSlot(slot)
            ? m_loadoutProfiles[static_cast<size_t>(slot)]
            : m_loadoutProfiles[static_cast<size_t>(EquipmentSlot::Primary)];
    }

    EquipmentProfileId WeaponModel::GetActiveEquipmentProfileId() const
    {
        return GetLoadoutProfile(m_activeEquipmentSlot);
    }

    const EquipmentProfile& WeaponModel::GetActiveEquipmentProfile() const
    {
        return GetEquipmentProfile(GetActiveEquipmentProfileId());
    }

    const EquipmentProfile& WeaponModel::GetEquipmentProfile(EquipmentProfileId profileId)
    {
        const size_t index = static_cast<size_t>(profileId);
        return EquipmentProfiles[index < EquipmentProfileCount ? index : 0];
    }

    const WeaponProfile& WeaponModel::GetWeaponProfile(WeaponId weaponId)
    {
        return GetEquipmentProfile(weaponId);
    }

    bool WeaponModel::IsValidEquipmentSlot(EquipmentSlot slot)
    {
        return static_cast<size_t>(slot) < EquipmentSlotCount;
    }

    bool WeaponModel::IsSlotCompatible(EquipmentSlot slot, EquipmentProfileId profileId)
    {
        const size_t profileIndex = static_cast<size_t>(profileId);
        return IsValidEquipmentSlot(slot) && profileIndex < EquipmentProfileCount
            && GetEquipmentProfile(profileId).m_allowedSlot == slot;
    }

    size_t WeaponModel::GetActiveEquipmentIndex() const
    {
        return static_cast<size_t>(m_loadoutProfiles[static_cast<size_t>(m_activeEquipmentSlot)]);
    }

    void WeaponModel::FinishReload()
    {
        EquipmentState& weapon = m_equipment[GetActiveEquipmentIndex()];
        const EquipmentProfile& profile = GetActiveEquipmentProfile();
        const int needed = profile.m_magazineCapacity - weapon.m_magazine;
        const int transferred = AZStd::min(needed, weapon.m_reserve);
        weapon.m_magazine += transferred;
        weapon.m_reserve -= transferred;
        weapon.m_reloading = false;
        weapon.m_reloadRemaining = 0.0f;
    }
}
