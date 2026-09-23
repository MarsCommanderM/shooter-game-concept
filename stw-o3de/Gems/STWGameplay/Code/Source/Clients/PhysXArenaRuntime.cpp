#include "PhysXArenaRuntime.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzFramework/Components/TransformComponent.h>
#include <Source/BoxColliderComponent.h>
#include <Source/StaticRigidBodyComponent.h>

namespace STWGameplay
{
    namespace
    {
        // The ramp is the only non-axis-aligned collider: its rotation is the
        // shortest arc from +X to the actual start->end climb direction, so the
        // box's local X (length) axis exactly follows the walkable slope -
        // computed, not hand-picked Euler angles. Endpoints/width/thickness must
        // match tools/blender/generate_industrial_yard.py's ANNEX_RAMP exactly;
        // the two are independent authorings of the same physical ramp (visual
        // mesh vs. collision), not one generated from the other.
        const AZ::Vector3 RampStart(-13.0f, 0.0f, 0.05f);
        const AZ::Vector3 RampEnd(-19.0f, 0.0f, 3.45f);
        const AZ::Vector3 RampDirection = RampEnd - RampStart;

        // North Scrapyard crane: two switchback ramps, same compute-don't-
        // guess rotation approach. Endpoints must match tools/blender/
        // generate_industrial_yard.py's CRANE_RAMPS exactly.
        const AZ::Vector3 CraneRamp1Start(-1.5f, 15.5f, 0.05f);
        const AZ::Vector3 CraneRamp1End(-1.5f, 20.5f, 3.5f);
        const AZ::Vector3 CraneRamp1Direction = CraneRamp1End - CraneRamp1Start;
        const AZ::Vector3 CraneRamp2Start(1.5f, 20.5f, 3.55f);
        const AZ::Vector3 CraneRamp2End(1.5f, 15.5f, 7.0f);
        const AZ::Vector3 CraneRamp2Direction = CraneRamp2End - CraneRamp2Start;

        // East Containerhof ramp: same compute-don't-guess rotation
        // approach. Endpoints must match tools/blender/
        // generate_industrial_yard.py's CONTAINER_RAMP exactly.
        const AZ::Vector3 ContainerRampStart(15.5f, -2.0f, 0.05f);
        const AZ::Vector3 ContainerRampEnd(18.5f, -2.0f, 2.4f);
        const AZ::Vector3 ContainerRampDirection = ContainerRampEnd - ContainerRampStart;

        // South Verladezone dock ramp: same compute-don't-guess rotation
        // approach. Endpoints must match tools/blender/
        // generate_industrial_yard.py's DOCK_RAMP exactly.
        const AZ::Vector3 DockRampStart(0.0f, -17.5f, 0.05f);
        const AZ::Vector3 DockRampEnd(0.0f, -20.0f, 1.2f);
        const AZ::Vector3 DockRampDirection = DockRampEnd - DockRampStart;

        const AZStd::array<PhysXArenaRuntime::StaticColliderDescription, PhysXArenaRuntime::StaticColliderCount>
            StaticColliderDescriptions = {{
                { "STW Floor", AZ::Vector3(0.0f, 0.0f, -0.5f), AZ::Vector3(24.0f, 24.0f, 1.0f) },
                // North wall is split around a 3 m doorway (x -1.5..1.5) into
                // the North Scrapyard/crane landmark instead of one solid facade.
                { "STW North Wall Left", AZ::Vector3(-6.75f, 12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f) },
                { "STW North Wall Right", AZ::Vector3(6.75f, 12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f) },
                // South wall is split around a 3 m doorway (x -1.5..1.5) into
                // the South Verladezone instead of one solid facade.
                { "STW South Wall Left", AZ::Vector3(-6.75f, -12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f) },
                { "STW South Wall Right", AZ::Vector3(6.75f, -12.0f, 2.0f), AZ::Vector3(10.5f, 0.5f, 4.0f) },
                // East wall is split around a 3 m doorway (y -1.5..1.5) into
                // the East Containerhof instead of one solid facade.
                { "STW East Wall Left", AZ::Vector3(12.0f, -6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f) },
                { "STW East Wall Right", AZ::Vector3(12.0f, 6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f) },
                // West wall is split around a 3 m doorway (y -1.5..1.5) into the
                // West Annex building instead of one solid facade.
                { "STW West Wall Left", AZ::Vector3(-12.0f, -6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f) },
                { "STW West Wall Right", AZ::Vector3(-12.0f, 6.75f, 2.0f), AZ::Vector3(0.5f, 10.5f, 4.0f) },
                { "STW Left Cover", AZ::Vector3(-2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f) },
                { "STW Right Cover", AZ::Vector3(2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f) },
                { "STW Step", AZ::Vector3(5.0f, -2.0f, 0.125f), AZ::Vector3(2.0f, 2.0f, 0.25f) },
                // West Annex: first enterable, multi-storey building. Ground
                // floor entered through the west wall doorway; a real ramp
                // (not a lift/teleport) climbs to a genuine upper floor, open
                // above the 3.5 m annex wall height on every side as an
                // unglazed window/balcony over the arena.
                { "STW Annex Ground Floor", AZ::Vector3(-16.0f, 0.0f, -0.05f), AZ::Vector3(8.0f, 8.0f, 0.1f) },
                { "STW Annex North Wall", AZ::Vector3(-16.0f, 4.0f, 1.75f), AZ::Vector3(8.0f, 0.3f, 3.5f) },
                { "STW Annex South Wall", AZ::Vector3(-16.0f, -4.0f, 1.75f), AZ::Vector3(8.0f, 0.3f, 3.5f) },
                { "STW Annex Far Wall", AZ::Vector3(-20.0f, 0.0f, 1.75f), AZ::Vector3(0.3f, 8.0f, 3.5f) },
                { "STW Annex East Wall North", AZ::Vector3(-12.0f, 2.75f, 1.75f), AZ::Vector3(0.3f, 2.5f, 3.5f) },
                { "STW Annex East Wall South", AZ::Vector3(-12.0f, -2.75f, 1.75f), AZ::Vector3(0.3f, 2.5f, 3.5f) },
                { "STW Annex Ramp", (RampStart + RampEnd) * 0.5f,
                    AZ::Vector3(RampDirection.GetLength(), 2.5f, 0.2f),
                    AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisX(), RampDirection.GetNormalized()) },
                { "STW Annex Upper Floor North", AZ::Vector3(-16.0f, 2.625f, 3.5f), AZ::Vector3(6.0f, 2.75f, 0.15f) },
                { "STW Annex Upper Floor South", AZ::Vector3(-16.0f, -2.625f, 3.5f), AZ::Vector3(6.0f, 2.75f, 0.15f) },
                // North Scrapyard + crane: open steel-lattice tower (no
                // walls, unlike the West Annex), short crate cover at
                // ground level, two switchback ramps up to a platform and
                // a long cantilevered sniper boom over the whole arena.
                { "STW Scrapyard Ground", AZ::Vector3(0.0f, 17.0f, -0.05f), AZ::Vector3(8.0f, 10.0f, 0.1f) },
                { "STW Scrapyard Cover A", AZ::Vector3(-2.5f, 14.0f, 0.6f), AZ::Vector3(1.4f, 1.4f, 1.2f) },
                { "STW Scrapyard Cover B", AZ::Vector3(2.5f, 15.5f, 0.75f), AZ::Vector3(1.6f, 1.6f, 1.5f) },
                { "STW Crane Ramp 1", (CraneRamp1Start + CraneRamp1End) * 0.5f,
                    AZ::Vector3(CraneRamp1Direction.GetLength(), 1.8f, 0.2f),
                    AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisX(), CraneRamp1Direction.GetNormalized()) },
                { "STW Crane Landing", AZ::Vector3(-1.0f, 20.0f, 3.5f), AZ::Vector3(2.0f, 1.2f, 0.15f) },
                { "STW Crane Ramp 2", (CraneRamp2Start + CraneRamp2End) * 0.5f,
                    AZ::Vector3(CraneRamp2Direction.GetLength(), 1.8f, 0.2f),
                    AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisX(), CraneRamp2Direction.GetNormalized()) },
                { "STW Crane Top Platform", AZ::Vector3(0.0f, 17.0f, 7.05f), AZ::Vector3(4.0f, 2.2f, 0.2f) },
                { "STW Crane Boom", AZ::Vector3(0.0f, 8.5f, 7.1f), AZ::Vector3(1.4f, 15.0f, 0.2f) },
                // East Containerhof: third landmark, deliberately unlike the
                // first two - stacked container cover, no walls, no crane,
                // one elevated platform reached by a ramp.
                { "STW Containerhof Ground", AZ::Vector3(16.0f, 0.0f, -0.05f), AZ::Vector3(8.0f, 12.0f, 0.1f) },
                { "STW Container Low A", AZ::Vector3(14.0f, -4.3f, 1.25f), AZ::Vector3(3.0f, 1.6f, 2.5f) },
                { "STW Container Low B", AZ::Vector3(14.0f, 0.0f, 1.25f), AZ::Vector3(3.0f, 1.6f, 2.5f) },
                { "STW Container Low C", AZ::Vector3(14.0f, 4.3f, 1.25f), AZ::Vector3(3.0f, 1.6f, 2.5f) },
                { "STW Container Platform Support", AZ::Vector3(18.5f, 0.0f, 1.2f), AZ::Vector3(3.0f, 3.0f, 2.4f) },
                { "STW Container Ramp", (ContainerRampStart + ContainerRampEnd) * 0.5f,
                    AZ::Vector3(ContainerRampDirection.GetLength(), 1.8f, 0.2f),
                    AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisX(), ContainerRampDirection.GetNormalized()) },
                { "STW Container Platform", AZ::Vector3(18.5f, 0.0f, 2.5f), AZ::Vector3(3.4f, 3.4f, 0.2f) },
                // South Verladezone: fourth and final cardinal landmark -
                // raised loading dock platform, two parked-trailer cover
                // lanes, small crate cluster at the doorway.
                { "STW Verladezone Ground", AZ::Vector3(0.0f, -17.0f, -0.05f), AZ::Vector3(8.0f, 10.0f, 0.1f) },
                { "STW Dock Platform", AZ::Vector3(0.0f, -20.5f, 0.6f), AZ::Vector3(5.0f, 2.5f, 1.2f) },
                { "STW Truck Trailer A", AZ::Vector3(-2.6f, -15.0f, 1.1f), AZ::Vector3(1.8f, 4.5f, 2.2f) },
                { "STW Truck Trailer B", AZ::Vector3(2.6f, -15.0f, 1.1f), AZ::Vector3(1.8f, 4.5f, 2.2f) },
                { "STW Loading Crates", AZ::Vector3(0.0f, -13.0f, 0.75f), AZ::Vector3(2.2f, 1.6f, 1.5f) },
                { "STW Dock Ramp", (DockRampStart + DockRampEnd) * 0.5f,
                    AZ::Vector3(DockRampDirection.GetLength(), 2.5f, 0.2f),
                    AZ::Quaternion::CreateShortestArc(AZ::Vector3::CreateAxisX(), DockRampDirection.GetNormalized()) },
                // NW connector: outdoor L-shaped walkway linking the West
                // Annex directly to the North Scrapyard, bypassing the
                // central hof. Flat, axis-aligned, no ramp.
                { "STW Connector NW Ground A", AZ::Vector3(-16.0f, 8.5f, -0.05f), AZ::Vector3(3.0f, 9.0f, 0.1f) },
                { "STW Connector NW Ground B", AZ::Vector3(-10.0f, 13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f) },
                { "STW Connector NW Cover A", AZ::Vector3(-16.8f, 8.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                { "STW Connector NW Cover B", AZ::Vector3(-10.0f, 13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                // Lane-break chicane: at a different x-slice than Cover B,
                // leaving a real ~1.2 m gap on the north side of the lane.
                { "STW Connector NW Lane Break", AZ::Vector3(-13.0f, 12.4f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f) },
                // NE connector: outdoor L-shaped walkway linking the North
                // Scrapyard directly to the East Containerhof, bypassing
                // the central hof. Flat, axis-aligned, no ramp.
                { "STW Connector NE Ground A", AZ::Vector3(16.0f, 9.5f, -0.05f), AZ::Vector3(3.0f, 7.0f, 0.1f) },
                { "STW Connector NE Ground B", AZ::Vector3(10.0f, 13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f) },
                { "STW Connector NE Cover A", AZ::Vector3(16.8f, 9.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                { "STW Connector NE Cover B", AZ::Vector3(10.0f, 13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                // Gap on the south side this time, for route variety.
                { "STW Connector NE Lane Break", AZ::Vector3(13.0f, 13.6f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f) },
                // SE connector: outdoor L-shaped walkway linking the East
                // Containerhof directly to the South Verladezone, bypassing
                // the central hof. Flat, axis-aligned, no ramp.
                { "STW Connector SE Ground A", AZ::Vector3(16.0f, -9.5f, -0.05f), AZ::Vector3(3.0f, 7.0f, 0.1f) },
                { "STW Connector SE Ground B", AZ::Vector3(10.0f, -13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f) },
                { "STW Connector SE Cover A", AZ::Vector3(16.8f, -9.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                { "STW Connector SE Cover B", AZ::Vector3(10.0f, -13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                { "STW Connector SE Lane Break", AZ::Vector3(13.0f, -13.6f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f) },
                // SW connector: outdoor L-shaped walkway linking the West
                // Annex directly to the South Verladezone, bypassing the
                // central hof. Closes the full loop of connectors around
                // the yard. Flat, axis-aligned, no ramp.
                { "STW Connector SW Ground A", AZ::Vector3(-16.0f, -8.5f, -0.05f), AZ::Vector3(3.0f, 9.0f, 0.1f) },
                { "STW Connector SW Ground B", AZ::Vector3(-10.0f, -13.0f, -0.05f), AZ::Vector3(12.0f, 3.0f, 0.1f) },
                { "STW Connector SW Cover A", AZ::Vector3(-16.8f, -8.5f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                { "STW Connector SW Cover B", AZ::Vector3(-10.0f, -13.8f, 0.75f), AZ::Vector3(1.2f, 1.2f, 1.5f) },
                { "STW Connector SW Lane Break", AZ::Vector3(-13.0f, -12.4f, 0.8f), AZ::Vector3(0.6f, 1.8f, 1.6f) },
                // Central hof height variation + sightline breaks: four
                // two-tier crate clusters, one per quadrant, breaking the
                // open diagonals between adjacent cardinal doorways and
                // giving real verticality inside the hof itself.
                { "STW Central Cover NE Low", AZ::Vector3(7.0f, 7.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f) },
                { "STW Central Cover NE High", AZ::Vector3(7.9f, 7.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f) },
                { "STW Central Cover NW Low", AZ::Vector3(-7.0f, 7.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f) },
                { "STW Central Cover NW High", AZ::Vector3(-7.9f, 7.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f) },
                { "STW Central Cover SE Low", AZ::Vector3(7.0f, -8.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f) },
                { "STW Central Cover SE High", AZ::Vector3(7.9f, -8.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f) },
                { "STW Central Cover SW Low", AZ::Vector3(-7.0f, -8.0f, 0.6f), AZ::Vector3(1.6f, 1.6f, 1.2f) },
                { "STW Central Cover SW High", AZ::Vector3(-7.9f, -8.9f, 0.9f), AZ::Vector3(1.4f, 1.4f, 1.8f) }
            }};

        void DeactivateArenaEntity(AZStd::unique_ptr<AZ::Entity>& entity)
        {
            if (entity && entity->GetState() == AZ::Entity::State::Active)
            {
                entity->Deactivate();
            }
            entity.reset();
        }
    }

    PhysXArenaRuntime::~PhysXArenaRuntime()
    {
        Shutdown();
    }

    bool PhysXArenaRuntime::Initialize()
    {
        Shutdown();
        for (const StaticColliderDescription& description : GetStaticColliderDescriptions())
        {
            if (!CreateStaticBox(description))
            {
                Shutdown();
                return false;
            }
        }
        if (!IsValid())
        {
            Shutdown();
            return false;
        }
        return true;
    }

    void PhysXArenaRuntime::Shutdown()
    {
        for (auto iterator = m_staticColliderEntities.rbegin(); iterator != m_staticColliderEntities.rend(); ++iterator)
        {
            DeactivateArenaEntity(*iterator);
        }
        m_staticColliderEntities.clear();
    }

    bool PhysXArenaRuntime::IsValid() const
    {
        return m_staticColliderEntities.size() == StaticColliderCount
            && AZStd::all_of(
                m_staticColliderEntities.begin(), m_staticColliderEntities.end(),
                [](const AZStd::unique_ptr<AZ::Entity>& entity)
                {
                    return entity && entity->GetState() == AZ::Entity::State::Active;
                });
    }

    const AZStd::array<PhysXArenaRuntime::StaticColliderDescription, PhysXArenaRuntime::StaticColliderCount>&
    PhysXArenaRuntime::GetStaticColliderDescriptions()
    {
        return StaticColliderDescriptions;
    }

    bool PhysXArenaRuntime::CreateStaticBox(const StaticColliderDescription& description)
    {
        if (description.m_name == nullptr || !description.m_center.IsFinite()
            || !description.m_dimensions.IsFinite() || description.m_dimensions.GetMinElement() <= 0.0f
            || !description.m_rotation.IsFinite())
        {
            return false;
        }

        auto entity = AZStd::make_unique<AZ::Entity>(description.m_name);
        auto* transform = entity->CreateComponent<AzFramework::TransformComponent>();
        if (transform == nullptr)
        {
            return false;
        }
        transform->SetWorldTM(
            AZ::Transform::CreateFromQuaternionAndTranslation(description.m_rotation, description.m_center));

        auto* collider = entity->CreateComponent<PhysX::BoxColliderComponent>();
        if (collider == nullptr)
        {
            return false;
        }
        collider->SetShapeConfigurationList({ AZStd::make_pair(
            AZStd::make_shared<Physics::ColliderConfiguration>(),
            AZStd::make_shared<Physics::BoxShapeConfiguration>(description.m_dimensions)) });
        if (entity->CreateComponent<PhysX::StaticRigidBodyComponent>() == nullptr)
        {
            return false;
        }
        entity->Init();
        entity->Activate();
        if (entity->GetState() != AZ::Entity::State::Active)
        {
            return false;
        }
        m_staticColliderEntities.push_back(AZStd::move(entity));
        return true;
    }
}
