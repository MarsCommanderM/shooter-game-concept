#include "PhysXPlayerRuntime.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Physics/CharacterBus.h>
#include <AzFramework/Physics/Collision/CollisionGroups.h>
#include <AzFramework/Physics/ShapeConfiguration.h>
#include <PhysX/CharacterGameplayBus.h>
#include <PhysXCharacters/Components/CharacterControllerComponent.h>
#include <PhysXCharacters/Components/CharacterGameplayComponent.h>
#include <Source/BoxColliderComponent.h>
#include <Source/StaticRigidBodyComponent.h>

namespace STWGameplay
{
    namespace
    {
        const AZ::Vector3 PlayerSpawn(0.0f, -6.0f, 0.15f);

        void DeactivateEntity(AZStd::unique_ptr<AZ::Entity>& entity)
        {
            if (entity && entity->GetState() == AZ::Entity::State::Active)
            {
                entity->Deactivate();
            }
            entity.reset();
        }
    }

    PhysXPlayerRuntime::~PhysXPlayerRuntime()
    {
        Shutdown();
    }

    bool PhysXPlayerRuntime::Initialize()
    {
        Shutdown();

        // Minimal collision course: floor, perimeter, two covers and a valid step.
        if (!CreateStaticBox("STW Floor", AZ::Vector3(0.0f, 0.0f, -0.5f), AZ::Vector3(24.0f, 24.0f, 1.0f))
            || !CreateStaticBox("STW North Wall", AZ::Vector3(0.0f, 12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f))
            || !CreateStaticBox("STW South Wall", AZ::Vector3(0.0f, -12.0f, 2.0f), AZ::Vector3(24.0f, 0.5f, 4.0f))
            || !CreateStaticBox("STW East Wall", AZ::Vector3(12.0f, 0.0f, 2.0f), AZ::Vector3(0.5f, 24.0f, 4.0f))
            || !CreateStaticBox("STW West Wall", AZ::Vector3(-12.0f, 0.0f, 2.0f), AZ::Vector3(0.5f, 24.0f, 4.0f))
            || !CreateStaticBox("STW Left Cover", AZ::Vector3(-2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f))
            || !CreateStaticBox("STW Right Cover", AZ::Vector3(2.25f, 0.0f, 1.25f), AZ::Vector3(1.5f, 2.0f, 2.5f))
            || !CreateStaticBox("STW Step", AZ::Vector3(5.0f, -2.0f, 0.125f), AZ::Vector3(2.0f, 2.0f, 0.25f)))
        {
            Shutdown();
            return false;
        }

        m_playerEntity = AZStd::make_unique<AZ::Entity>("STW PhysX Player");
        auto* transform = m_playerEntity->CreateComponent<AzFramework::TransformComponent>();
        transform->SetWorldTM(AZ::Transform::CreateTranslation(PlayerSpawn));

        auto characterConfiguration = AZStd::make_unique<Physics::CharacterConfiguration>();
        characterConfiguration->m_maximumSlopeAngle = SlopeLimitDegrees;
        characterConfiguration->m_stepHeight = StepHeight;
        characterConfiguration->m_minimumMovementDistance = 0.001f;
        characterConfiguration->m_maximumSpeed = MaximumControllerSpeed;
        characterConfiguration->m_applyMoveOnPhysicsTick = true;
        auto capsuleConfiguration = AZStd::make_shared<Physics::CapsuleShapeConfiguration>(CapsuleHeight, CapsuleRadius);
        m_playerEntity->CreateComponent<PhysX::CharacterControllerComponent>(
            AZStd::move(characterConfiguration), AZStd::move(capsuleConfiguration));

        PhysX::CharacterGameplayConfiguration gameplayConfiguration;
        gameplayConfiguration.m_gravityMultiplier = 1.0f;
        gameplayConfiguration.m_groundDetectionBoxHeight = GroundProbeHeight;
        m_playerEntity->CreateComponent<PhysX::CharacterGameplayComponent>(gameplayConfiguration);
        m_playerEntity->Init();
        m_playerEntity->Activate();

        if (!IsValid())
        {
            AZ_Error("STWGameplay", false, "PhysX Character Controller failed to initialize");
            Shutdown();
            return false;
        }
        return true;
    }

    void PhysXPlayerRuntime::Shutdown()
    {
        DeactivateEntity(m_playerEntity);
        for (auto iterator = m_environmentEntities.rbegin(); iterator != m_environmentEntities.rend(); ++iterator)
        {
            DeactivateEntity(*iterator);
        }
        m_environmentEntities.clear();
    }

    bool PhysXPlayerRuntime::QueueVelocity(const AZ::Vector3& velocity)
    {
        if (!IsValid() || !velocity.IsFinite())
        {
            return false;
        }
        Physics::CharacterRequestBus::Event(
            m_playerEntity->GetId(), &Physics::CharacterRequests::AddVelocityForTick, velocity);
        return true;
    }

    bool PhysXPlayerRuntime::Synchronize(AZ::Vector3& position, bool& grounded) const
    {
        if (!IsValid())
        {
            return false;
        }
        Physics::CharacterRequestBus::EventResult(
            position, m_playerEntity->GetId(), &Physics::CharacterRequests::GetBasePosition);
        PhysX::CharacterGameplayRequestBus::EventResult(
            grounded, m_playerEntity->GetId(), &PhysX::CharacterGameplayRequests::IsOnGround);
        return position.IsFinite();
    }

    bool PhysXPlayerRuntime::IsValid() const
    {
        if (!m_playerEntity || m_playerEntity->GetState() != AZ::Entity::State::Active)
        {
            return false;
        }
        bool present = false;
        Physics::CharacterRequestBus::EventResult(
            present, m_playerEntity->GetId(), &Physics::CharacterRequests::IsPresent);
        return present;
    }

    bool PhysXPlayerRuntime::CreateStaticBox(
        const char* name, const AZ::Vector3& center, const AZ::Vector3& dimensions)
    {
        if (!center.IsFinite() || !dimensions.IsFinite() || dimensions.GetMinElement() <= 0.0f)
        {
            return false;
        }
        auto entity = AZStd::make_unique<AZ::Entity>(name);
        auto* transform = entity->CreateComponent<AzFramework::TransformComponent>();
        transform->SetWorldTM(AZ::Transform::CreateTranslation(center));

        auto* collider = entity->CreateComponent<PhysX::BoxColliderComponent>();
        collider->SetShapeConfigurationList({ AZStd::make_pair(
            AZStd::make_shared<Physics::ColliderConfiguration>(),
            AZStd::make_shared<Physics::BoxShapeConfiguration>(dimensions)) });
        entity->CreateComponent<PhysX::StaticRigidBodyComponent>();
        entity->Init();
        entity->Activate();
        m_environmentEntities.push_back(AZStd::move(entity));
        return true;
    }
}
