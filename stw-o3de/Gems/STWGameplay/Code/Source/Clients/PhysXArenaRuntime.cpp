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

        const AZStd::array<PhysXArenaRuntime::StaticColliderDescription, PhysXArenaRuntime::StaticColliderCount>
            StaticColliderDescriptions = {{
                { "STW Floor", AZ::Vector3(0.0f, 0.0f, -0.5f), AZ::Vector3(24.0f, 24.0f, 1.0f) },
                { "STW North Wall", AZ::Vector3(0.0f, 12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f) },
                { "STW South Wall", AZ::Vector3(0.0f, -12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f) },
                { "STW East Wall", AZ::Vector3(12.0f, 0.0f, 2.0f), AZ::Vector3(0.5f, 24.0f, 4.0f) },
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
                { "STW Annex Upper Floor South", AZ::Vector3(-16.0f, -2.625f, 3.5f), AZ::Vector3(6.0f, 2.75f, 0.15f) }
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
