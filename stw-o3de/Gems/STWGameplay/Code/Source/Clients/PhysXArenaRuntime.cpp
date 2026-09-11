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
        const AZStd::array<PhysXArenaRuntime::StaticColliderDescription, PhysXArenaRuntime::StaticColliderCount>
            StaticColliderDescriptions = {{
                { "STW Floor", AZ::Vector3(0.0f, 0.0f, -0.5f), AZ::Vector3(24.0f, 24.0f, 1.0f) },
                { "STW North Wall", AZ::Vector3(0.0f, 12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f) },
                { "STW South Wall", AZ::Vector3(0.0f, -12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f) },
                { "STW East Wall", AZ::Vector3(12.0f, 0.0f, 2.0f), AZ::Vector3(0.5f, 24.0f, 4.0f) },
                { "STW West Wall", AZ::Vector3(-12.0f, 0.0f, 2.0f), AZ::Vector3(0.5f, 24.0f, 4.0f) },
                { "STW Left Cover", AZ::Vector3(-2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f) },
                { "STW Right Cover", AZ::Vector3(2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f) },
                { "STW Step", AZ::Vector3(5.0f, -2.0f, 0.125f), AZ::Vector3(2.0f, 2.0f, 0.25f) }
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
            || !description.m_dimensions.IsFinite() || description.m_dimensions.GetMinElement() <= 0.0f)
        {
            return false;
        }

        auto entity = AZStd::make_unique<AZ::Entity>(description.m_name);
        auto* transform = entity->CreateComponent<AzFramework::TransformComponent>();
        if (transform == nullptr)
        {
            return false;
        }
        transform->SetWorldTM(AZ::Transform::CreateTranslation(description.m_center));

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
