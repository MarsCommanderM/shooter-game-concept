#include <STWGameplay/IndustrialYardLookTemplate.h>

#include <STWGameplay/ArenaLayout.h>

namespace STWGameplay
{
    namespace
    {
        constexpr float BoundsTolerance = 0.02f;

        float GetAxis(const AZ::Vector3& value, int axis)
        {
            switch (axis)
            {
            case 0:
                return value.GetX();
            case 1:
                return value.GetY();
            default:
                return value.GetZ();
            }
        }

        bool WithinCollider(
            const IndustrialYardLookTemplate::Piece& piece,
            const AZ::Vector3& colliderCenter,
            const AZ::Vector3& colliderSize)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                const float distance = GetAxis(piece.m_center - colliderCenter, axis);
                const float halfExtent = GetAxis(piece.m_size, axis) * 0.5f;
                const float colliderHalfExtent = GetAxis(colliderSize, axis) * 0.5f;
                if (distance - halfExtent < -colliderHalfExtent - BoundsTolerance
                    || distance + halfExtent > colliderHalfExtent + BoundsTolerance)
                {
                    return false;
                }
            }
            return true;
        }

        bool ValidMaterial(const IndustrialYardLookTemplate::MaterialIntent& intent)
        {
            return intent.m_name && intent.m_name[0] != '\0'
                && intent.m_dryRoughness >= 0.0f && intent.m_dryRoughness <= 1.0f
                && intent.m_wetRoughness >= 0.0f && intent.m_wetRoughness <= intent.m_dryRoughness
                && intent.m_metallic >= 0.0f && intent.m_metallic <= 1.0f;
        }
    }

    AZStd::array<IndustrialYardLookTemplate::Piece, IndustrialYardLookTemplate::PieceCount>
    IndustrialYardLookTemplate::GetPieces()
    {
        return {{
            { "wet_concrete_deck", "deck", AZ::Vector3(0.0f, 0.0f, 0.005f),
                AZ::Vector3(24.0f, 24.0f, 0.01f), Anchor::FloorSurface },
            { "north_brick_facade", "wall", AZ::Vector3(0.0f, 12.0f, 2.0f),
                AZ::Vector3(23.95f, 0.45f, 3.95f), Anchor::NorthWall },
            { "south_concrete_facade", "wall", AZ::Vector3(0.0f, -12.0f, 2.0f),
                AZ::Vector3(23.95f, 0.45f, 3.95f), Anchor::SouthWall },
            { "east_steel_facade", "wall", AZ::Vector3(12.0f, 0.0f, 2.0f),
                AZ::Vector3(0.45f, 23.95f, 3.95f), Anchor::EastWall },
            { "west_steel_facade_left", "wall", AZ::Vector3(-12.0f, -6.75f, 2.0f),
                AZ::Vector3(0.45f, 10.5f, 3.95f), Anchor::WestWall },
            { "west_steel_facade_right", "wall", AZ::Vector3(-12.0f, 6.75f, 2.0f),
                AZ::Vector3(0.45f, 10.5f, 3.95f), Anchor::WestWall },
            { "left_cover_cladding", "cover", AZ::Vector3(-2.25f, 0.0f, 1.25f),
                AZ::Vector3(1.50f, 2.00f, 2.50f), Anchor::LeftCover },
            { "right_cover_cladding", "cover", AZ::Vector3(2.25f, 0.0f, 1.25f),
                AZ::Vector3(1.50f, 2.00f, 2.50f), Anchor::RightCover },
            { "service_step_skin", "cover", AZ::Vector3(5.0f, -2.0f, 0.125f),
                AZ::Vector3(2.0f, 2.0f, 0.25f), Anchor::Step },
            { "suspended_gallery", "arch", AZ::Vector3(0.0f, 6.0f, 4.45f),
                AZ::Vector3(8.0f, 1.0f, 0.50f), Anchor::AboveArena },
            { "west_signage", "props", AZ::Vector3(-12.70f, 2.0f, 2.2f),
                AZ::Vector3(0.50f, 2.0f, 1.5f), Anchor::BeyondWestWall },
            { "shallow_puddles", "mark", AZ::Vector3(0.0f, -6.0f, 0.012f),
                AZ::Vector3(5.0f, 2.0f, 0.004f), Anchor::FloorSurface },
            { "roof_service_truss", "struct", AZ::Vector3(0.0f, 2.0f, 4.45f),
                AZ::Vector3(10.0f, 0.6f, 0.50f), Anchor::AboveArena },
            { "outside_factory_tower", "beacon", AZ::Vector3(-13.0f, 5.0f, 2.5f),
                AZ::Vector3(0.8f, 1.5f, 3.0f), Anchor::BeyondWestWall },
            { "flush_hazard_trim", "trim", AZ::Vector3(6.0f, -6.0f, 0.0115f),
                AZ::Vector3(4.0f, 2.0f, 0.003f), Anchor::FloorSurface },
            // West Annex: first enterable, multi-storey building, entered
            // through the west wall doorway above. Its east-facing pieces
            // sit flush with the doorway plane (x=-12) for a seamless
            // connection, which BeyondWestWall's 0.25 m clearance rule
            // can't accept - hence the dedicated Anchor::WestAnnex envelope
            // check below. Collision lives in PhysXArenaRuntime; these
            // entries mirror tools/blender/generate_industrial_yard.py's
            // ANNEX_PIECES/ANNEX_RAMP, same numbers, independently authored.
            { "annex_ground_floor", "deck", AZ::Vector3(-16.0f, 0.0f, -0.05f),
                AZ::Vector3(8.0f, 8.0f, 0.1f), Anchor::WestAnnex },
            { "annex_north_wall", "wall", AZ::Vector3(-16.0f, 4.0f, 1.75f),
                AZ::Vector3(8.0f, 0.3f, 3.5f), Anchor::WestAnnex },
            { "annex_south_wall", "wall", AZ::Vector3(-16.0f, -4.0f, 1.75f),
                AZ::Vector3(8.0f, 0.3f, 3.5f), Anchor::WestAnnex },
            { "annex_far_wall", "wall", AZ::Vector3(-20.0f, 0.0f, 1.75f),
                AZ::Vector3(0.3f, 8.0f, 3.5f), Anchor::WestAnnex },
            { "annex_east_wall_north", "wall", AZ::Vector3(-12.0f, 2.75f, 1.75f),
                AZ::Vector3(0.3f, 2.5f, 3.5f), Anchor::WestAnnex },
            { "annex_east_wall_south", "wall", AZ::Vector3(-12.0f, -2.75f, 1.75f),
                AZ::Vector3(0.3f, 2.5f, 3.5f), Anchor::WestAnnex },
            // Real (not hand-estimated) AABB of the rotated ramp mesh, read
            // back from Blender's evaluated bound_box at generation time.
            { "annex_ramp", "struct", AZ::Vector3(-16.0f, 0.0f, 1.75f),
                AZ::Vector3(6.1f, 2.5f, 3.575f), Anchor::WestAnnex },
            { "annex_upper_floor_north", "deck", AZ::Vector3(-16.0f, 2.625f, 3.5f),
                AZ::Vector3(6.0f, 2.75f, 0.15f), Anchor::WestAnnex },
            { "annex_upper_floor_south", "deck", AZ::Vector3(-16.0f, -2.625f, 3.5f),
                AZ::Vector3(6.0f, 2.75f, 0.15f), Anchor::WestAnnex }
        }};
    }

    AZStd::array<IndustrialYardLookTemplate::MaterialIntent, IndustrialYardLookTemplate::MaterialCount>
    IndustrialYardLookTemplate::GetMaterialIntents()
    {
        return {{
            { "weathered_concrete", 0.82f, 0.38f, 0.00f },
            { "painted_steel", 0.54f, 0.32f, 0.00f },
            { "aged_brick", 0.90f, 0.57f, 0.00f },
            { "shallow_water", 0.11f, 0.07f, 0.00f }
        }};
    }

    bool IndustrialYardLookTemplate::ValidatePiece(const Piece& piece)
    {
        if (!piece.m_name || !piece.m_name[0] || !piece.m_visualGroup || !piece.m_visualGroup[0]
            || !piece.m_center.IsFinite() || !piece.m_size.IsFinite()
            || piece.m_size.GetX() <= 0.0f || piece.m_size.GetY() <= 0.0f || piece.m_size.GetZ() <= 0.0f)
        {
            return false;
        }

        switch (piece.m_anchor)
        {
        case Anchor::FloorSurface:
            return piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= BoundsTolerance
                && piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= -12.0f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= 12.0f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= -12.0f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= 12.0f + BoundsTolerance;
        case Anchor::NorthWall:
            return WithinCollider(piece, AZ::Vector3(0.0f, 12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f));
        case Anchor::SouthWall:
            return WithinCollider(piece, AZ::Vector3(0.0f, -12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f));
        case Anchor::EastWall:
            return WithinCollider(piece, AZ::Vector3(12.0f, 0.0f, 2.0f), AZ::Vector3(0.5f, 24.0f, 4.0f));
        case Anchor::WestWall:
            return WithinCollider(piece, AZ::Vector3(-12.0f, 0.0f, 2.0f), AZ::Vector3(0.5f, 24.0f, 4.0f));
        case Anchor::LeftCover:
            return WithinCollider(piece, AZ::Vector3(-2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f));
        case Anchor::RightCover:
            return WithinCollider(piece, AZ::Vector3(2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f));
        case Anchor::Step:
            return WithinCollider(piece, AZ::Vector3(5.0f, -2.0f, 0.125f), AZ::Vector3(2.0f, 2.0f, 0.25f));
        case Anchor::AboveArena:
            return piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f
                    >= ArenaLayout::BoundsMaximum.GetZ() + 0.05f
                && piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= ArenaLayout::BoundsMinimum.GetX()
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= ArenaLayout::BoundsMaximum.GetX()
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= ArenaLayout::BoundsMinimum.GetY()
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= ArenaLayout::BoundsMaximum.GetY();
        case Anchor::BeyondWestWall:
            return piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= -12.25f + BoundsTolerance;
        case Anchor::WestAnnex:
            // Envelope of the whole annex building: flush with the doorway
            // plane on the east side (x=-12, no clearance requirement,
            // unlike BeyondWestWall), bounded on every other side by the
            // annex's actual authored footprint (8x8 m footprint centered at
            // x=-16, plus the upper-floor overhang and ramp).
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= -20.25f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= -11.80f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= -4.25f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= 4.25f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 4.45f + BoundsTolerance;
        }
        return false;
    }

    bool IndustrialYardLookTemplate::ValidateSceneContract()
    {
        if (!ArenaLayout::Validate())
        {
            return false;
        }
        for (const Piece& piece : GetPieces())
        {
            if (!ValidatePiece(piece))
            {
                return false;
            }
        }
        for (const MaterialIntent& material : GetMaterialIntents())
        {
            if (!ValidMaterial(material))
            {
                return false;
            }
        }
        return true;
    }
}
