#pragma once

#include <cstddef>

#include <STWGameplay/WeaponModel.h>

namespace STWGameplay
{
    enum class FirstPersonArmPose : unsigned char
    {
        Hip = 0,
        Ads,
        Reload,
        Inspect
    };

    //! One camera-local presentation of the arms for a single weapon profile.
    //! Offsets are presentation only. They carry no ammo, damage, or authority.
    struct FirstPersonArmPoseBinding
    {
        FirstPersonArmPose m_pose = FirstPersonArmPose::Hip;
        const char* m_name = "";
        const char* m_assetPath = "";
        float m_right = 0.0f;
        float m_forward = 0.0f;
        float m_up = 0.0f;
    };

    //! Hand sockets plus hip, ADS, reload, and inspect for one equipment profile.
    struct FirstPersonArmBinding
    {
        EquipmentProfileId m_profileId = EquipmentProfileId::STW_SMG_01;
        const char* m_profileName = "";
        const char* m_actorPath = "";
        const char* m_handSocketLeft = "";
        const char* m_handSocketRight = "";
        FirstPersonArmPoseBinding m_poses[4] = {};
    };

    //! Data the unit test and the runtime marker share. No game process is required to read it.
    class FirstPersonArmCatalog final
    {
    public:
        static constexpr std::size_t PoseCount = 4;

        static const FirstPersonArmBinding* All();
        static std::size_t Count();
        static const FirstPersonArmBinding* Find(EquipmentProfileId profileId);
        //! One line naming every bound profile. A rifle-only table cannot produce the full line.
        static const char* ReadyMarker();
    };
}
