#pragma once

#include <AzCore/Math/Vector3.h>
#include <STWGameplay/EnemyCombatModel.h>
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
        bool m_fire = false;
        bool m_reload = false;
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
        int m_damageEvents = 0;
        int m_deathEvents = 0;
        int m_respawnEvents = 0;
    };

    struct WeaponState
    {
        int m_magazine = 30;
        int m_reserve = 150;
        float m_cooldownRemaining = 0.0f;
        float m_reloadRemaining = 0.0f;
        bool m_reloading = false;
    };

    using TargetState = EnemyState; // compatibility name for existing presentation/tests

    struct PresentationState
    {
        bool m_shotFired = false;
        bool m_hit = false;
        float m_fireCueRemaining = 0.0f;
        float m_hitCueRemaining = 0.0f;
    };

    class PlayerSliceModel
    {
    public:
        static constexpr float WalkSpeed = 4.5f;
        static constexpr float SprintSpeed = 7.5f;
        static constexpr float EyeHeight = 1.7f;
        static constexpr float PitchLimit = 1.45f;
        static constexpr float LookSensitivity = 0.0025f;
        static constexpr float FireInterval = 0.075f; // MP5: 800 rounds/minute
        static constexpr float ReloadDuration = 1.75f;
        static constexpr float WeaponRange = 60.0f;
        static constexpr float WeaponDamage = 16.0f;

        bool Update(float deltaTime, const PlayerInput& input);
        bool TryFire();
        bool StartReload();
        bool ApplyDamage(float damage);
        void ResetPlayer();

        const PlayerState& GetPlayer() const { return m_player; }
        const WeaponState& GetWeapon() const { return m_weapon; }
        const TargetState& GetTarget() const { return m_enemy.GetState(); }
        const EnemyCombatModel& GetEnemy() const { return m_enemy; }
        EnemyCombatModel& GetEnemy() { return m_enemy; }
        const PresentationState& GetPresentation() const { return m_presentation; }
        AZ::Vector3 GetEyePosition() const;
        AZ::Vector3 GetAimDirection() const;
        AZ::Vector3 GetDesiredVelocity(const PlayerInput& input) const;

        void SetTargetPosition(const AZ::Vector3& position) { m_enemy.SynchronizePhysicalPosition(position); }
        void SetPlayerPosition(const AZ::Vector3& position);
        void SynchronizePhysicalState(const AZ::Vector3& position, bool grounded);

    private:
        bool RayHitsTarget(const AZ::Vector3& origin, const AZ::Vector3& direction) const;
        void FinishReload();

        PlayerState m_player;
        WeaponState m_weapon;
        EnemyCombatModel m_enemy;
        PresentationState m_presentation;
    };
}
