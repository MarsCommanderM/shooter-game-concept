#pragma once

#include <cstddef>

#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>

namespace STWGameplay
{
    enum class EquipmentSlot : AZ::u8
    {
        Primary = 0,
        Secondary,
        Tactical,
        Lethal,
        Melee
    };

    enum class EquipmentCategory : AZ::u8
    {
        Rifle = 0,
        Smg,
        Lmg,
        Marksman,
        Sidearm,
        Launcher,
        Flash,
        Smoke,
        Frag,
        Melee
    };

    enum class EquipmentProfileId : AZ::u8
    {
        STW_SMG_01 = 0,
        STW_RIFLE_02 = 1, // Block 19B compatibility profile
        STW_RIFLE_03,
        STW_LMG_04,
        STW_SIDEARM_01,
        STW_LAUNCHER_01,
        STW_TACTICAL_FLASH_01,
        STW_TACTICAL_SMOKE_01,
        STW_LETHAL_FRAG_01,
        STW_MELEE_01
    };

    struct EquipmentState
    {
        EquipmentProfileId m_profileId = EquipmentProfileId::STW_SMG_01;
        int m_magazine = 0;
        int m_reserve = 0;
        int m_charges = 0;
        float m_cooldownRemaining = 0.0f;
        float m_reloadRemaining = 0.0f;
        bool m_reloading = false;
    };

    struct EquipmentProfile
    {
        EquipmentProfileId m_profileId = EquipmentProfileId::STW_SMG_01;
        EquipmentCategory m_category = EquipmentCategory::Smg;
        EquipmentSlot m_allowedSlot = EquipmentSlot::Primary;
        int m_magazineCapacity = 0;
        int m_initialReserve = 0;
        int m_chargeCapacity = 0;
        int m_initialCharges = 0;
        float m_fireInterval = 0.075f;
        float m_reloadDuration = 1.75f;
        float m_range = 60.0f;
        float m_damage = 16.0f;
        const char* m_displayName = "STW_SMG_01";
        const char* m_presentationAssetPath = nullptr;
        const char* m_presentationMaterialPath = nullptr;
    };

    // Compatibility names retained for the verified Block 19B public surface.
    using WeaponId = EquipmentProfileId;
    using WeaponState = EquipmentState;
    using WeaponProfile = EquipmentProfile;
    using WeaponEventId = AZ::u64;

    struct WeaponUseResult
    {
        bool m_accepted = false;
        // Magazine-backed equipment emits the authoritative shot-fired event.
        bool m_shotFired = false;
        // Any successfully activated equipment emits equipment-used, including
        // tactical, lethal, and melee equipment.
        bool m_equipmentUsed = false;
        EquipmentSlot m_slot = EquipmentSlot::Primary;
        EquipmentProfileId m_profileId = EquipmentProfileId::STW_SMG_01;
        EquipmentCategory m_category = EquipmentCategory::Smg;
        float m_range = 0.0f;
        float m_damage = 0.0f;
        WeaponEventId m_eventId = 0;
    };

    //! Authoritative deterministic equipment/weapon state.
    //! It owns loadout, resources, cadence, reload state, active slot, and
    //! accepted-use event identity. It has no player, enemy, physics, or
    //! presentation dependency.
    class WeaponModel final
    {
    public:
        static constexpr size_t EquipmentSlotCount = 5;
        static constexpr size_t EquipmentProfileCount = 10;
        static constexpr size_t WeaponCount = 2;

        WeaponModel();

        bool Update(float deltaTime);
        bool TryUse(bool playerAlive, WeaponUseResult& result);
        bool StartReload(bool playerAlive);
        bool RequestWeaponSwitch(bool playerAlive);
        bool RequestEquipmentSwitch(EquipmentSlot slot, bool playerAlive);
        bool SetLoadoutProfile(EquipmentSlot slot, EquipmentProfileId profileId);
        void ResetLoadout();

        const EquipmentState& GetWeapon() const { return GetEquipment(m_activeEquipmentSlot); }
        const EquipmentState& GetWeapon(WeaponId weaponId) const { return GetEquipment(weaponId); }
        const EquipmentState& GetEquipment(EquipmentSlot slot) const;
        const EquipmentState& GetEquipment(EquipmentProfileId profileId) const;

        EquipmentSlot GetActiveEquipmentSlot() const { return m_activeEquipmentSlot; }
        EquipmentProfileId GetActiveEquipmentProfileId() const;
        const EquipmentProfile& GetActiveEquipmentProfile() const;
        EquipmentProfileId GetLoadoutProfile(EquipmentSlot slot) const;
        WeaponEventId GetLastAcceptedUseEventId() const { return m_lastAcceptedUseEventId; }

        static const EquipmentProfile& GetEquipmentProfile(EquipmentProfileId profileId);
        static const WeaponProfile& GetWeaponProfile(WeaponId weaponId);
        static bool IsValidEquipmentSlot(EquipmentSlot slot);
        static bool IsSlotCompatible(EquipmentSlot slot, EquipmentProfileId profileId);

    private:
        size_t GetActiveEquipmentIndex() const;
        void FinishReload();

        AZStd::array<EquipmentState, EquipmentProfileCount> m_equipment{};
        AZStd::array<EquipmentProfileId, EquipmentSlotCount> m_loadoutProfiles{};
        EquipmentSlot m_activeEquipmentSlot = EquipmentSlot::Primary;
        WeaponEventId m_nextUseEventId = 1;
        WeaponEventId m_lastAcceptedUseEventId = 0;
    };
}
