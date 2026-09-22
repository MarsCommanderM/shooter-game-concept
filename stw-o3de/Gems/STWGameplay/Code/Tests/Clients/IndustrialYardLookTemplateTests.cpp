#include <AzTest/AzTest.h>

#include <STWGameplay/IndustrialYardLookTemplate.h>

namespace STWGameplay
{
    TEST(IndustrialYardLookTemplateTests, DefaultVisualBoundsRespectExistingArena)
    {
        EXPECT_TRUE(IndustrialYardLookTemplate::ValidateSceneContract());
    }

    TEST(IndustrialYardLookTemplateTests, RejectsSolidObjectWithoutColliderInPlayerLane)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        pieces[0].m_center = AZ::Vector3(0.0f, 0.0f, 0.8f);
        pieces[0].m_size = AZ::Vector3(2.0f, 2.0f, 1.6f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(pieces[0]));
    }

    TEST(IndustrialYardLookTemplateTests, RejectsLowHangingUncollidedGallery)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        pieces[8].m_center = AZ::Vector3(0.0f, 6.0f, 2.0f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(pieces[8]));
    }

    TEST(IndustrialYardLookTemplateTests, RejectsCoverSkinOutsidePhysXCover)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        pieces[5].m_size = AZ::Vector3(3.0f, 2.0f, 2.5f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(pieces[5]));
    }
}
