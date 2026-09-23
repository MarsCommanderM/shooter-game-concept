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

    TEST(IndustrialYardLookTemplateTests, NorthScrapyardCraneBoomReachesOverTheYard)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "crane_boom");
        ASSERT_LT(index, pieces.size());
        EXPECT_EQ(pieces[index].m_anchor, IndustrialYardLookTemplate::Anchor::NorthScrapyard);
        // The boom is the one piece that legitimately reaches back over the
        // main arena floor (y well below the 12 m wall) while staying well
        // above the 4 m wall height - that combination is exactly why
        // NorthScrapyard exists instead of reusing AboveArena or
        // BeyondWestWall-style anchors.
        EXPECT_LT(pieces[index].m_center.GetY() - pieces[index].m_size.GetY() * 0.5f, 12.0f);
        EXPECT_GT(pieces[index].m_center.GetZ(), 4.0f);
        EXPECT_TRUE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));

        // A piece that strays outside the crane's authored envelope must
        // still be rejected.
        auto intrusion = pieces[index];
        intrusion.m_center = AZ::Vector3(20.0f, 8.5f, 7.1f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(intrusion));
    }

    TEST(IndustrialYardLookTemplateTests, EastContainerhofPlatformSitsFlushWithDoorwayPlane)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "container_platform");
        ASSERT_LT(index, pieces.size());
        EXPECT_EQ(pieces[index].m_anchor, IndustrialYardLookTemplate::Anchor::EastContainerhof);
        EXPECT_TRUE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));

        // A piece that reaches back across the doorway into the main arena
        // floor must still be rejected - EastContainerhof is a bounded
        // envelope, not an "anything goes" anchor.
        auto intrusion = pieces[index];
        intrusion.m_center = AZ::Vector3(8.0f, 0.0f, -0.05f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(intrusion));
    }

    TEST(IndustrialYardLookTemplateTests, SouthVerladezoneDockPlatformSitsFlushWithDoorwayPlane)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "dock_platform");
        ASSERT_LT(index, pieces.size());
        EXPECT_EQ(pieces[index].m_anchor, IndustrialYardLookTemplate::Anchor::SouthVerladezone);
        EXPECT_TRUE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));

        // A piece that reaches back across the doorway into the main arena
        // floor must still be rejected - SouthVerladezone is a bounded
        // envelope, not an "anything goes" anchor.
        auto intrusion = pieces[index];
        intrusion.m_center = AZ::Vector3(0.0f, -8.0f, -0.05f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(intrusion));
    }

    TEST(IndustrialYardLookTemplateTests, ConnectorNWLinksAnnexToScrapyardWithoutTouchingCentralFloor)
    {
        auto pieces = IndustrialYardLookTemplate::GetPieces();
        const size_t index = FindPieceIndex(pieces, "connector_nw_ground_b");
        ASSERT_LT(index, pieces.size());
        EXPECT_EQ(pieces[index].m_anchor, IndustrialYardLookTemplate::Anchor::ConnectorNW);
        EXPECT_TRUE(IndustrialYardLookTemplate::ValidatePiece(pieces[index]));

        // A piece that reaches into the central 24x24 arena floor must
        // still be rejected - ConnectorNW is a bounded exterior-corner
        // envelope, not an "anything goes" anchor.
        auto intrusion = pieces[index];
        intrusion.m_center = AZ::Vector3(0.0f, 0.0f, -0.05f);
        EXPECT_FALSE(IndustrialYardLookTemplate::ValidatePiece(intrusion));
    }
}
