#include <AzTest/AzTest.h>

#include <STWGameplay/FirstPersonArmCatalog.h>
#include <STWGameplay/WeaponModel.h>

#include <cstring>
#include <string>

namespace STWGameplay
{
    TEST(FirstPersonArmCatalogTests, EveryWeaponProfileHasItsOwnArmPoses)
    {
        ASSERT_EQ(FirstPersonArmCatalog::Count(), WeaponModel::EquipmentProfileCount);
        EXPECT_EQ(FirstPersonArmCatalog::PoseFor(true, 0.0f, true), FirstPersonArmPose::Reload);
        EXPECT_EQ(FirstPersonArmCatalog::PoseFor(false, 0.0f, true), FirstPersonArmPose::Inspect);
        EXPECT_EQ(FirstPersonArmCatalog::PoseFor(false, 0.6f, false), FirstPersonArmPose::Ads);
        EXPECT_EQ(FirstPersonArmCatalog::PoseFor(false, 0.0f, false), FirstPersonArmPose::Hip);

        const FirstPersonArmSelection rifleOnly = FirstPersonArmCatalog::Select(
            EquipmentProfileId::STW_RIFLE_02, FirstPersonArmPose::Hip);
        ASSERT_TRUE(rifleOnly.m_owned);
        const std::string rifleOnlyMarker = FirstPersonArmCatalog::MarkerFor(&rifleOnly, 1);
        EXPECT_EQ(std::strstr(rifleOnlyMarker.c_str(), "result=PASS"), nullptr);

        const FirstPersonArmSelection smgReload = FirstPersonArmCatalog::Select(
            EquipmentProfileId::STW_SMG_01, FirstPersonArmPose::Reload);
        ASSERT_TRUE(smgReload.m_owned);
        EXPECT_EQ(smgReload.m_requested, EquipmentProfileId::STW_SMG_01);
        EXPECT_EQ(smgReload.m_selected, EquipmentProfileId::STW_SMG_01);
        ASSERT_NE(smgReload.m_actorPath, nullptr);
        ASSERT_NE(smgReload.m_motionPath, nullptr);
        EXPECT_EQ(std::strstr(smgReload.m_actorPath, "stw_fp_01"), nullptr);
        EXPECT_NE(std::strstr(smgReload.m_actorPath, "STW_SMG_01"), nullptr);
        EXPECT_NE(std::strstr(smgReload.m_motionPath, "STW_SMG_01"), nullptr);
        EXPECT_STREQ(smgReload.m_poseName, "reload");
        EXPECT_STRNE(smgReload.m_actorPath, rifleOnly.m_actorPath);
        EXPECT_STRNE(smgReload.m_motionPath, rifleOnly.m_motionPath);
        EXPECT_NE(smgReload.m_right, rifleOnly.m_right);

        const FirstPersonArmSelection smgInspect = FirstPersonArmCatalog::Select(
            EquipmentProfileId::STW_SMG_01, FirstPersonArmPose::Inspect);
        ASSERT_TRUE(smgInspect.m_owned);
        EXPECT_STREQ(smgInspect.m_poseName, "inspect");
        EXPECT_STRNE(smgInspect.m_motionPath, smgReload.m_motionPath);
        EXPECT_NE(smgInspect.m_up, smgReload.m_up);

        constexpr std::size_t selectionCount =
            WeaponModel::EquipmentProfileCount * FirstPersonArmCatalog::PoseCount;
        FirstPersonArmSelection selections[selectionCount];
        std::size_t written = 0;
        for (std::size_t index = 0; index < FirstPersonArmCatalog::Count(); ++index)
        {
            for (std::size_t poseIndex = 0; poseIndex < FirstPersonArmCatalog::PoseCount; ++poseIndex)
            {
                selections[written++] = FirstPersonArmCatalog::Select(
                    static_cast<EquipmentProfileId>(index), static_cast<FirstPersonArmPose>(poseIndex));
            }
        }
        const std::string unresolved = FirstPersonArmCatalog::MarkerFor(selections, written);
        EXPECT_EQ(std::strstr(unresolved.c_str(), "result=PASS"), nullptr);

        FirstPersonArmSelection smgWrong = smgReload;
        EXPECT_FALSE(FirstPersonArmCatalog::ConfirmRuntime(
            smgWrong, rifleOnly.m_actorPath, smgReload.m_motionPath, true, true));
        EXPECT_FALSE(smgWrong.m_resolved);
        EXPECT_FALSE(FirstPersonArmCatalog::ConfirmRuntime(
            smgWrong, "assets/industrialyard/stw_industrial_yard_01/firstperson/stw_fp_01.actor",
            smgReload.m_motionPath, true, true));

        FirstPersonArmSelection rifleResolved = rifleOnly;
        EXPECT_TRUE(FirstPersonArmCatalog::ConfirmRuntime(
            rifleResolved, rifleOnly.m_actorPath, rifleOnly.m_motionPath, true, true));
        const std::string rifleOnlyResolved = FirstPersonArmCatalog::MarkerFor(&rifleResolved, 1);
        EXPECT_EQ(std::strstr(rifleOnlyResolved.c_str(), "result=PASS"), nullptr);

        for (std::size_t index = 0; index < written; ++index)
        {
            EXPECT_TRUE(FirstPersonArmCatalog::ConfirmRuntime(
                selections[index], selections[index].m_actorPath, selections[index].m_motionPath, true, true));
        }
        const std::string marker = FirstPersonArmCatalog::MarkerFor(selections, written);
        EXPECT_NE(std::strstr(marker.c_str(), "ATOM_FIRSTPERSON_ARMS_MESH result=PASS"), nullptr);
        EXPECT_NE(std::strstr(marker.c_str(), "poses=hip,ads,reload,inspect"), nullptr);
        EXPECT_NE(std::strstr(marker.c_str(), "sockets=hand_L,hand_R"), nullptr);

        const char* poseNames[FirstPersonArmCatalog::PoseCount] = {"hip", "ads", "reload", "inspect"};
        int profilesBesidesRifle = 0;
        for (std::size_t index = 0; index < FirstPersonArmCatalog::Count(); ++index)
        {
            const auto profileId = static_cast<EquipmentProfileId>(index);
            const FirstPersonArmBinding* binding = FirstPersonArmCatalog::Find(profileId);
            ASSERT_NE(binding, nullptr);
            EXPECT_EQ(binding->m_profileId, profileId);
            ASSERT_NE(binding->m_profileName, nullptr);
            EXPECT_GT(std::strlen(binding->m_profileName), 0u);
            EXPECT_STREQ(binding->m_handSocketLeft, "hand_L");
            EXPECT_STREQ(binding->m_handSocketRight, "hand_R");
            EXPECT_NE(std::strstr(marker.c_str(), binding->m_profileName), nullptr);
            if (std::strcmp(binding->m_profileName, "STW_RIFLE_02") != 0)
            {
                ++profilesBesidesRifle;
            }

            for (std::size_t earlier = 0; earlier < index; ++earlier)
            {
                const FirstPersonArmBinding* previous =
                    FirstPersonArmCatalog::Find(static_cast<EquipmentProfileId>(earlier));
                ASSERT_NE(previous, nullptr);
                EXPECT_STRNE(previous->m_profileName, binding->m_profileName);
                EXPECT_NE(previous->m_poses[0].m_right, binding->m_poses[0].m_right);
            }

            for (std::size_t poseIndex = 0; poseIndex < FirstPersonArmCatalog::PoseCount; ++poseIndex)
            {
                const FirstPersonArmPoseBinding& pose = binding->m_poses[poseIndex];
                EXPECT_STREQ(pose.m_name, poseNames[poseIndex]);
                ASSERT_NE(pose.m_assetPath, nullptr);
                EXPECT_NE(std::strstr(pose.m_assetPath, binding->m_profileName), nullptr);
                for (std::size_t other = poseIndex + 1; other < FirstPersonArmCatalog::PoseCount; ++other)
                {
                    const FirstPersonArmPoseBinding& rest = binding->m_poses[other];
                    const bool sameOffset = pose.m_right == rest.m_right && pose.m_forward == rest.m_forward
                        && pose.m_up == rest.m_up;
                    EXPECT_FALSE(sameOffset);
                    EXPECT_STRNE(pose.m_assetPath, rest.m_assetPath);
                }
            }
        }
        EXPECT_GT(profilesBesidesRifle, 0);
        EXPECT_EQ(FirstPersonArmCatalog::Find(static_cast<EquipmentProfileId>(FirstPersonArmCatalog::Count())), nullptr);
    }
}
