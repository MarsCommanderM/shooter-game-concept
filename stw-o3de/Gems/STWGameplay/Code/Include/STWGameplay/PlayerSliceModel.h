#pragma once

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>

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
        AZ::Vector3 m_position = AZ::Vector3(0.0f, -6.0f, 0.0f);
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
        float m_maxHealth = 100.0f;
        float m_health = 100.0f;
        bool m_alive = true;
        bool m_grounded = true;
    };

    struct WeaponState
    {
        int m_magazine = 30;
        int m_reserve = 150;
        float m_cooldownRemaining = 0.0f;
        float m_reloadRemaining = 0.0f;
        bool m_reloading = false;
    };

    struct TargetState
    {
        AZ::Vector3 m_position = AZ::Vector3(0.0f, 6.0f, 1.2f);
        float m_radius = 0.75f;
        float m_maxHealth = 100.0f;
        float m_health = 100.0f;
        bool m_alive = true;
        int m_deathEvents = 0;
    };

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

        const PlayerState& GetPlayer() const { return m_player; }
        const WeaponState& GetWeapon() const { return m_weapon; }
        const TargetState& GetTarget() const { return m_target; }
        const PresentationState& GetPresentation() const { return m_presentation; }
        AZ::Vector3 GetEyePosition() const;
        AZ::Vector3 GetAimDirection() const;

        void SetTargetPosition(const AZ::Vector3& position) { m_target.m_position = position; }
        void SetPlayerPosition(const AZ::Vector3& position);

    private:
        AZ::Vector3 ResolveMovement(const AZ::Vector3& from, const AZ::Vector3& displacement) const;
        bool RayHitsTarget(const AZ::Vector3& origin, const AZ::Vector3& direction) const;
        void FinishReload();

        PlayerState m_player;
        WeaponState m_weapon;
        TargetState m_target;
        PresentationState m_presentation;
        AZStd::array<AZ::Aabb, 3> m_cover = {
            AZ::Aabb::CreateFromMinMax(AZ::Vector3(-3.0f, -1.0f, 0.0f), AZ::Vector3(-1.5f, 1.0f, 2.5f)),
            AZ::Aabb::CreateFromMinMax(AZ::Vector3(1.5f, -1.0f, 0.0f), AZ::Vector3(3.0f, 1.0f, 2.5f)),
            AZ::Aabb::CreateFromMinMax(AZ::Vector3(-1.0f, 2.5f, 0.0f), AZ::Vector3(1.0f, 3.3f, 1.4f)) };
    };
}
