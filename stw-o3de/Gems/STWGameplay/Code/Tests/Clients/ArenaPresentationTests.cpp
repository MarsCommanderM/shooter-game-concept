#include <gtest/gtest.h>

#include <STWGameplay/ArenaPresentation.h>

namespace STWGameplay
{
    TEST(ArenaPresentationTests, UsesAStableVisualAssetSet)
    {
        EXPECT_EQ(ArenaPresentation::GetVisualAssetCount(), 9u);
    }

    TEST(ArenaPresentationTests, PresentationDoesNotOwnGameplayAuthority)
    {
        EXPECT_TRUE(ArenaPresentation::IsVisualOnly());
    }

    namespace
    {
        using DiscoveryFlags = AZStd::array<bool, ArenaPresentation::VisualAssetCount>;

        DiscoveryFlags AllOf(bool value)
        {
            DiscoveryFlags flags{};
            flags.fill(value);
            return flags;
        }

        DiscoveryFlags AllButOne(bool value, size_t index)
        {
            DiscoveryFlags flags = AllOf(value);
            flags[index] = !value;
            return flags;
        }

        // The whole catalog resolved: every path found and every identity valid.
        uint32_t UnresolvedMaskForFullyValidCatalog()
        {
            return ArenaPresentation::ComputeUnresolvedMask(
                AllOf(true), AllOf(true), AllOf(true), AllOf(true), AllOf(false));
        }
    }

    TEST(ArenaPresentationTests, DiscoveryIsCompleteOnlyWhenEveryAssetResolves)
    {
        EXPECT_EQ(UnresolvedMaskForFullyValidCatalog(), 0u);
    }

    TEST(ArenaPresentationTests, DiscoveryStaysOpenWhenAModelIsMissing)
    {
        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllButOne(true, 4), AllOf(true), AllOf(true), AllOf(true), AllOf(false));

        EXPECT_EQ(unresolved, 1u << 4);
    }

    TEST(ArenaPresentationTests, DiscoveryStaysOpenWhenOnlyTheMaterialIsMissing)
    {
        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(true), AllButOne(true, 7), AllOf(true), AllOf(true), AllOf(false));

        EXPECT_EQ(unresolved, 1u << 7);
    }

    // ---- Block 26E-R4: the mask uses the same criterion as runtime readiness --
    //
    // Every case below has the catalog path present. Before R4 that alone
    // cleared the bit, m_assetsDiscovered flipped to true, discovery stopped for
    // good, and IsGeometryReady() stayed false because the mesh could never be
    // acquired from an invalid AssetId.

    TEST(ArenaPresentationTests, PathFoundWithBothIdentitiesValidResolves)
    {
        EXPECT_EQ(UnresolvedMaskForFullyValidCatalog(), 0u);
    }

    TEST(ArenaPresentationTests, PathFoundWithAnInvalidModelIdStaysUnresolved)
    {
        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(true), AllOf(true), AllButOne(true, 3), AllOf(true), AllOf(false));

        EXPECT_EQ(unresolved, 1u << 3);
    }

    TEST(ArenaPresentationTests, PathFoundWithAnInvalidMaterialIdStaysUnresolved)
    {
        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(true), AllOf(true), AllOf(true), AllButOne(true, 6), AllOf(false));

        EXPECT_EQ(unresolved, 1u << 6);
    }

    TEST(ArenaPresentationTests, AValidModelDoesNotResolveAnEntryWithAnInvalidMaterial)
    {
        DiscoveryFlags materialIds = AllOf(true);
        materialIds[1] = false;

        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(true), AllOf(true), AllOf(true), materialIds, AllOf(false));

        EXPECT_EQ(unresolved, 1u << 1);
    }

    TEST(ArenaPresentationTests, AValidMaterialDoesNotResolveAnEntryWithAnInvalidModel)
    {
        DiscoveryFlags modelIds = AllOf(true);
        modelIds[8] = false;

        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(true), AllOf(true), modelIds, AllOf(true), AllOf(false));

        EXPECT_EQ(unresolved, 1u << 8);
    }

    TEST(ArenaPresentationTests, AllNineValidIdentitiesCompleteDiscovery)
    {
        EXPECT_EQ(UnresolvedMaskForFullyValidCatalog(), 0u);
        EXPECT_EQ(ArenaPresentation::GetVisualAssetCount(), 9u);
    }

    TEST(ArenaPresentationTests, OneOfNineInvalidLeavesDiscoveryPending)
    {
        for (size_t index = 0; index < ArenaPresentation::VisualAssetCount; ++index)
        {
            const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
                AllOf(true), AllOf(true), AllButOne(true, index), AllOf(true), AllOf(false));

            EXPECT_NE(unresolved, 0u) << "entry " << index << " must stay pending";
            EXPECT_EQ(unresolved, 1u << index);
        }
    }

    TEST(ArenaPresentationTests, AnUnresolvedEntryKeepsDiscoveryRetrying)
    {
        // DiscoverAssets() sets m_assetsDiscovered = (mask == 0), and Update()
        // only re-enumerates the catalog while m_assetsDiscovered is false. A
        // non-zero mask is therefore exactly "retry remains active".
        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(true), AllOf(true), AllButOne(true, 5), AllOf(true), AllOf(false));

        const bool assetsDiscovered = unresolved == 0;
        EXPECT_FALSE(assetsDiscovered);
        EXPECT_GT(ArenaPresentation::DiscoveryRetryUpdates, 0u);
    }

    TEST(ArenaPresentationTests, DiscoveryRetryKeepsAlreadyResolvedAssets)
    {
        // A later catalog pass sees nothing new; assets resolved by an earlier
        // pass must stay resolved rather than regressing to unresolved.
        DiscoveryFlags alreadyDiscovered = AllOf(true);
        alreadyDiscovered[2] = false;

        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(false), AllOf(false), AllOf(false), AllOf(false), alreadyDiscovered);

        EXPECT_EQ(unresolved, 1u << 2);
    }

    TEST(ArenaPresentationTests, DiscoveryReportsEveryAssetUnresolvedOnAnEmptyCatalog)
    {
        const uint32_t unresolved = ArenaPresentation::ComputeUnresolvedMask(
            AllOf(false), AllOf(false), AllOf(false), AllOf(false), AllOf(false));

        EXPECT_EQ(unresolved, (1u << ArenaPresentation::VisualAssetCount) - 1u);
    }

    TEST(ArenaPresentationTests, DiscoveryRetriesOnACadenceRatherThanEveryUpdate)
    {
        EXPECT_GT(ArenaPresentation::DiscoveryRetryUpdates, 1u);
    }

    TEST(ArenaPresentationTests, CameraSpaceRelationDetectsCameraInsideBounds)
    {
        const AZ::Aabb worldBounds = AZ::Aabb::CreateFromMinMax(
            AZ::Vector3(-2.0f, -3.0f, -1.0f), AZ::Vector3(2.0f, 5.0f, 3.0f));

        const ArenaPresentation::CameraSpaceRelation relation =
            ArenaPresentation::CalculateCameraSpaceRelation(
                worldBounds,
                AZ::Vector3::CreateZero(),
                AZ::Vector3::CreateAxisX(),
                AZ::Vector3::CreateAxisY(),
                AZ::Vector3::CreateAxisZ());

        EXPECT_TRUE(relation.m_valid);
        EXPECT_TRUE(relation.m_cameraInsideBounds);
        EXPECT_TRUE(relation.m_cameraSpaceCenter.IsClose(AZ::Vector3(0.0f, 1.0f, 1.0f)));
    }

    TEST(ArenaPresentationTests, CameraSpaceRelationReportsForwardLateralAndVerticalOffsets)
    {
        const AZ::Aabb worldBounds = AZ::Aabb::CreateFromMinMax(
            AZ::Vector3(-1.0f, 0.0f, -1.0f), AZ::Vector3(1.0f, 2.0f, 1.0f));

        const ArenaPresentation::CameraSpaceRelation relation =
            ArenaPresentation::CalculateCameraSpaceRelation(
                worldBounds,
                AZ::Vector3(0.0f, -5.0f, 0.5f),
                AZ::Vector3::CreateAxisX(),
                AZ::Vector3::CreateAxisY(),
                AZ::Vector3::CreateAxisZ());

        EXPECT_TRUE(relation.m_valid);
        EXPECT_FALSE(relation.m_cameraInsideBounds);
        EXPECT_TRUE(relation.m_cameraSpaceCenter.IsClose(AZ::Vector3(0.0f, 6.0f, -0.5f)));
    }

    TEST(ArenaPresentationTests, CameraSpaceRelationRejectsDegenerateCameraBasis)
    {
        const AZ::Aabb worldBounds = AZ::Aabb::CreateFromMinMax(
            AZ::Vector3(-1.0f, -1.0f, -1.0f), AZ::Vector3(1.0f, 1.0f, 1.0f));

        const ArenaPresentation::CameraSpaceRelation relation =
            ArenaPresentation::CalculateCameraSpaceRelation(
                worldBounds,
                AZ::Vector3::CreateZero(),
                AZ::Vector3::CreateZero(),
                AZ::Vector3::CreateAxisY(),
                AZ::Vector3::CreateAxisZ());

        EXPECT_FALSE(relation.m_valid);
        EXPECT_FALSE(relation.m_cameraInsideBounds);
    }
}
