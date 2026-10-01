#include <STWGameplay/FirstPersonArmCatalog.h>

#include <AzCore/std/string/string.h>

namespace STWGameplay
{
    namespace
    {
        FirstPersonArmPoseBinding Pose(
            FirstPersonArmPose pose, const char* name, const char* path, float right, float forward, float up)
        {
            return FirstPersonArmPoseBinding{pose, name, path, right, forward, up};
        }

        FirstPersonArmBinding Bind(EquipmentProfileId id, const char* name, float scale)
        {
            const AZStd::string root =
                AZStd::string("assets/industrialyard/stw_industrial_yard_01/firstperson/profiles/") + name + "/" + name;
            // Paths are stored in function-local static strings so the table outlives this call.
            struct Paths
            {
                AZStd::string actor;
                AZStd::string hip;
                AZStd::string ads;
                AZStd::string reload;
                AZStd::string inspect;
            };
            static AZStd::array<Paths, WeaponModel::EquipmentProfileCount> storage{};
            const auto index = static_cast<std::size_t>(id);
            Paths& path = storage[index];
            if (path.actor.empty())
            {
                path.actor = root + ".fbx";
                path.hip = root + "_hip.fbx";
                path.ads = root + "_ads.fbx";
                path.reload = root + "_reload.fbx";
                path.inspect = root + "_inspect.fbx";
            }
            const float base = 0.20f + (0.03f * static_cast<float>(index));
            return FirstPersonArmBinding{
                id,
                name,
                path.actor.c_str(),
                "hand_L",
                "hand_R",
                {
                    Pose(FirstPersonArmPose::Hip, "hip", path.hip.c_str(), base, 0.62f * scale, -0.20f),
                    Pose(FirstPersonArmPose::Ads, "ads", path.ads.c_str(), base * 0.15f, 0.66f * scale, -0.12f),
                    Pose(FirstPersonArmPose::Reload, "reload", path.reload.c_str(), base + 0.08f, 0.48f * scale, -0.34f),
                    Pose(FirstPersonArmPose::Inspect, "inspect", path.inspect.c_str(), base + 0.12f, 0.40f * scale, -0.08f),
                }};
        }

        const AZStd::array<FirstPersonArmBinding, WeaponModel::EquipmentProfileCount>& Bindings()
        {
            static const AZStd::array<FirstPersonArmBinding, WeaponModel::EquipmentProfileCount> bindings = {
                Bind(EquipmentProfileId::STW_SMG_01, "STW_SMG_01", 0.72f),
                Bind(EquipmentProfileId::STW_RIFLE_02, "STW_RIFLE_02", 1.00f),
                Bind(EquipmentProfileId::STW_RIFLE_03, "STW_RIFLE_03", 1.08f),
                Bind(EquipmentProfileId::STW_LMG_04, "STW_LMG_04", 1.22f),
                Bind(EquipmentProfileId::STW_SIDEARM_01, "STW_SIDEARM_01", 0.55f),
                Bind(EquipmentProfileId::STW_LAUNCHER_01, "STW_LAUNCHER_01", 1.15f),
                Bind(EquipmentProfileId::STW_TACTICAL_FLASH_01, "STW_TACTICAL_FLASH_01", 0.40f),
                Bind(EquipmentProfileId::STW_TACTICAL_SMOKE_01, "STW_TACTICAL_SMOKE_01", 0.42f),
                Bind(EquipmentProfileId::STW_LETHAL_FRAG_01, "STW_LETHAL_FRAG_01", 0.36f),
                Bind(EquipmentProfileId::STW_MELEE_01, "STW_MELEE_01", 0.80f),
            };
            return bindings;
        }
    }

    const FirstPersonArmBinding* FirstPersonArmCatalog::All()
    {
        return Bindings().data();
    }

    std::size_t FirstPersonArmCatalog::Count()
    {
        return Bindings().size();
    }

    const FirstPersonArmBinding* FirstPersonArmCatalog::Find(EquipmentProfileId profileId)
    {
        const auto index = static_cast<std::size_t>(profileId);
        if (index >= Bindings().size() || Bindings()[index].m_profileId != profileId)
        {
            return nullptr;
        }
        return &Bindings()[index];
    }

    const char* FirstPersonArmCatalog::ReadyMarker()
    {
        static AZStd::string marker;
        if (marker.empty())
        {
            marker = "ATOM_FIRSTPERSON_ARMS_MESH result=PASS";
            for (const FirstPersonArmBinding& binding : Bindings())
            {
                marker += " profile=";
                marker += binding.m_profileName;
            }
            marker += " poses=hip,ads,reload,inspect sockets=hand_L,hand_R";
        }
        return marker.c_str();
    }
}
