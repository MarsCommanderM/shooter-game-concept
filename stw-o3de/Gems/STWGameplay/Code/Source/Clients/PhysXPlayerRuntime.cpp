#include "PhysXPlayerRuntime.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Physics/CharacterBus.h>
#include <AzFramework/Physics/Collision/CollisionGroups.h>
#include <AzFramework/Physics/Common/PhysicsSceneQueries.h>
#include <AzFramework/Physics/PhysicsScene.h>
#include <AzFramework/Physics/ShapeConfiguration.h>
#include <PhysX/CharacterControllerBus.h>
#include <PhysX/CharacterGameplayBus.h>
#include <PhysXCharacters/Components/CharacterControllerComponent.h>
#include <PhysXCharacters/Components/CharacterGameplayComponent.h>
#include <Source/BoxColliderComponent.h>
#include <Source/StaticRigidBodyComponent.h>
#include <STWGameplay/ArenaLayout.h>

namespace STWGameplay
{
    namespace
    {
        constexpr float StandingClearanceLift = 0.01f;

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
        m_crouched = false;
        m_pendingJumpSpeed = 0.0f;

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
        transform->SetWorldTM(AZ::Transform::CreateTranslation(ArenaLayout::PlayerSpawn));

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
        m_crouched = false;
        m_pendingJumpSpeed = 0.0f;
    }

    bool PhysXPlayerRuntime::QueueVelocity(const AZ::Vector3& velocity)
    {
        if (!IsValid() || !velocity.IsFinite())
        {
            return false;
        }
        if (velocity.GetZ() > 0.0f)
        {
            // AddVelocityForTick provides the collision-resolved takeoff step. Once PhysX
            // reports that step made the controller airborne, Synchronize seeds the existing
            // CharacterGameplayComponent falling velocity so gravity owns the remaining arc.
            m_pendingJumpSpeed = velocity.GetZ();
        }
        Physics::CharacterRequestBus::Event(
            m_playerEntity->GetId(), &Physics::CharacterRequests::AddVelocityForTick, velocity);
        return true;
    }

    bool PhysXPlayerRuntime::ApplyCrouchRequest(bool crouchDesired, bool grounded)
    {
        if (!IsValid() || !grounded || crouchDesired == m_crouched)
        {
            return IsValid();
        }
        if (!crouchDesired && !CanRestoreStandingHeight())
        {
            return true;
        }

        const float requestedHeight = crouchDesired ? CrouchedCapsuleHeight : CapsuleHeight;
        PhysX::CharacterControllerRequestBus::Event(
            m_playerEntity->GetId(), &PhysX::CharacterControllerRequests::Resize, requestedHeight);
        const float appliedHeight = GetControllerHeight();
        if (!AZ::IsClose(appliedHeight, requestedHeight, 0.001f))
        {
            return false;
        }
        m_crouched = crouchDesired;
        return true;
    }

    float PhysXPlayerRuntime::GetControllerHeight() const
    {
        float height = 0.0f;
        if (IsValid())
        {
            PhysX::CharacterControllerRequestBus::EventResult(
                height, m_playerEntity->GetId(), &PhysX::CharacterControllerRequests::GetHeight);
        }
        return height;
    }

    float PhysXPlayerRuntime::GetEyeHeight() const
    {
        return AZStd::max(0.0f, GetControllerHeight() - 0.10f);
    }

    bool PhysXPlayerRuntime::CanRestoreStandingHeight() const
    {
        Physics::Character* character = nullptr;
        Physics::CharacterRequestBus::EventResult(
            character, m_playerEntity->GetId(), &Physics::CharacterRequests::GetCharacter);
        if (!character || !character->GetScene())
        {
            return false;
        }

        const AZ::Vector3 basePosition = character->GetBasePosition();
        const AZ::Transform pose = AZ::Transform::CreateTranslation(
            basePosition + AZ::Vector3::CreateAxisZ(0.5f * CapsuleHeight + StandingClearanceLift));
        AzPhysics::OverlapRequest request = AzPhysics::OverlapRequestHelpers::CreateCapsuleOverlapRequest(
            CapsuleHeight, CapsuleRadius, pose);
        request.m_collisionGroup = character->GetCollisionGroup();
        const AZ::EntityId playerEntityId = m_playerEntity->GetId();
        const AzPhysics::SceneQueryHits hits = character->GetScene()->QueryScene(&request);
        return AZStd::none_of(
            hits.m_hits.begin(), hits.m_hits.end(),
            [&playerEntityId](const AzPhysics::SceneQueryHit& hit)
            {
                return hit.m_entityId != playerEntityId;
            });
    }

    bool PhysXPlayerRuntime::ResetPosition(const AZ::Vector3& position)
    {
        if (!IsValid() || !position.IsFinite())
        {
            return false;
        }
        Physics::CharacterRequestBus::Event(
            m_playerEntity->GetId(), &Physics::CharacterRequests::SetBasePosition, position);
        return true;
    }

    bool PhysXPlayerRuntime::Synchronize(AZ::Vector3& position, bool& grounded)
    {
        if (!IsValid())
        {
            return false;
        }
        Physics::CharacterRequestBus::EventResult(
            position, m_playerEntity->GetId(), &Physics::CharacterRequests::GetBasePosition);
        PhysX::CharacterGameplayRequestBus::EventResult(
            grounded, m_playerEntity->GetId(), &PhysX::CharacterGameplayRequests::IsOnGround);
        if (!grounded && m_pendingJumpSpeed > 0.0f)
        {
            PhysX::CharacterGameplayRequestBus::Event(
                m_playerEntity->GetId(), &PhysX::CharacterGameplayRequests::SetFallingVelocity,
                AZ::Vector3::CreateAxisZ(m_pendingJumpSpeed));
            m_pendingJumpSpeed = 0.0f;
        }
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
