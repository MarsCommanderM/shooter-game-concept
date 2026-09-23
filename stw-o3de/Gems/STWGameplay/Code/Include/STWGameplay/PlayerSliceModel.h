#pragma once

#include <cstddef>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/optional.h>
#include <STWGameplay/ArenaLayout.h>
#include <STWGameplay/EnemyCollectionModel.h>
#include <STWGameplay/PlayerMovementModel.h>
#include <STWGameplay/PlayerCommand.h>
#include <STWGameplay/WeaponModel.h>

namespace STWGameplay
{
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
        // Grace window after any respawn (checkpoint or interactive) during which the player
        // cannot take damage. Without this, respawning back into an already-engaged enemy's
        // attack range re-kills the player before they get a single frame of real control -
        // proven live: 264 consecutive death/respawn cycles observed in one interactive session,
        // ~3.5s apart, before this fix.
        static constexpr float RespawnInvulnerabilityDuration = 1.5f;
        static constexpr float WeaponRange = 60.0f;
        static constexpr float WeaponDamage = 16.0f;
        static constexpr size_t EquipmentSlotCount = WeaponModel::EquipmentSlotCount;
        static constexpr size_t EquipmentProfileCount = WeaponModel::EquipmentProfileCount;
        static constexpr size_t WeaponCount = WeaponModel::WeaponCount; // legacy two-weapon gate compatibility

        PlayerSliceModel();
        explicit PlayerSliceModel(EnemyCollectionModel& enemyCollection);
        bool Update(float deltaTime, const PlayerInput& input);
        //! Updates one network player's state while leaving the shared enemy simulation to the
        //! composition root's single world update.
        bool UpdateNetworkPlayer(float deltaTime, const PlayerInput& input);
        bool TryFire();
        bool StartReload();
        bool RequestWeaponSwitch();
        bool RequestEquipmentSwitch(EquipmentSlot slot);
        bool SetLoadoutProfile(EquipmentSlot slot, EquipmentProfileId profileId);
        bool ApplyDamage(float damage);
        void ResetPlayer();

        const PlayerState& GetPlayer() const { return m_player; }
        const WeaponState& GetWeapon() const { return m_weapons.GetWeapon(); }
        const WeaponState& GetWeapon(WeaponId weaponId) const { return m_weapons.GetWeapon(weaponId); }
        const EquipmentState& GetEquipment(EquipmentSlot slot) const { return m_weapons.GetEquipment(slot); }
        const EquipmentState& GetEquipment(EquipmentProfileId profileId) const
        {
            return m_weapons.GetEquipment(profileId);
        }
        WeaponId GetActiveWeaponId() const { return m_weapons.GetActiveEquipmentProfileId(); }
        EquipmentSlot GetActiveEquipmentSlot() const { return m_weapons.GetActiveEquipmentSlot(); }
        EquipmentProfileId GetActiveEquipmentProfileId() const
        {
            return m_weapons.GetActiveEquipmentProfileId();
        }
        const EquipmentProfile& GetActiveEquipmentProfile() const
        {
            return m_weapons.GetActiveEquipmentProfile();
        }
        static const EquipmentProfile& GetEquipmentProfile(EquipmentProfileId profileId)
        {
            return WeaponModel::GetEquipmentProfile(profileId);
        }
        static const WeaponProfile& GetWeaponProfile(WeaponId weaponId)
        {
            return WeaponModel::GetWeaponProfile(weaponId);
        }
        static bool IsValidEquipmentSlot(EquipmentSlot slot)
        {
            return WeaponModel::IsValidEquipmentSlot(slot);
        }
        static bool IsSlotCompatible(EquipmentSlot slot, EquipmentProfileId profileId)
        {
            return WeaponModel::IsSlotCompatible(slot, profileId);
        }
        EquipmentProfileId GetLoadoutProfile(EquipmentSlot slot) const
        {
            return m_weapons.GetLoadoutProfile(slot);
        }
        WeaponEventId GetLastAcceptedUseEventId() const { return m_weapons.GetLastAcceptedUseEventId(); }
        const TargetState& GetTarget() const { return GetEnemy().GetState(); }
        const EnemyCombatModel& GetEnemy() const { return *m_enemyCollection->GetEnemy(PrimaryEnemyId); }
        EnemyCombatModel& GetEnemy() { return *m_enemyCollection->GetEnemy(PrimaryEnemyId); }
        const EnemyCollectionModel& GetEnemies() const { return *m_enemyCollection; }
        EnemyCollectionModel& GetEnemies() { return *m_enemyCollection; }
        const PresentationState& GetPresentation() const { return m_presentation; }
        const PlayerMovementState& GetMovementState() const { return m_movement.GetMovementState(); }
        AZ::Vector3 GetEyePosition() const;
        AZ::Vector3 GetAimDirection() const;
        //! Compatibility facade: returns the immediate requested velocity used by existing
        //! traversal callers. Runtime physics consumes GetMovementVelocity().
        AZ::Vector3 GetDesiredVelocity(const PlayerInput& input) const;
        //! Stateful horizontal velocity produced by PlayerMovementModel, with existing
        //! slide/mantle/jump overrides applied after it.
        AZ::Vector3 GetMovementVelocity() const;
        bool IsMantleRequested() const { return m_player.m_mantleRequested; }
        void BeginMantle(const AZ::Vector3& direction);

        void SetTargetPosition(const AZ::Vector3& position) { m_enemyCollection->SynchronizePhysicalPosition(PrimaryEnemyId, position); }
        void SetPlayerPosition(const AZ::Vector3& position);
        void SynchronizePhysicalState(const AZ::Vector3& position, bool grounded);

    private:
        bool UpdateInternal(float deltaTime, const PlayerInput& input, bool updateEnemySimulation);
        bool RayHitsEnemy(const EnemyState& target, const AZ::Vector3& origin, const AZ::Vector3& direction,
            float maximumRange, float& projectedDistance) const;

        PlayerState m_player;
        WeaponModel m_weapons;
        AZStd::optional<EnemyCollectionModel> m_ownedEnemyCollection;
        EnemyCollectionModel* m_enemyCollection = nullptr;
        PresentationState m_presentation;
        bool m_jumpWasHeld = false;
        bool m_crouchWasHeld = false;
        bool m_mantleWasHeld = false;
        bool m_weaponSwitchWasHeld = false;
        int m_requestedEquipmentSlotWasHeld = -1;
        float m_jumpImpulseThisTick = 0.0f;
        float m_invulnerabilityRemaining = 0.0f;
        PlayerMovementModel m_movement;
    };
}
