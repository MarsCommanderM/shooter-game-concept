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
            { "north_brick_facade_left", "wall", AZ::Vector3(-6.75f, 12.0f, 2.0f),
                AZ::Vector3(10.5f, 0.45f, 3.95f), Anchor::NorthWall },
            { "north_brick_facade_right", "wall", AZ::Vector3(6.75f, 12.0f, 2.0f),
                AZ::Vector3(10.5f, 0.45f, 3.95f), Anchor::NorthWall },
            { "south_concrete_facade_left", "wall", AZ::Vector3(-6.75f, -12.0f, 2.0f),
                AZ::Vector3(10.5f, 0.45f, 3.95f), Anchor::SouthWall },
            { "south_concrete_facade_right", "wall", AZ::Vector3(6.75f, -12.0f, 2.0f),
                AZ::Vector3(10.5f, 0.45f, 3.95f), Anchor::SouthWall },
            { "east_steel_facade_left", "wall", AZ::Vector3(12.0f, -6.75f, 2.0f),
                AZ::Vector3(0.45f, 10.5f, 3.95f), Anchor::EastWall },
            { "east_steel_facade_right", "wall", AZ::Vector3(12.0f, 6.75f, 2.0f),
                AZ::Vector3(0.45f, 10.5f, 3.95f), Anchor::EastWall },
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
                AZ::Vector3(6.0f, 2.75f, 0.15f), Anchor::WestAnnex },
            // North Scrapyard + crane: second landmark, deliberately not a
            // copy of the West Annex (open lattice, no walls, two switchback
            // ramps, a long cantilevered boom). Mirrors tools/blender/
            // generate_industrial_yard.py's SCRAPYARD_PIECES/CRANE_RAMPS.
            { "scrapyard_ground", "deck", AZ::Vector3(0.0f, 17.0f, -0.05f),
                AZ::Vector3(8.0f, 10.0f, 0.1f), Anchor::NorthScrapyard },
            { "scrapyard_cover_a", "cover", AZ::Vector3(-2.5f, 14.0f, 0.6f),
                AZ::Vector3(1.4f, 1.4f, 1.2f), Anchor::NorthScrapyard },
            { "scrapyard_cover_b", "cover", AZ::Vector3(2.5f, 15.5f, 0.75f),
                AZ::Vector3(1.6f, 1.6f, 1.5f), Anchor::NorthScrapyard },
            { "crane_landing", "struct", AZ::Vector3(-1.0f, 20.0f, 3.5f),
                AZ::Vector3(2.0f, 1.2f, 0.15f), Anchor::NorthScrapyard },
            { "crane_top_platform", "struct", AZ::Vector3(0.0f, 17.0f, 7.05f),
                AZ::Vector3(4.0f, 2.2f, 0.2f), Anchor::NorthScrapyard },
            { "crane_boom", "struct", AZ::Vector3(0.0f, 8.5f, 7.1f),
                AZ::Vector3(1.4f, 15.0f, 0.2f), Anchor::NorthScrapyard },
            // Real (not hand-estimated) AABBs of the rotated ramp meshes,
            // read back from Blender's evaluated bound_box.
            { "crane_ramp_1", "struct", AZ::Vector3(-1.5f, 18.0f, 1.775f),
                AZ::Vector3(1.8f, 5.114f, 3.615f), Anchor::NorthScrapyard },
            { "crane_ramp_2", "struct", AZ::Vector3(1.5f, 18.0f, 5.275f),
                AZ::Vector3(1.8f, 5.114f, 3.615f), Anchor::NorthScrapyard },
            // East Containerhof: third landmark, deliberately unlike the
            // first two (stacked container cover, no walls, no crane, one
            // elevated platform). Mirrors tools/blender/
            // generate_industrial_yard.py's EAST_CONTAINERHOF_PIECES/
            // CONTAINER_RAMP. Built as a real greybox pass, no high-detail
            // dressing, per the route-network-first map design guide.
            { "containerhof_ground", "deck", AZ::Vector3(16.0f, 0.0f, -0.05f),
                AZ::Vector3(8.0f, 12.0f, 0.1f), Anchor::EastContainerhof },
            { "container_low_a", "cover", AZ::Vector3(14.0f, -4.3f, 1.25f),
                AZ::Vector3(3.0f, 1.6f, 2.5f), Anchor::EastContainerhof },
            { "container_low_b", "cover", AZ::Vector3(14.0f, 0.0f, 1.25f),
                AZ::Vector3(3.0f, 1.6f, 2.5f), Anchor::EastContainerhof },
            { "container_low_c", "cover", AZ::Vector3(14.0f, 4.3f, 1.25f),
                AZ::Vector3(3.0f, 1.6f, 2.5f), Anchor::EastContainerhof },
            { "container_platform_support", "cover", AZ::Vector3(18.5f, 0.0f, 1.2f),
                AZ::Vector3(3.0f, 3.0f, 2.4f), Anchor::EastContainerhof },
            { "container_platform", "struct", AZ::Vector3(18.5f, 0.0f, 2.5f),
                AZ::Vector3(3.4f, 3.4f, 0.2f), Anchor::EastContainerhof },
            // Real (not hand-estimated) AABB of the rotated ramp mesh, read
            // back from Blender's evaluated bound_box at generation time.
            { "container_ramp", "struct", AZ::Vector3(17.0f, -2.0f, 1.225f),
                AZ::Vector3(3.123f, 1.8f, 2.508f), Anchor::EastContainerhof },
            // South Verladezone: fourth and final cardinal landmark - raised
            // loading dock, parked-trailer cover lanes, doorway crate
            // cluster. Mirrors tools/blender/generate_industrial_yard.py's
            // VERLADEZONE_PIECES/DOCK_RAMP. Completes the crossing route
            // network (west<->east, north<->south) per the route-network-
            // first map design guide.
            { "verladezone_ground", "deck", AZ::Vector3(0.0f, -17.0f, -0.05f),
                AZ::Vector3(8.0f, 10.0f, 0.1f), Anchor::SouthVerladezone },
            { "dock_platform", "struct", AZ::Vector3(0.0f, -20.5f, 0.6f),
                AZ::Vector3(5.0f, 2.5f, 1.2f), Anchor::SouthVerladezone },
            { "truck_trailer_a", "cover", AZ::Vector3(-2.6f, -15.0f, 1.1f),
                AZ::Vector3(1.8f, 4.5f, 2.2f), Anchor::SouthVerladezone },
            { "truck_trailer_b", "cover", AZ::Vector3(2.6f, -15.0f, 1.1f),
                AZ::Vector3(1.8f, 4.5f, 2.2f), Anchor::SouthVerladezone },
            { "loading_crates", "cover", AZ::Vector3(0.0f, -13.0f, 0.75f),
                AZ::Vector3(2.2f, 1.6f, 1.5f), Anchor::SouthVerladezone },
            // Real (not hand-estimated) AABB of the rotated ramp mesh, read
            // back from Blender's evaluated bound_box at generation time.
            { "dock_ramp", "struct", AZ::Vector3(0.0f, -18.75f, 0.625f),
                AZ::Vector3(2.5f, 2.584f, 1.332f), Anchor::SouthVerladezone },
            // NW connector: outdoor L-shaped walkway linking the West Annex
            // directly to the North Scrapyard, bypassing the central hof -
            // the first genuine crossing route beyond the four cardinal
            // landmarks. Mirrors tools/blender/generate_industrial_yard.py's
            // CONNECTOR_NW_PIECES. Flat, no ramp.
            { "connector_nw_ground_a", "deck", AZ::Vector3(-16.0f, 8.5f, -0.05f),
                AZ::Vector3(3.0f, 9.0f, 0.1f), Anchor::ConnectorNW },
            { "connector_nw_ground_b", "deck", AZ::Vector3(-10.0f, 13.0f, -0.05f),
                AZ::Vector3(12.0f, 3.0f, 0.1f), Anchor::ConnectorNW },
            { "connector_nw_cover_a", "cover", AZ::Vector3(-16.8f, 8.5f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorNW },
            { "connector_nw_cover_b", "cover", AZ::Vector3(-10.0f, 13.8f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorNW },
            // NE connector: mirrors the NW connector - links the North
            // Scrapyard directly to the East Containerhof.
            { "connector_ne_ground_a", "deck", AZ::Vector3(16.0f, 9.5f, -0.05f),
                AZ::Vector3(3.0f, 7.0f, 0.1f), Anchor::ConnectorNE },
            { "connector_ne_ground_b", "deck", AZ::Vector3(10.0f, 13.0f, -0.05f),
                AZ::Vector3(12.0f, 3.0f, 0.1f), Anchor::ConnectorNE },
            { "connector_ne_cover_a", "cover", AZ::Vector3(16.8f, 9.5f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorNE },
            { "connector_ne_cover_b", "cover", AZ::Vector3(10.0f, 13.8f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorNE },
            // SE connector: mirrors the NE connector across y=0 - links the
            // East Containerhof directly to the South Verladezone.
            { "connector_se_ground_a", "deck", AZ::Vector3(16.0f, -9.5f, -0.05f),
                AZ::Vector3(3.0f, 7.0f, 0.1f), Anchor::ConnectorSE },
            { "connector_se_ground_b", "deck", AZ::Vector3(10.0f, -13.0f, -0.05f),
                AZ::Vector3(12.0f, 3.0f, 0.1f), Anchor::ConnectorSE },
            { "connector_se_cover_a", "cover", AZ::Vector3(16.8f, -9.5f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorSE },
            { "connector_se_cover_b", "cover", AZ::Vector3(10.0f, -13.8f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorSE },
            // SW connector: mirrors the NW connector across y=0 - links the
            // West Annex directly to the South Verladezone, closing the
            // full loop of connectors around the yard.
            { "connector_sw_ground_a", "deck", AZ::Vector3(-16.0f, -8.5f, -0.05f),
                AZ::Vector3(3.0f, 9.0f, 0.1f), Anchor::ConnectorSW },
            { "connector_sw_ground_b", "deck", AZ::Vector3(-10.0f, -13.0f, -0.05f),
                AZ::Vector3(12.0f, 3.0f, 0.1f), Anchor::ConnectorSW },
            { "connector_sw_cover_a", "cover", AZ::Vector3(-16.8f, -8.5f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorSW },
            { "connector_sw_cover_b", "cover", AZ::Vector3(-10.0f, -13.8f, 0.75f),
                AZ::Vector3(1.2f, 1.2f, 1.5f), Anchor::ConnectorSW }
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
        case Anchor::NorthScrapyard:
            // Envelope of the whole scrapyard+crane structure: flush with
            // the doorway plane on the south side (y=12, no clearance
            // requirement), bounded by the crane's actual authored reach -
            // including the boom, which deliberately extends back over the
            // yard (y down to ~1) well above the 4 m wall height.
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= -4.5f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= 4.5f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= 0.5f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= 23.0f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 7.4f + BoundsTolerance;
        case Anchor::EastContainerhof:
            // Envelope of the whole containerhof: flush with the doorway
            // plane on the west side (x=12, no clearance requirement),
            // bounded by the actual authored footprint (8x12 m ground plane
            // plus the elevated platform, max height ~2.6 m - well short of
            // AboveArena's 4 m clearance requirement).
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= 11.80f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= 20.25f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= -6.25f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= 6.25f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 2.75f + BoundsTolerance;
        case Anchor::SouthVerladezone:
            // Envelope of the whole verladezone: flush with the doorway
            // plane on the north side (y=-12, no clearance requirement),
            // bounded by the actual authored footprint (8x10 m ground plane
            // plus the parked trailers, max height ~2.2 m).
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= -4.25f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= 4.25f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= -22.25f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= -11.80f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 2.35f + BoundsTolerance;
        case Anchor::ConnectorNW:
            // Envelope of the L-shaped walkway itself (both deck segments
            // plus the two sightline-break cover pieces), in the exterior
            // corner outside both the west and north wall lines.
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= -17.75f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= -3.75f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= 3.75f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= 14.75f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 1.75f + BoundsTolerance;
        case Anchor::ConnectorNE:
            // Mirrors ConnectorNW's envelope across x=0.
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= 3.75f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= 17.75f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= 3.75f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= 14.75f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 1.75f + BoundsTolerance;
        case Anchor::ConnectorSE:
            // Mirrors ConnectorNE's envelope across y=0.
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= 3.75f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= 17.75f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= -14.75f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= -3.75f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 1.75f + BoundsTolerance;
        case Anchor::ConnectorSW:
            // Mirrors ConnectorNW's envelope across y=0.
            return piece.m_center.GetX() - piece.m_size.GetX() * 0.5f >= -17.75f - BoundsTolerance
                && piece.m_center.GetX() + piece.m_size.GetX() * 0.5f <= -3.75f + BoundsTolerance
                && piece.m_center.GetY() - piece.m_size.GetY() * 0.5f >= -14.75f - BoundsTolerance
                && piece.m_center.GetY() + piece.m_size.GetY() * 0.5f <= -3.75f + BoundsTolerance
                && piece.m_center.GetZ() - piece.m_size.GetZ() * 0.5f >= -0.15f - BoundsTolerance
                && piece.m_center.GetZ() + piece.m_size.GetZ() * 0.5f <= 1.75f + BoundsTolerance;
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
