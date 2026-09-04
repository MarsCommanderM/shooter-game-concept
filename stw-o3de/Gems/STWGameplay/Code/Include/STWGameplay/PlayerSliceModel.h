#pragma once

#include <cstddef>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>
#include <STWGameplay/EnemyCollectionModel.h>
#include <STWGameplay/ArenaLayout.h>

namespace STWGameplay
{
    struct PlayerInput
    {
        float m_forward = 0.0f;
        float m_strafe = 0.0f;
        float m_lookX = 0.0f;
        float m_lookY = 0.0f;
        bool m_sprint = false;
        bool m_jump = false;
        bool m_crouch = false;
        bool m_mantle = false;
        bool m_fire = false;
        bool m_reload = false;
        bool m_switchWeapon = false;
        // -1 means no direct slot request. The native adapter uses this only for the
        // proven-unused number-row slot keys; the model validates the request.
        int m_requestedEquipmentSlot = -1;
    };

    struct PlayerState
    {
        AZ::Vector3 m_position = ArenaLayout::PlayerSpawn;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
        float m_maxHealth = 100.0f;
        float m_health = 100.0f;
        bool m_alive = true;
        bool m_grounded = false;
        bool m_crouchDesired = false;
        bool m_slideActive = false;
        bool m_mantleRequested = false;
        bool m_mantleActive = false;
        float m_mantleElapsed = 0.0f;
        AZ::Vector3 m_mantleDirection = AZ::Vector3::CreateZero();
        float m_slideElapsed = 0.0f;
        float m_slideSpeed = 0.0f;
        AZ::Vector3 m_slideDirection = AZ::Vector3::CreateZero();
        int m_damageEvents = 0;
        int m_deathEvents = 0;
        int m_respawnEvents = 0;
        int m_jumpEvents = 0;
        int m_slideEvents = 0;
        int m_mantleEvents = 0;
    };

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

    using TargetState = EnemyState; // compatibility name for existing presentation/tests

    struct PresentationState
    {
        bool m_shotFired = false;
        bool m_hit = false;
        bool m_equipmentUsed = false;
        bool m_equipmentChanged = false;
        EnemyId m_hitEnemyId = InvalidEnemyId;
        EquipmentProfileId m_activeEquipmentProfile = EquipmentProfileId::STW_SMG_01;
        float m_fireCueRemaining = 0.0f;
        float m_hitCueRemaining = 0.0f;
    };

    class PlayerSliceModel
    {
    public:
        static constexpr float WalkSpeed = 4.5f;
        static constexpr float SprintSpeed = 7.5f;
        static constexpr float JumpImpulseSpeed = 5.5f;
        static constexpr float SlideStartSpeed = 8.5f;
        static constexpr float SlideEndSpeed = WalkSpeed;
        static constexpr float SlideDuration = 0.80f;
        static constexpr float SlideInputThreshold = 0.10f;
        static constexpr float MantleDuration = 0.60f;
        static constexpr float MantleSpeed = WalkSpeed;
        static constexpr float EyeHeight = 1.7f;
        static constexpr float PitchLimit = 1.45f;
        static constexpr float LookSensitivity = 0.0025f;
        static constexpr float FireInterval = 0.075f; // STW_SMG_01 base cadence
        static constexpr float ReloadDuration = 1.75f;
        static constexpr float WeaponRange = 60.0f;
        static constexpr float WeaponDamage = 16.0f;
        static constexpr size_t EquipmentSlotCount = 5;
        static constexpr size_t EquipmentProfileCount = 10;
        static constexpr size_t WeaponCount = 2; // legacy two-weapon gate compatibility

        PlayerSliceModel();
        bool Update(float deltaTime, const PlayerInput& input);
        bool TryFire();
        bool StartReload();
        bool RequestWeaponSwitch();
        bool RequestEquipmentSwitch(EquipmentSlot slot);
        bool SetLoadoutProfile(EquipmentSlot slot, EquipmentProfileId profileId);
        bool ApplyDamage(float damage);
        void ResetPlayer();

        const PlayerState& GetPlayer() const { return m_player; }
        const WeaponState& GetWeapon() const { return GetEquipment(m_activeEquipmentSlot); }
        const WeaponState& GetWeapon(WeaponId weaponId) const
        {
            return m_equipment[static_cast<size_t>(weaponId) < EquipmentProfileCount
                    ? static_cast<size_t>(weaponId) : 0];
        }
        const EquipmentState& GetEquipment(EquipmentSlot slot) const;
        const EquipmentState& GetEquipment(EquipmentProfileId profileId) const
        {
            return GetWeapon(profileId);
        }
        WeaponId GetActiveWeaponId() const { return GetActiveEquipmentProfileId(); }
        EquipmentSlot GetActiveEquipmentSlot() const { return m_activeEquipmentSlot; }
        EquipmentProfileId GetActiveEquipmentProfileId() const;
        const EquipmentProfile& GetActiveEquipmentProfile() const;
        static const EquipmentProfile& GetEquipmentProfile(EquipmentProfileId profileId);
        static const WeaponProfile& GetWeaponProfile(WeaponId weaponId);
        static bool IsValidEquipmentSlot(EquipmentSlot slot);
        static bool IsSlotCompatible(EquipmentSlot slot, EquipmentProfileId profileId);
        EquipmentProfileId GetLoadoutProfile(EquipmentSlot slot) const;
        const TargetState& GetTarget() const { return GetEnemy().GetState(); }
        const EnemyCombatModel& GetEnemy() const { return *m_enemies.GetEnemy(PrimaryEnemyId); }
        EnemyCombatModel& GetEnemy() { return *m_enemies.GetEnemy(PrimaryEnemyId); }
        const EnemyCollectionModel& GetEnemies() const { return m_enemies; }
        EnemyCollectionModel& GetEnemies() { return m_enemies; }
        const PresentationState& GetPresentation() const { return m_presentation; }
        AZ::Vector3 GetEyePosition() const;
        AZ::Vector3 GetAimDirection() const;
        AZ::Vector3 GetDesiredVelocity(const PlayerInput& input) const;
        bool IsMantleRequested() const { return m_player.m_mantleRequested; }
        void BeginMantle(const AZ::Vector3& direction);

        void SetTargetPosition(const AZ::Vector3& position) { m_enemies.SynchronizePhysicalPosition(PrimaryEnemyId, position); }
        void SetPlayerPosition(const AZ::Vector3& position);
        void SynchronizePhysicalState(const AZ::Vector3& position, bool grounded);

    private:
        bool RayHitsEnemy(const EnemyState& target, const AZ::Vector3& origin, const AZ::Vector3& direction,
            float& projectedDistance) const;
        void FinishReload();
        void ResetWeapons();
        size_t GetActiveEquipmentIndex() const;

        PlayerState m_player;
        AZStd::array<WeaponState, EquipmentProfileCount> m_equipment;
        AZStd::array<EquipmentProfileId, EquipmentSlotCount> m_loadoutProfiles;
        EquipmentSlot m_activeEquipmentSlot = EquipmentSlot::Primary;
        EnemyCollectionModel m_enemies;
        PresentationState m_presentation;
        bool m_jumpWasHeld = false;
        bool m_crouchWasHeld = false;
        bool m_mantleWasHeld = false;
        bool m_weaponSwitchWasHeld = false;
        int m_requestedEquipmentSlotWasHeld = -1;
        float m_jumpImpulseThisTick = 0.0f;
    };
}
