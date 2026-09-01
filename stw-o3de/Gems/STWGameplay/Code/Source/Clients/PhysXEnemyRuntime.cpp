#include "PhysXEnemyRuntime.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Physics/CharacterBus.h>
#include <AzFramework/Physics/ShapeConfiguration.h>
#include <PhysX/CharacterGameplayBus.h>
#include <PhysXCharacters/Components/CharacterControllerComponent.h>
#include <PhysXCharacters/Components/CharacterGameplayComponent.h>

namespace STWGameplay
{
    PhysXEnemyRuntime::~PhysXEnemyRuntime() { Shutdown(); }

    bool PhysXEnemyRuntime::Initialize(const AZ::Vector3& centerPosition)
    {
        Shutdown();
        if (!centerPosition.IsFinite())
        {
            return false;
        }
        const AZ::Vector3 base = centerPosition - AZ::Vector3(0.0f, 0.0f, CenterHeight);
        m_enemyEntity = AZStd::make_unique<AZ::Entity>("STW_ENEMY_01 PhysX Controller");
        auto* transform = m_enemyEntity->CreateComponent<AzFramework::TransformComponent>();
        transform->SetWorldTM(AZ::Transform::CreateTranslation(base));

        auto configuration = AZStd::make_unique<Physics::CharacterConfiguration>();
        configuration->m_maximumSlopeAngle = 45.0f;
        configuration->m_stepHeight = 0.25f;
        configuration->m_minimumMovementDistance = 0.001f;
        configuration->m_maximumSpeed = 3.0f;
        configuration->m_applyMoveOnPhysicsTick = true;
        auto capsule = AZStd::make_shared<Physics::CapsuleShapeConfiguration>(CapsuleHeight, CapsuleRadius);
        m_enemyEntity->CreateComponent<PhysX::CharacterControllerComponent>(
            AZStd::move(configuration), AZStd::move(capsule));
        PhysX::CharacterGameplayConfiguration gameplay;
        gameplay.m_gravityMultiplier = 1.0f;
        gameplay.m_groundDetectionBoxHeight = 0.08f;
        m_enemyEntity->CreateComponent<PhysX::CharacterGameplayComponent>(gameplay);
        m_enemyEntity->Init();
        m_enemyEntity->Activate();
        if (!IsValid())
        {
            Shutdown();
            return false;
        }
        return true;
    }

    void PhysXEnemyRuntime::Shutdown()
    {
        if (m_enemyEntity && m_enemyEntity->GetState() == AZ::Entity::State::Active)
        {
            m_enemyEntity->Deactivate();
        }
        m_enemyEntity.reset();
    }

    bool PhysXEnemyRuntime::QueueVelocity(const AZ::Vector3& velocity)
    {
        if (!IsValid() || !velocity.IsFinite())
        {
            return false;
        }
        Physics::CharacterRequestBus::Event(
            m_enemyEntity->GetId(), &Physics::CharacterRequests::AddVelocityForTick, velocity);
        return true;
    }

    bool PhysXEnemyRuntime::Synchronize(AZ::Vector3& centerPosition, bool& grounded) const
    {
        if (!IsValid())
        {
            return false;
        }
        AZ::Vector3 base = AZ::Vector3::CreateZero();
        Physics::CharacterRequestBus::EventResult(
            base, m_enemyEntity->GetId(), &Physics::CharacterRequests::GetBasePosition);
        PhysX::CharacterGameplayRequestBus::EventResult(
            grounded, m_enemyEntity->GetId(), &PhysX::CharacterGameplayRequests::IsOnGround);
        centerPosition = base + AZ::Vector3(0.0f, 0.0f, CenterHeight);
        return centerPosition.IsFinite();
    }

    bool PhysXEnemyRuntime::ResetPosition(const AZ::Vector3& centerPosition)
    {
        if (!IsValid() || !centerPosition.IsFinite())
        {
            return false;
        }
        Physics::CharacterRequestBus::Event(
            m_enemyEntity->GetId(), &Physics::CharacterRequests::SetBasePosition,
            centerPosition - AZ::Vector3(0.0f, 0.0f, CenterHeight));
        return true;
    }

    bool PhysXEnemyRuntime::IsValid() const
    {
        if (!m_enemyEntity || m_enemyEntity->GetState() != AZ::Entity::State::Active)
        {
            return false;
        }
        bool present = false;
        Physics::CharacterRequestBus::EventResult(
            present, m_enemyEntity->GetId(), &Physics::CharacterRequests::IsPresent);
        return present;
    }
}
