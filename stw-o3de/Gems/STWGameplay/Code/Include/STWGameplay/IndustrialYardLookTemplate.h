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
            BeyondWestWall
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

        static constexpr size_t PieceCount = 14;
        static constexpr size_t MaterialCount = 4;

        static AZStd::array<Piece, PieceCount> GetPieces();
        static AZStd::array<MaterialIntent, MaterialCount> GetMaterialIntents();
        static bool ValidatePiece(const Piece& piece);
        static bool ValidateSceneContract();
    };
}
