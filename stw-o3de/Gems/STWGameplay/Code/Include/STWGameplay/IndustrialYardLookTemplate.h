#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>

#include <cstddef>

namespace STWGameplay
{
    class IndustrialYardLookTemplate final
    {
    public:
        enum class Anchor
        {
            FloorSurface,
            NorthWall,
            SouthWall,
            EastWall,
            WestWall,
            LeftCover,
            RightCover,
            Step,
            AboveArena,
            BeyondWestWall,
            //! The West Annex building envelope: flush with the west wall's
            //! doorway plane (x=-12), unlike BeyondWestWall's 0.25 m clearance
            //! requirement, which a seamless doorway connection cannot satisfy.
            WestAnnex,
            //! The North Scrapyard/crane envelope: flush with the north
            //! wall's doorway plane (y=12), and tall enough (the boom reaches
            //! z~7.1 m) that AboveArena's arena-footprint x/y bound would
            //! reject it - the boom deliberately reaches back over the yard.
            NorthScrapyard,
            //! The East Containerhof envelope: flush with the east wall's
            //! doorway plane (x=12). A third, low-rise landmark (max height
            //! ~2.6 m) - too short for AboveArena and on the wrong side for
            //! BeyondWestWall/WestAnnex/NorthScrapyard.
            EastContainerhof,
            //! The South Verladezone envelope: flush with the south wall's
            //! doorway plane (y=-12). Fourth and final cardinal landmark -
            //! completes the crossing route network.
            SouthVerladezone,
            //! The NW connector envelope: an outdoor walkway in the
            //! previously-empty exterior corner (x<-12, y>12) linking the
            //! West Annex directly to the North Scrapyard, bypassing the
            //! central hof - a real crossing route, not another landmark.
            ConnectorNW
        };

        struct Piece
        {
            const char* m_name;
            const char* m_visualGroup;
            AZ::Vector3 m_center;
            AZ::Vector3 m_size;
            Anchor m_anchor;
        };

        struct MaterialIntent
        {
            const char* m_name;
            float m_dryRoughness;
            float m_wetRoughness;
            float m_metallic;
        };

        static constexpr size_t PieceCount = 52;
        static constexpr size_t MaterialCount = 4;

        static AZStd::array<Piece, PieceCount> GetPieces();
        static AZStd::array<MaterialIntent, MaterialCount> GetMaterialIntents();
        static bool ValidatePiece(const Piece& piece);
        static bool ValidateSceneContract();
    };
}
