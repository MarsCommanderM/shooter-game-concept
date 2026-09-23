#include <AzTest/AzTest.h>

#include <cstring>

#include "Clients/PhysXArenaRuntime.h"

namespace STWGameplay
{
    TEST(PhysXArenaRuntimeTests, StaticColliderContractRemainsCanonical)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        ASSERT_EQ(colliders.size(), PhysXArenaRuntime::StaticColliderCount);

        // Original arena (west wall now split around the West Annex doorway)
        // plus the West Annex itself: ground floor, four walls (one split
        // around the doorway), the climb ramp, and the two-piece upper floor.
        const AZ::Vector3 expectedCenters[] = {
            AZ::Vector3(0.0f, 0.0f, -0.5f),
            AZ::Vector3(0.0f, 12.0f, 2.0f),
            AZ::Vector3(0.0f, -12.0f, 2.0f),
            AZ::Vector3(12.0f, 0.0f, 2.0f),
            AZ::Vector3(-12.0f, -6.75f, 2.0f),
            AZ::Vector3(-12.0f, 6.75f, 2.0f),
            AZ::Vector3(-2.25f, 0.0f, 1.25f),
            AZ::Vector3(2.25f, 0.0f, 1.25f),
            AZ::Vector3(5.0f, -2.0f, 0.125f),
            AZ::Vector3(-16.0f, 0.0f, -0.05f),
            AZ::Vector3(-16.0f, 4.0f, 1.75f),
            AZ::Vector3(-16.0f, -4.0f, 1.75f),
            AZ::Vector3(-20.0f, 0.0f, 1.75f),
            AZ::Vector3(-12.0f, 2.75f, 1.75f),
            AZ::Vector3(-12.0f, -2.75f, 1.75f),
            AZ::Vector3(-16.0f, 0.0f, 1.75f),
            AZ::Vector3(-16.0f, 2.625f, 3.5f),
            AZ::Vector3(-16.0f, -2.625f, 3.5f)
        };
        const AZ::Vector3 expectedDimensions[] = {
            AZ::Vector3(24.0f, 24.0f, 1.0f),
            AZ::Vector3(24.0f, 0.5f, 4.0f),
            AZ::Vector3(24.0f, 0.5f, 4.0f),
            AZ::Vector3(0.5f, 24.0f, 4.0f),
            AZ::Vector3(0.5f, 10.5f, 4.0f),
            AZ::Vector3(0.5f, 10.5f, 4.0f),
            AZ::Vector3(1.5f, 2.0f, 2.5f),
            AZ::Vector3(1.5f, 2.0f, 2.5f),
            AZ::Vector3(2.0f, 2.0f, 0.25f),
            AZ::Vector3(8.0f, 8.0f, 0.1f),
            AZ::Vector3(8.0f, 0.3f, 3.5f),
            AZ::Vector3(8.0f, 0.3f, 3.5f),
            AZ::Vector3(0.3f, 8.0f, 3.5f),
            AZ::Vector3(0.3f, 2.5f, 3.5f),
            AZ::Vector3(0.3f, 2.5f, 3.5f),
            AZ::Vector3(6.8963f, 2.5f, 0.2f),
            AZ::Vector3(6.0f, 2.75f, 0.15f),
            AZ::Vector3(6.0f, 2.75f, 0.15f)
        };
        constexpr size_t RampIndex = 15;

        for (size_t index = 0; index < colliders.size(); ++index)
        {
            ASSERT_NE(colliders[index].m_name, nullptr);
            EXPECT_TRUE(colliders[index].m_center.IsClose(expectedCenters[index]))
                << "index " << index << " (" << colliders[index].m_name << ")";
            EXPECT_TRUE(colliders[index].m_dimensions.IsClose(expectedDimensions[index], 0.001f))
                << "index " << index << " (" << colliders[index].m_name << ")";
            EXPECT_TRUE(colliders[index].m_center.IsFinite());
            EXPECT_TRUE(colliders[index].m_dimensions.IsFinite());
            EXPECT_TRUE(colliders[index].m_rotation.IsFinite());
            EXPECT_GT(colliders[index].m_dimensions.GetMinElement(), 0.0f);
            // Every collider is axis-aligned except the ramp, which must
            // actually be rotated to follow its climb slope - not left flat
            // and merely mislabeled.
            if (index == RampIndex)
            {
                EXPECT_FALSE(colliders[index].m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));
            }
            else
            {
                EXPECT_TRUE(colliders[index].m_rotation.IsClose(AZ::Quaternion::CreateIdentity()))
                    << "index " << index << " (" << colliders[index].m_name << ")";
            }
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

    TEST(PhysXArenaRuntimeTests, WestAnnexRampClimbsFromGroundToUpperFloor)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const PhysXArenaRuntime::StaticColliderDescription* ramp = nullptr;
        for (const auto& collider : colliders)
        {
            if (collider.m_name != nullptr && std::strcmp(collider.m_name, "STW Annex Ramp") == 0)
            {
                ramp = &collider;
                break;
            }
        }
        ASSERT_NE(ramp, nullptr);
        // The ramp's local X (length) axis, rotated into world space, must
        // point from the doorway (low, near x=-12) toward the annex's far
        // wall and up to the upper floor height (high, near x=-20, z~3.5) -
        // otherwise it is rotated but climbs the wrong way.
        const AZ::Vector3 climbDirection = ramp->m_rotation.TransformVector(AZ::Vector3::CreateAxisX());
        EXPECT_LT(climbDirection.GetX(), 0.0f);
        EXPECT_GT(climbDirection.GetZ(), 0.0f);
    }
}
