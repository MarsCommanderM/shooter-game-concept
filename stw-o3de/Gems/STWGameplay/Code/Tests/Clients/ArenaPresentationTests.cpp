#include <gtest/gtest.h>

#include <STWGameplay/ArenaPresentation.h>

namespace STWGameplay
{
    TEST(ArenaPresentationTests, UsesAStableVisualAssetSet)
    {
        EXPECT_EQ(ArenaPresentation::GetVisualAssetCount(), 4u);
    }

    TEST(ArenaPresentationTests, PresentationDoesNotOwnGameplayAuthority)
    {
        EXPECT_TRUE(ArenaPresentation::IsVisualOnly());
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
