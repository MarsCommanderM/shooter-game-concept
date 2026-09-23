#include <AzTest/AzTest.h>

#include <cstring>

#include "Clients/PhysXArenaRuntime.h"

namespace STWGameplay
{
    namespace
    {
        const PhysXArenaRuntime::StaticColliderDescription* FindCollider(
            const AZStd::array<PhysXArenaRuntime::StaticColliderDescription, PhysXArenaRuntime::StaticColliderCount>&
                colliders,
            const char* name)
        {
            for (const auto& collider : colliders)
            {
                if (collider.m_name != nullptr && std::strcmp(collider.m_name, name) == 0)
                {
                    return &collider;
                }
            }
            return nullptr;
        }

        struct Expectation
        {
            const char* m_name;
            AZ::Vector3 m_center;
            AZ::Vector3 m_dimensions;
            bool m_rotated; // true only for the switchback/loading ramps
        };
    }

    // Name-based, not index-based: a new collider inserted anywhere in the
    // array (as every West Annex / North Scrapyard addition did) must not
    // silently shift every later expectation - that already broke this test
    // once this session.
    TEST(PhysXArenaRuntimeTests, StaticColliderContractRemainsCanonical)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        ASSERT_EQ(colliders.size(), PhysXArenaRuntime::StaticColliderCount);

        const Expectation expectations[] = {
            { "STW Floor", AZ::Vector3(0.0f, 0.0f, -0.5f), AZ::Vector3(24.0f, 24.0f, 1.0f), false },
            { "STW North Wall Left", AZ::Vector3(-6.75f, 12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f), false },
            { "STW North Wall Right", AZ::Vector3(6.75f, 12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f), false },
            { "STW South Wall Left", AZ::Vector3(-6.75f, -12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f), false },
            { "STW South Wall Right", AZ::Vector3(6.75f, -12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f), false },
            { "STW East Wall Left", AZ::Vector3(12.0f, -6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f), false },
            { "STW East Wall Right", AZ::Vector3(12.0f, 6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f), false },
            { "STW West Wall Left", AZ::Vector3(-12.0f, -6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f), false },
            { "STW West Wall Right", AZ::Vector3(-12.0f, 6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f), false },
            { "STW Left Cover", AZ::Vector3(-2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f), false },
            { "STW Right Cover", AZ::Vector3(2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f), false },
            { "STW Step", AZ::Vector3(5.0f, -2.0f, 0.125f), AZ::Vector3(2.0f, 2.0f, 0.25f), false },
            { "STW Annex Ground Floor", AZ::Vector3(-16.0f, 0.0f, -0.05f), AZ::Vector3(8.0f, 8.0f, 0.1f), false },
            { "STW Annex North Wall", AZ::Vector3(-16.0f, 4.0f, 1.75f), AZ::Vector3(8.0f, 0.3f, 3.5f), false },
            { "STW Annex South Wall", AZ::Vector3(-16.0f, -4.0f, 1.75f), AZ::Vector3(8.0f, 0.3f, 3.5f), false },
            { "STW Annex Far Wall", AZ::Vector3(-20.0f, 0.0f, 1.75f), AZ::Vector3(0.3f, 8.0f, 3.5f), false },
            { "STW Annex East Wall North", AZ::Vector3(-12.0f, 2.75f, 1.75f), AZ::Vector3(0.3f, 2.5f, 3.5f), false },
            { "STW Annex East Wall South", AZ::Vector3(-12.0f, -2.75f, 1.75f), AZ::Vector3(0.3f, 2.5f, 3.5f), false },
            { "STW Annex Ramp", AZ::Vector3(-16.0f, 0.0f, 1.75f), AZ::Vector3(6.8963f, 2.5f, 0.2f), true },
            { "STW Annex Upper Floor North", AZ::Vector3(-16.0f, 2.625f, 3.5f), AZ::Vector3(6.0f, 2.75f, 0.15f), false },
            { "STW Annex Upper Floor South", AZ::Vector3(-16.0f, -2.625f, 3.5f), AZ::Vector3(6.0f, 2.75f, 0.15f), false },
            { "STW Scrapyard Ground", AZ::Vector3(0.0f, 17.0f, -0.05f), AZ::Vector3(8.0f, 10.0f, 0.1f), false },
            { "STW Scrapyard Cover A", AZ::Vector3(-2.5f, 14.0f, 0.6f), AZ::Vector3(1.4f, 1.4f, 1.2f), false },
            { "STW Scrapyard Cover B", AZ::Vector3(2.5f, 15.5f, 0.75f), AZ::Vector3(1.6f, 1.6f, 1.5f), false },
            { "STW Crane Ramp 1", AZ::Vector3(-1.5f, 18.0f, 1.775f), AZ::Vector3(6.0747f, 1.8f, 0.2f), true },
            { "STW Crane Landing", AZ::Vector3(-1.0f, 20.0f, 3.5f), AZ::Vector3(2.0f, 1.2f, 0.15f), false },
            { "STW Crane Ramp 2", AZ::Vector3(1.5f, 18.0f, 5.275f), AZ::Vector3(6.0747f, 1.8f, 0.2f), true },
            { "STW Crane Top Platform", AZ::Vector3(0.0f, 17.0f, 7.05f), AZ::Vector3(4.0f, 2.2f, 0.2f), false },
            { "STW Crane Boom", AZ::Vector3(0.0f, 8.5f, 7.1f), AZ::Vector3(1.4f, 15.0f, 0.2f), false },
            { "STW Containerhof Ground", AZ::Vector3(16.0f, 0.0f, -0.05f), AZ::Vector3(8.0f, 12.0f, 0.1f), false },
            { "STW Container Low A", AZ::Vector3(14.0f, -4.3f, 1.25f), AZ::Vector3(3.0f, 1.6f, 2.5f), false },
            { "STW Container Low B", AZ::Vector3(14.0f, 0.0f, 1.25f), AZ::Vector3(3.0f, 1.6f, 2.5f), false },
            { "STW Container Low C", AZ::Vector3(14.0f, 4.3f, 1.25f), AZ::Vector3(3.0f, 1.6f, 2.5f), false },
            { "STW Container Platform Support", AZ::Vector3(18.5f, 0.0f, 1.2f), AZ::Vector3(3.0f, 3.0f, 2.4f), false },
            { "STW Container Ramp", AZ::Vector3(17.0f, -2.0f, 1.225f), AZ::Vector3(3.8108f, 1.8f, 0.2f), true },
            { "STW Container Platform", AZ::Vector3(18.5f, 0.0f, 2.5f), AZ::Vector3(3.4f, 3.4f, 0.2f), false },
            { "STW Verladezone Ground", AZ::Vector3(0.0f, -17.0f, -0.05f), AZ::Vector3(8.0f, 10.0f, 0.1f), false },
            { "STW Dock Platform", AZ::Vector3(0.0f, -20.5f, 0.6f), AZ::Vector3(5.0f, 2.5f, 1.2f), false },
            { "STW Truck Trailer A", AZ::Vector3(-2.6f, -15.0f, 1.1f), AZ::Vector3(1.8f, 4.5f, 2.2f), false },
            { "STW Truck Trailer B", AZ::Vector3(2.6f, -15.0f, 1.1f), AZ::Vector3(1.8f, 4.5f, 2.2f), false },
            { "STW Loading Crates", AZ::Vector3(0.0f, -13.0f, 0.75f), AZ::Vector3(2.2f, 1.6f, 1.5f), false },
            { "STW Dock Ramp", AZ::Vector3(0.0f, -18.75f, 0.625f), AZ::Vector3(2.7518f, 2.5f, 0.2f), true },
            { "STW Connector NW Ground A", AZ::Vector3(-16.0f, 8.5f, -0.05f), AZ::Vector3(3.0f, 9.0f, 0.1f), false },
            { "STW Connector NW Ground B", AZ::Vector3(-10.0f, 13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f), false },
            { "STW Connector NW Cover A", AZ::Vector3(-16.8f, 8.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector NW Cover B", AZ::Vector3(-10.0f, 13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector NW Lane Break", AZ::Vector3(-13.0f, 12.4f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f), false },
            { "STW Connector NE Ground A", AZ::Vector3(16.0f, 9.5f, -0.05f), AZ::Vector3(3.0f, 7.0f, 0.1f), false },
            { "STW Connector NE Ground B", AZ::Vector3(10.0f, 13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f), false },
            { "STW Connector NE Cover A", AZ::Vector3(16.8f, 9.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector NE Cover B", AZ::Vector3(10.0f, 13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector NE Lane Break", AZ::Vector3(13.0f, 13.6f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f), false },
            { "STW Connector SE Ground A", AZ::Vector3(16.0f, -9.5f, -0.05f), AZ::Vector3(3.0f, 7.0f, 0.1f), false },
            { "STW Connector SE Ground B", AZ::Vector3(10.0f, -13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f), false },
            { "STW Connector SE Cover A", AZ::Vector3(16.8f, -9.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector SE Cover B", AZ::Vector3(10.0f, -13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector SE Lane Break", AZ::Vector3(13.0f, -13.6f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f), false },
            { "STW Connector SW Ground A", AZ::Vector3(-16.0f, -8.5f, -0.05f), AZ::Vector3(3.0f, 9.0f, 0.1f), false },
            { "STW Connector SW Ground B", AZ::Vector3(-10.0f, -13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f), false },
            { "STW Connector SW Cover A", AZ::Vector3(-16.8f, -8.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector SW Cover B", AZ::Vector3(-10.0f, -13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f), false },
            { "STW Connector SW Lane Break", AZ::Vector3(-13.0f, -12.4f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f), false },
            { "STW Central Cover NE Low", AZ::Vector3(7.0f, 7.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f), false },
            { "STW Central Cover NE High", AZ::Vector3(7.9f, 7.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f), false },
            { "STW Central Cover NW Low", AZ::Vector3(-7.0f, 7.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f), false },
            { "STW Central Cover NW High", AZ::Vector3(-7.9f, 7.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f), false },
            { "STW Central Cover SE Low", AZ::Vector3(7.0f, -8.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f), false },
            { "STW Central Cover SE High", AZ::Vector3(7.9f, -8.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f), false },
            { "STW Central Cover SW Low", AZ::Vector3(-7.0f, -8.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f), false },
            { "STW Central Cover SW High", AZ::Vector3(-7.9f, -8.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f), false },
        };
        ASSERT_EQ(AZ_ARRAY_SIZE(expectations), PhysXArenaRuntime::StaticColliderCount);

        for (const Expectation& expectation : expectations)
        {
            const auto* collider = FindCollider(colliders, expectation.m_name);
            ASSERT_NE(collider, nullptr) << "missing collider: " << expectation.m_name;
            EXPECT_TRUE(collider->m_center.IsClose(expectation.m_center)) << expectation.m_name;
            EXPECT_TRUE(collider->m_dimensions.IsClose(expectation.m_dimensions, 0.001f)) << expectation.m_name;
            EXPECT_TRUE(collider->m_center.IsFinite());
            EXPECT_TRUE(collider->m_dimensions.IsFinite());
            EXPECT_TRUE(collider->m_rotation.IsFinite());
            EXPECT_GT(collider->m_dimensions.GetMinElement(), 0.0f);
            // Ramps must actually be rotated to follow their climb slope -
            // not left flat and merely mislabeled; everything else must
            // stay axis-aligned.
            if (expectation.m_rotated)
            {
                EXPECT_FALSE(collider->m_rotation.IsClose(AZ::Quaternion::CreateIdentity())) << expectation.m_name;
            }
            else
            {
                EXPECT_TRUE(collider->m_rotation.IsClose(AZ::Quaternion::CreateIdentity())) << expectation.m_name;
            }
        }

        for (size_t index = 0; index < colliders.size(); ++index)
        {
            ASSERT_NE(colliders[index].m_name, nullptr);
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
        const auto* ramp = FindCollider(colliders, "STW Annex Ramp");
        ASSERT_NE(ramp, nullptr);
        // The ramp's local X (length) axis, rotated into world space, must
        // point from the doorway (low, near x=-12) toward the annex's far
        // wall and up to the upper floor height (high, near x=-20, z~3.5) -
        // otherwise it is rotated but climbs the wrong way.
        const AZ::Vector3 climbDirection = ramp->m_rotation.TransformVector(AZ::Vector3::CreateAxisX());
        EXPECT_LT(climbDirection.GetX(), 0.0f);
        EXPECT_GT(climbDirection.GetZ(), 0.0f);
    }

    TEST(PhysXArenaRuntimeTests, CraneSwitchbackRampsClimbInOppositeHorizontalDirections)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const auto* ramp1 = FindCollider(colliders, "STW Crane Ramp 1");
        const auto* ramp2 = FindCollider(colliders, "STW Crane Ramp 2");
        ASSERT_NE(ramp1, nullptr);
        ASSERT_NE(ramp2, nullptr);
        const AZ::Vector3 direction1 = ramp1->m_rotation.TransformVector(AZ::Vector3::CreateAxisX());
        const AZ::Vector3 direction2 = ramp2->m_rotation.TransformVector(AZ::Vector3::CreateAxisX());
        // Both climb (positive Z), but a real switchback reverses horizontal
        // direction between flights - if it didn't, this would be one long
        // straight ramp, not a switchback fitting a compact tower footprint.
        EXPECT_GT(direction1.GetZ(), 0.0f);
        EXPECT_GT(direction2.GetZ(), 0.0f);
        EXPECT_LT(direction1.GetY() * direction2.GetY(), 0.0f);
    }

    TEST(PhysXArenaRuntimeTests, ContainerRampClimbsFromGroundToPlatformHeight)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const auto* ramp = FindCollider(colliders, "STW Container Ramp");
        ASSERT_NE(ramp, nullptr);
        // Climbs from the doorway lane (low, x=15.5) toward the container
        // platform (high, x=18.5, z~2.4) - otherwise it is rotated but
        // climbs the wrong way.
        const AZ::Vector3 climbDirection = ramp->m_rotation.TransformVector(AZ::Vector3::CreateAxisX());
        EXPECT_GT(climbDirection.GetX(), 0.0f);
        EXPECT_GT(climbDirection.GetZ(), 0.0f);
    }

    TEST(PhysXArenaRuntimeTests, DockRampClimbsFromGroundToPlatformHeight)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const auto* ramp = FindCollider(colliders, "STW Dock Ramp");
        ASSERT_NE(ramp, nullptr);
        // Climbs from the doorway lane (low, y=-17.5) deeper into the
        // verladezone (high, y=-20, z~1.2) toward the dock platform.
        const AZ::Vector3 climbDirection = ramp->m_rotation.TransformVector(AZ::Vector3::CreateAxisX());
        EXPECT_LT(climbDirection.GetY(), 0.0f);
        EXPECT_GT(climbDirection.GetZ(), 0.0f);
    }

    TEST(PhysXArenaRuntimeTests, ConnectorNWDeckSegmentsJoinWithoutAGap)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const auto* segmentA = FindCollider(colliders, "STW Connector NW Ground A");
        const auto* segmentB = FindCollider(colliders, "STW Connector NW Ground B");
        ASSERT_NE(segmentA, nullptr);
        ASSERT_NE(segmentB, nullptr);
        // The L-shaped walkway must not have a fall-through gap at its
        // corner: segment A's x-range and segment B's y-range must overlap
        // at their shared corner, not just touch or miss.
        const float segmentAMinX = segmentA->m_center.GetX() - segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentAMaxX = segmentA->m_center.GetX() + segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentBMinX = segmentB->m_center.GetX() - segmentB->m_dimensions.GetX() * 0.5f;
        EXPECT_GT(segmentAMaxX, segmentBMinX);
        EXPECT_LT(segmentAMinX, segmentBMinX);

        const float segmentAMaxY = segmentA->m_center.GetY() + segmentA->m_dimensions.GetY() * 0.5f;
        const float segmentBMinY = segmentB->m_center.GetY() - segmentB->m_dimensions.GetY() * 0.5f;
        const float segmentBMaxY = segmentB->m_center.GetY() + segmentB->m_dimensions.GetY() * 0.5f;
        EXPECT_GT(segmentAMaxY, segmentBMinY);
        EXPECT_LT(segmentAMaxY, segmentBMaxY);
    }

    TEST(PhysXArenaRuntimeTests, ConnectorNEDeckSegmentsJoinWithoutAGap)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const auto* segmentA = FindCollider(colliders, "STW Connector NE Ground A");
        const auto* segmentB = FindCollider(colliders, "STW Connector NE Ground B");
        ASSERT_NE(segmentA, nullptr);
        ASSERT_NE(segmentB, nullptr);
        const float segmentAMinX = segmentA->m_center.GetX() - segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentAMaxX = segmentA->m_center.GetX() + segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentBMaxX = segmentB->m_center.GetX() + segmentB->m_dimensions.GetX() * 0.5f;
        // Mirror of the NW check: segment A's x-range must overlap segment
        // B's max-x edge, not just touch or miss.
        EXPECT_LT(segmentAMinX, segmentBMaxX);
        EXPECT_GT(segmentAMaxX, segmentBMaxX);

        const float segmentAMaxY = segmentA->m_center.GetY() + segmentA->m_dimensions.GetY() * 0.5f;
        const float segmentBMinY = segmentB->m_center.GetY() - segmentB->m_dimensions.GetY() * 0.5f;
        const float segmentBMaxY = segmentB->m_center.GetY() + segmentB->m_dimensions.GetY() * 0.5f;
        EXPECT_GT(segmentAMaxY, segmentBMinY);
        EXPECT_LT(segmentAMaxY, segmentBMaxY);
    }

    TEST(PhysXArenaRuntimeTests, ConnectorSEDeckSegmentsJoinWithoutAGap)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const auto* segmentA = FindCollider(colliders, "STW Connector SE Ground A");
        const auto* segmentB = FindCollider(colliders, "STW Connector SE Ground B");
        ASSERT_NE(segmentA, nullptr);
        ASSERT_NE(segmentB, nullptr);
        const float segmentAMinX = segmentA->m_center.GetX() - segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentAMaxX = segmentA->m_center.GetX() + segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentBMaxX = segmentB->m_center.GetX() + segmentB->m_dimensions.GetX() * 0.5f;
        EXPECT_LT(segmentAMinX, segmentBMaxX);
        EXPECT_GT(segmentAMaxX, segmentBMaxX);

        // Mirror of the NE check across y=0: segment A's y-range (the more
        // negative side) must overlap segment B's min-y edge.
        const float segmentAMinY = segmentA->m_center.GetY() - segmentA->m_dimensions.GetY() * 0.5f;
        const float segmentBMinY = segmentB->m_center.GetY() - segmentB->m_dimensions.GetY() * 0.5f;
        const float segmentBMaxY = segmentB->m_center.GetY() + segmentB->m_dimensions.GetY() * 0.5f;
        EXPECT_LT(segmentAMinY, segmentBMaxY);
        EXPECT_GT(segmentAMinY, segmentBMinY);
    }

    TEST(PhysXArenaRuntimeTests, ConnectorSWDeckSegmentsJoinWithoutAGap)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const auto* segmentA = FindCollider(colliders, "STW Connector SW Ground A");
        const auto* segmentB = FindCollider(colliders, "STW Connector SW Ground B");
        ASSERT_NE(segmentA, nullptr);
        ASSERT_NE(segmentB, nullptr);
        const float segmentAMinX = segmentA->m_center.GetX() - segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentAMaxX = segmentA->m_center.GetX() + segmentA->m_dimensions.GetX() * 0.5f;
        const float segmentBMinX = segmentB->m_center.GetX() - segmentB->m_dimensions.GetX() * 0.5f;
        EXPECT_GT(segmentAMaxX, segmentBMinX);
        EXPECT_LT(segmentAMinX, segmentBMinX);

        const float segmentAMinY = segmentA->m_center.GetY() - segmentA->m_dimensions.GetY() * 0.5f;
        const float segmentBMinY = segmentB->m_center.GetY() - segmentB->m_dimensions.GetY() * 0.5f;
        const float segmentBMaxY = segmentB->m_center.GetY() + segmentB->m_dimensions.GetY() * 0.5f;
        EXPECT_LT(segmentAMinY, segmentBMaxY);
        EXPECT_GT(segmentAMinY, segmentBMinY);
    }

    TEST(PhysXArenaRuntimeTests, CentralYardCoverClustersFormARealStepUp)
    {
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const char* pairs[4][2] = {
            { "STW Central Cover NE Low", "STW Central Cover NE High" },
            { "STW Central Cover NW Low", "STW Central Cover NW High" },
            { "STW Central Cover SE Low", "STW Central Cover SE High" },
            { "STW Central Cover SW Low", "STW Central Cover SW High" },
        };
        for (const auto& pair : pairs)
        {
            const auto* low = FindCollider(colliders, pair[0]);
            const auto* high = FindCollider(colliders, pair[1]);
            ASSERT_NE(low, nullptr) << pair[0];
            ASSERT_NE(high, nullptr) << pair[1];

            const float lowTop = low->m_center.GetZ() + low->m_dimensions.GetZ() * 0.5f;
            const float highTop = high->m_center.GetZ() + high->m_dimensions.GetZ() * 0.5f;
            // A real jump-up step, not a flush or overlapping pair: the high
            // crate's top must clear the low crate's top by a meaningful,
            // still-jumpable amount.
            EXPECT_GT(highTop - lowTop, 0.3f) << pair[1];
            EXPECT_LT(highTop - lowTop, 1.0f) << pair[1];

            // The two crates must not occupy the same footprint (a real
            // staggered pair, not one box hidden inside the other).
            const float planarOffset = (high->m_center - low->m_center).GetLengthSq();
            EXPECT_GT(planarOffset, 0.5f) << pair[1];
        }
    }

    TEST(PhysXArenaRuntimeTests, ConnectorLaneBreaksFormARealChicaneNotADeadEnd)
    {
        // A lane-break pillar that fully overlaps the existing corner cover
        // piece in X would combine with it to block the whole 3 m lane width
        // (this broke once already, caught before the gate: see
        // tools/blender/generate_industrial_yard.py's CONNECTOR_*_PIECES
        // comments). Each pillar must sit at a DIFFERENT x-slice than its
        // connector's Cover B, and each of those two slices must still leave
        // a real, walkable gap somewhere in the 3 m lane width.
        const auto& colliders = PhysXArenaRuntime::GetStaticColliderDescriptions();
        const char* pairs[4][2] = {
            { "STW Connector NW Cover B", "STW Connector NW Lane Break" },
            { "STW Connector NE Cover B", "STW Connector NE Lane Break" },
            { "STW Connector SE Cover B", "STW Connector SE Lane Break" },
            { "STW Connector SW Cover B", "STW Connector SW Lane Break" },
        };
        constexpr float LaneWidthMeters = 3.0f;
        constexpr float MinimumWalkableGapMeters = 0.8f;
        for (const auto& pair : pairs)
        {
            const auto* coverB = FindCollider(colliders, pair[0]);
            const auto* laneBreak = FindCollider(colliders, pair[1]);
            ASSERT_NE(coverB, nullptr) << pair[0];
            ASSERT_NE(laneBreak, nullptr) << pair[1];

            const float coverBMinX = coverB->m_center.GetX() - coverB->m_dimensions.GetX() * 0.5f;
            const float coverBMaxX = coverB->m_center.GetX() + coverB->m_dimensions.GetX() * 0.5f;
            const float laneBreakMinX = laneBreak->m_center.GetX() - laneBreak->m_dimensions.GetX() * 0.5f;
            const float laneBreakMaxX = laneBreak->m_center.GetX() + laneBreak->m_dimensions.GetX() * 0.5f;
            const bool xOverlaps = laneBreakMinX < coverBMaxX && laneBreakMaxX > coverBMinX;
            EXPECT_FALSE(xOverlaps) << pair[1];

            const float coverBGap = LaneWidthMeters - coverB->m_dimensions.GetY();
            const float laneBreakGap = LaneWidthMeters - laneBreak->m_dimensions.GetY();
            EXPECT_GT(coverBGap, MinimumWalkableGapMeters) << pair[0];
            EXPECT_GT(laneBreakGap, MinimumWalkableGapMeters) << pair[1];
        }
    }
}
