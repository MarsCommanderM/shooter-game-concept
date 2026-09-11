#include <AzTest/AzTest.h>

#include "Clients/PhysXArenaRuntime.h"

namespace STWGameplay
{
    TEST(PhysXArenaRuntimeTests, StaticColliderContractRemainsCanonical)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        ASSERT_EQ(colliders.size(), PhysXArenaRuntime::StaticColliderCount);

        const AZ::Vector3 expectedCenters[] = {
            AZ::Vector3(0.0f, 0.0f, -0.5f),
            AZ::Vector3(0.0f, 12.0f, 2.0f),
            AZ::Vector3(0.0f, -12.0f, 2.0f),
            AZ::Vector3(12.0f, 0.0f, 2.0f),
            AZ::Vector3(-12.0f, 0.0f, 2.0f),
            AZ::Vector3(-2.25f, 0.0f, 1.25f),
            AZ::Vector3(2.25f, 0.0f, 1.25f),
            AZ::Vector3(5.0f, -2.0f, 0.125f)
        };
        const AZ::Vector3 expectedDimensions[] = {
            AZ::Vector3(24.0f, 24.0f, 1.0f),
            AZ::Vector3(24.0f, 0.5f, 4.0f),
            AZ::Vector3(24.0f, 0.5f, 4.0f),
            AZ::Vector3(0.5f, 24.0f, 4.0f),
            AZ::Vector3(0.5f, 24.0f, 4.0f),
            AZ::Vector3(1.5f, 2.0f, 2.5f),
            AZ::Vector3(1.5f, 2.0f, 2.5f),
            AZ::Vector3(2.0f, 2.0f, 0.25f)
        };

        for (size_t index = 0; index < colliders.size(); ++index)
        {
            ASSERT_NE(colliders[index].m_name, nullptr);
            EXPECT_TRUE(colliders[index].m_center.IsClose(expectedCenters[index]));
            EXPECT_TRUE(colliders[index].m_dimensions.IsClose(expectedDimensions[index]));
            EXPECT_TRUE(colliders[index].m_center.IsFinite());
            EXPECT_TRUE(colliders[index].m_dimensions.IsFinite());
            EXPECT_GT(colliders[index].m_dimensions.GetMinElement(), 0.0f);
            for (size_t other = index + 1; other < colliders.size(); ++other)
            {
                EXPECT_STRNE(colliders[index].m_name, colliders[other].m_name);
            }
        }
    }

    TEST(PhysXArenaRuntimeTests, StaticColliderDescriptionsHaveNoGameplayDependency)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        EXPECT_EQ(colliders.size(), PhysXArenaRuntime::StaticColliderCount);
        EXPECT_EQ(PhysXArenaRuntime::GetStaticColliderDescriptions().data(), colliders.data());
    }
}
