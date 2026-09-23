#include <AzTest/AzTest.h>

#include <cstring>

#include <STWGameplay/IndustrialYardLookTemplate.h>

namespace STWGameplay
{
    namespace
    {
        // Looks pieces up by name rather than a hardcoded array index, so
        // adding or reordering unrelated pieces (e.g. the West Annex) can
        // never silently make one of these tests exercise the wrong piece.
        size_t FindPieceIndex(
            const AZStd::array<IndustrialYardLookTemplate::Piece, IndustrialYardLookTemplate::PieceCount>& pieces,
            const char* name)
        {
            for (size_t index = 0; index < pieces.size(); ++index)
            {
                if (std::strcmp(pieces[index].m_name, name) == 0)
                {
                    return index;
                }
            }
            return pieces.size();
        }
    }

    TEST(IndustrialYardLookTemplateTests, DefaultVisualBoundsRespectExistingArena)
    {
        EXPECT_TRUE(IndustrialYardLookTemplate::ValidateSceneContract());
    }

    TEST(IndustrialYardLookTemplateTests, RejectsSolidObjectWithoutColliderInPlayerLane)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "wet_concrete_deck");
        ASSERT_LT(index, pieces.size());
        pieces[index].m_center = AZ::Vector3(0.0f, 0.0f, 0.8f);
        pieces[index].m_size = AZ::Vector3(2.0f, 2.0f, 1.6f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));
    }

    TEST(IndustrialYardLookTemplateTests, RejectsLowHangingUncollidedGallery)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "suspended_gallery");
        ASSERT_LT(index, pieces.size());
        pieces[index].m_center = AZ::Vector3(0.0f, 6.0f, 2.0f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));
    }

    TEST(IndustrialYardLookTemplateTests, RejectsCoverSkinOutsidePhysXCover)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "left_cover_cladding");
        ASSERT_LT(index, pieces.size());
        pieces[index].m_size = AZ::Vector3(3.0f, 2.0f, 2.5f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));
    }

    TEST(IndustrialYardLookTemplateTests, WestAnnexPiecesSitFlushWithDoorwayPlane)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "annex_ground_floor");
        ASSERT_LT(index, pieces.size());
        EXPECT_EQ(pieces[index].m_anchor, IndustrialYardLookTemplate::Anchor::WestAnnex);
        EXPECT_TRUE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));

        // A piece that reaches back across the doorway into the main arena
        // floor must still be rejected - WestAnnex is not an "anything
        // goes" anchor, it is a bounded envelope.
        auto intrusion = pieces[index];
        intrusion.m_center = AZ::Vector3(-8.0f, 0.0f, -0.05f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(intrusion));
    }
}
