#include <AzTest/AzTest.h>

#include <STWGameplay/FirstPersonArmCatalog.h>
#include <STWGameplay/WeaponModel.h>

#include <cstring>

namespace STWGameplay
{
    TEST(FirstPersonArmCatalogTests, EveryWeaponProfileHasItsOwnArmPoses)
    {
        ASSERT_EQ(FirstPersonArmCatalog::Count(), WeaponModel::EquipmentProfileCount);
        const char* marker = FirstPersonArmCatalog::ReadyMarker();
        ASSERT_NE(marker, nullptr);
        EXPECT_NE(std::strstr(marker, "ATOM_FIRSTPERSON_ARMS_MESH result=PASS"), nullptr);
        EXPECT_NE(std::strstr(marker, "poses=hip,ads,reload,inspect"), nullptr);
        EXPECT_NE(std::strstr(marker, "sockets=hand_L,hand_R"), nullptr);

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
            EXPECT_NE(std::strstr(marker, binding->m_profileName), nullptr);
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
