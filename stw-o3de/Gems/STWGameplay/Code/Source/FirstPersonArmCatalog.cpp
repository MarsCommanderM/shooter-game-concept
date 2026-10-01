#include <STWGameplay/FirstPersonArmCatalog.h>

#include <AzCore/std/string/string.h>

#include <cstring>

namespace STWGameplay
{
    namespace
    {
        FirstPersonArmPoseBinding Pose(
            FirstPersonArmPose pose, const char* name, const char* path, float right, float forward, float up)
        {
            return FirstPersonArmPoseBinding{pose, name, path, right, forward, up};
        }

        bool ContainsFold(const char* haystack, const char* needle)
        {
            if (haystack == nullptr || needle == nullptr || needle[0] == '\0')
            {
                return false;
            }
            for (const char* cursor = haystack; *cursor != '\0'; ++cursor)
            {
                std::size_t index = 0;
                while (needle[index] != '\0' && cursor[index] != '\0'
                    && (cursor[index] == needle[index]
                        || (cursor[index] >= 'A' && cursor[index] <= 'Z'
                            && cursor[index] - 'A' + 'a' == needle[index])
                        || (needle[index] >= 'A' && needle[index] <= 'Z'
                            && needle[index] - 'A' + 'a' == cursor[index])))
                {
                    ++index;
                }
                if (needle[index] == '\0')
                {
                    return true;
                }
            }
            return false;
        }

        bool NamesActorProduct(const char* path, const char* profileName)
        {
            return path != nullptr && profileName != nullptr && std::strstr(path, ".actor") != nullptr
                && std::strstr(path, ".fbx") == nullptr && std::strstr(path, ".motion") == nullptr
                && std::strstr(path, "stw_fp_01") == nullptr && ContainsFold(path, profileName);
        }

        bool NamesMotionProduct(const char* path, const char* profileName)
        {
            return path != nullptr && profileName != nullptr && std::strstr(path, ".motion") != nullptr
                && std::strstr(path, ".fbx") == nullptr && std::strstr(path, ".actor") == nullptr
                && std::strstr(path, "stw_fp_01") == nullptr && ContainsFold(path, profileName);
        }

        FirstPersonArmBinding Bind(EquipmentProfileId id, const char* name, float scale)
        {
            AZStd::string product = name;
            for (char& character : product)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }
            const AZStd::string root =
                AZStd::string("assets/industrialyard/stw_industrial_yard_01/firstperson/profiles/") + product + "/"
                + product;
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
                path.actor = root + ".actor";
                path.hip = root + "_hip.motion";
                path.ads = root + "_ads.motion";
                path.reload = root + "_reload.motion";
                path.inspect = root + "_inspect.motion";
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

    FirstPersonArmSelection FirstPersonArmCatalog::Select(EquipmentProfileId profileId, FirstPersonArmPose pose)
    {
        FirstPersonArmSelection selection;
        selection.m_requested = profileId;
        selection.m_pose = pose;
        selection.m_loop = pose == FirstPersonArmPose::Hip || pose == FirstPersonArmPose::Ads;

        const FirstPersonArmBinding* binding = Find(profileId);
        const auto poseIndex = static_cast<std::size_t>(pose);
        if (binding == nullptr || binding->m_profileId != profileId || poseIndex >= PoseCount
            || binding->m_actorPath == nullptr || binding->m_profileName == nullptr
            || binding->m_profileName[0] == '\0')
        {
            return selection;
        }

        const FirstPersonArmPoseBinding& poseBinding = binding->m_poses[poseIndex];
        if (poseBinding.m_assetPath == nullptr || poseBinding.m_name == nullptr
            || !NamesActorProduct(binding->m_actorPath, binding->m_profileName)
            || !NamesMotionProduct(poseBinding.m_assetPath, binding->m_profileName)
            || std::strcmp(poseBinding.m_name, pose == FirstPersonArmPose::Hip ? "hip"
                    : pose == FirstPersonArmPose::Ads ? "ads"
                    : pose == FirstPersonArmPose::Reload ? "reload"
                    : "inspect") != 0)
        {
            return selection;
        }

        selection.m_binding = binding;
        selection.m_selected = binding->m_profileId;
        selection.m_actorPath = binding->m_actorPath;
        selection.m_motionPath = poseBinding.m_assetPath;
        selection.m_poseName = poseBinding.m_name;
        selection.m_handSocketLeft = binding->m_handSocketLeft;
        selection.m_handSocketRight = binding->m_handSocketRight;
        selection.m_right = poseBinding.m_right;
        selection.m_forward = poseBinding.m_forward;
        selection.m_up = poseBinding.m_up;
        selection.m_owned = true;
        return selection;
    }

    FirstPersonArmPose FirstPersonArmCatalog::PoseFor(bool reloading, float adsBlend, bool inspect)
    {
        if (reloading)
        {
            return FirstPersonArmPose::Reload;
        }
        if (inspect)
        {
            return FirstPersonArmPose::Inspect;
        }
        if (adsBlend > 0.5f)
        {
            return FirstPersonArmPose::Ads;
        }
        return FirstPersonArmPose::Hip;
    }

    bool FirstPersonArmCatalog::ConfirmRuntime(
        FirstPersonArmSelection& selection, const char* loadedMotionPath, bool meshVisible)
    {
        selection.m_resolved = false;
        if (!selection.m_owned || !meshVisible || selection.m_binding == nullptr
            || loadedMotionPath == nullptr || selection.m_actorPath == nullptr
            || selection.m_motionPath == nullptr || selection.m_binding->m_profileName == nullptr)
        {
            return false;
        }
        if (std::strcmp(loadedMotionPath, selection.m_motionPath) != 0)
        {
            return false;
        }
        if (!NamesActorProduct(selection.m_actorPath, selection.m_binding->m_profileName)
            || !NamesMotionProduct(loadedMotionPath, selection.m_binding->m_profileName))
        {
            return false;
        }
        selection.m_resolved = true;
        return true;
    }

    const char* FirstPersonArmCatalog::MarkerFor(const FirstPersonArmSelection* selections, std::size_t count)
    {
        static const char* poseNames[PoseCount] = {"hip", "ads", "reload", "inspect"};
        static AZStd::string marker;
        marker = "ATOM_FIRSTPERSON_ARMS_MESH result=";

        const char* names[WeaponModel::EquipmentProfileCount] = {};
        bool poseSeen[PoseCount] = {};
        bool anyOwned = false;
        bool socketsOk = true;

        if (selections != nullptr)
        {
            for (std::size_t index = 0; index < count; ++index)
            {
                const FirstPersonArmSelection& selection = selections[index];
                const auto profileIndex = static_cast<std::size_t>(selection.m_requested);
                const auto poseIndex = static_cast<std::size_t>(selection.m_pose);
                if (!selection.m_owned || !selection.m_resolved || selection.m_binding == nullptr
                    || selection.m_selected != selection.m_requested
                    || selection.m_binding->m_profileId != selection.m_requested
                    || profileIndex >= WeaponModel::EquipmentProfileCount || poseIndex >= PoseCount
                    || selection.m_actorPath == nullptr || selection.m_motionPath == nullptr
                    || selection.m_poseName == nullptr || selection.m_binding->m_profileName == nullptr
                    || !NamesActorProduct(selection.m_actorPath, selection.m_binding->m_profileName)
                    || !NamesMotionProduct(selection.m_motionPath, selection.m_binding->m_profileName)
                    || std::strcmp(selection.m_poseName, poseNames[poseIndex]) != 0
                    || selection.m_handSocketLeft == nullptr || selection.m_handSocketRight == nullptr
                    || std::strcmp(selection.m_handSocketLeft, "hand_L") != 0
                    || std::strcmp(selection.m_handSocketRight, "hand_R") != 0)
                {
                    if (selection.m_owned)
                    {
                        socketsOk = false;
                    }
                    continue;
                }

                names[profileIndex] = selection.m_binding->m_profileName;
                poseSeen[poseIndex] = true;
                anyOwned = true;
            }
        }

        bool complete = anyOwned && socketsOk;
        for (const char* name : names)
        {
            if (name == nullptr)
            {
                complete = false;
            }
        }
        for (const bool seen : poseSeen)
        {
            if (!seen)
            {
                complete = false;
            }
        }

        marker += complete ? "PASS" : "FAIL";
        for (std::size_t index = 0; index < WeaponModel::EquipmentProfileCount; ++index)
        {
            if (names[index] == nullptr)
            {
                continue;
            }
            marker += " profile=";
            marker += names[index];
        }
        if (poseSeen[0] && poseSeen[1] && poseSeen[2] && poseSeen[3])
        {
            marker += " poses=hip,ads,reload,inspect";
        }
        if (anyOwned && socketsOk)
        {
            marker += " sockets=hand_L,hand_R";
        }
        return marker.c_str();
    }
}
