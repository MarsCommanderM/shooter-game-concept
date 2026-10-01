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

    //! The arm the runtime will show for one requested profile and pose.
    //! m_owned is true only when that profile's own actor, pose path, and sockets were selected.
    struct FirstPersonArmSelection
    {
        EquipmentProfileId m_requested = EquipmentProfileId::STW_SMG_01;
        EquipmentProfileId m_selected = EquipmentProfileId::STW_SMG_01;
        const FirstPersonArmBinding* m_binding = nullptr;
        FirstPersonArmPose m_pose = FirstPersonArmPose::Hip;
        const char* m_actorPath = "";
        const char* m_motionPath = "";
        const char* m_poseName = "";
        const char* m_handSocketLeft = "";
        const char* m_handSocketRight = "";
        float m_right = 0.0f;
        float m_forward = 0.0f;
        float m_up = 0.0f;
        bool m_loop = true;
        bool m_owned = false;
        //! Set only by ConfirmRuntime after the presentation loaded this selection's own actor and motion.
        bool m_resolved = false;
    };

    //! Data the unit test and the runtime marker share. No game process is required to read it.
    class FirstPersonArmCatalog final
    {
    public:
        static constexpr std::size_t PoseCount = 4;

        static const FirstPersonArmBinding* All();
        static std::size_t Count();
        static const FirstPersonArmBinding* Find(EquipmentProfileId profileId);
        //! Binding the presentation loads for this profile. A rifle-only choice leaves m_owned false
        //! for every other profile, and it never substitutes the shared stw_fp_01 actor.
        static FirstPersonArmSelection Select(EquipmentProfileId profileId, FirstPersonArmPose pose);
        //! Reload wins, then inspect, then ADS, otherwise hip. Presentation uses this same mapping.
        static FirstPersonArmPose PoseFor(bool reloading, float adsBlend, bool inspect);
        //! Marks m_resolved only when the loaded actor and motion are this selection's own products
        //! and the skinned mesh and motion are actually ready. A table entry alone does not resolve.
        static bool ConfirmRuntime(
            FirstPersonArmSelection& selection, const char* loadedActorPath, const char* loadedMotionPath,
            bool meshVisible, bool motionReady);
        //! PASS only when every profile and pose was ConfirmRuntime'd from its own loaded products.
        //! The returned pointer is replaced by the next call.
        static const char* MarkerFor(const FirstPersonArmSelection* selections, std::size_t count);
    };
}
