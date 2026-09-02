#include "PhysXPlayerRuntime.h"

#include <AzCore/Debug/Trace.h>
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
        constexpr float MantleProbeDistance = 1.00f;
        constexpr float MantleMaxHeight = 0.80f;
        constexpr float MantleMinHeight = 0.10f;

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

    bool PhysXPlayerRuntime::CanStartMantle(const AZ::Vector3& direction, bool grounded) const
    {
        if (!IsValid() || !grounded || !direction.IsFinite())
        {
            return false;
        }
        Physics::Character* character = nullptr;
        Physics::CharacterRequestBus::EventResult(
            character, m_playerEntity->GetId(), &Physics::CharacterRequests::GetCharacter);
        if (!character || !character->GetScene())
        {
            return false;
        }
        const AZ::Vector3 planarDirection(direction.GetX(), direction.GetY(), 0.0f);
        if (planarDirection.GetLengthSq() < 0.01f)
        {
            return false;
        }
        const AZ::Vector3 base = character->GetBasePosition();
        const AZ::EntityId playerId = m_playerEntity->GetId();
        AzPhysics::RayCastRequest forward;
        forward.m_start = base + AZ::Vector3::CreateAxisZ(0.10f);
        forward.m_direction = planarDirection.GetNormalized();
        forward.m_distance = MantleProbeDistance;
        forward.m_reportMultipleHits = true;
        forward.m_collisionGroup = character->GetCollisionGroup();
        const AzPhysics::SceneQueryHits forwardHits = character->GetScene()->QueryScene(&forward);
        bool forwardProbeReported = false;
        for (const AzPhysics::SceneQueryHit& obstacle : forwardHits.m_hits)
        {
            if (obstacle.m_entityId == playerId || obstacle.m_normal.GetZ() > 0.5f)
            {
                continue;
            }
            AZ_Printf("STWGameplay", "MANTLE_DIAG forward_probe=PASS hit_distance=%.3f hit_position=(%.3f,%.3f,%.3f)\n",
                obstacle.m_distance, obstacle.m_position.GetX(), obstacle.m_position.GetY(), obstacle.m_position.GetZ());
            forwardProbeReported = true;
            bool topProbeReported = false;
            for (const float offset : { 0.45f, 0.90f, 1.35f })
            {
                const AZ::Vector3 topStart = obstacle.m_position + forward.m_direction * offset
                    + AZ::Vector3::CreateAxisZ(MantleMaxHeight);
                AzPhysics::RayCastRequest top;
                top.m_start = topStart;
                top.m_direction = -AZ::Vector3::CreateAxisZ(1.0f);
                top.m_distance = MantleMaxHeight + 0.75f;
                top.m_collisionGroup = character->GetCollisionGroup();
                const AzPhysics::SceneQueryHits topHits = character->GetScene()->QueryScene(&top);
                for (const AzPhysics::SceneQueryHit& surface : topHits.m_hits)
                {
                    if (surface.m_entityId == playerId || surface.m_normal.GetZ() < 0.7f)
                    {
                        continue;
                    }
                    if (!topProbeReported)
                    {
                        AZ_Printf("STWGameplay", "MANTLE_DIAG top_probe=PASS top_position=(%.3f,%.3f,%.3f)\n",
                            surface.m_position.GetX(), surface.m_position.GetY(), surface.m_position.GetZ());
                        topProbeReported = true;
                    }
                    const float height = surface.m_position.GetZ() - base.GetZ();
                    if (height < MantleMinHeight || height > MantleMaxHeight)
                    {
                        continue;
                    }
                    const AZ::Vector3 landingBase = obstacle.m_position + forward.m_direction * offset;
                    const AZ::Transform pose = AZ::Transform::CreateTranslation(
                        landingBase + AZ::Vector3::CreateAxisZ(0.5f * CapsuleHeight));
                    AzPhysics::OverlapRequest clearance = AzPhysics::OverlapRequestHelpers::CreateCapsuleOverlapRequest(
                        CapsuleHeight, CapsuleRadius, pose);
                    clearance.m_collisionGroup = character->GetCollisionGroup();
                    const AzPhysics::SceneQueryHits overlaps = character->GetScene()->QueryScene(&clearance);
                    const bool clear = AZStd::none_of(
                        overlaps.m_hits.begin(), overlaps.m_hits.end(),
                        [&playerId](const AzPhysics::SceneQueryHit& hit) { return hit.m_entityId != playerId; });
                    AZ_Printf("STWGameplay", "MANTLE_DIAG clearance_probe=%s destination=(%.3f,%.3f,%.3f)\n",
                        clear ? "PASS" : "FAIL", landingBase.GetX(), landingBase.GetY(), landingBase.GetZ());
                    if (clear)
                    {
                        return true;
                    }
                }
            }
            if (!topProbeReported)
            {
                AZ_Printf("STWGameplay", "MANTLE_DIAG top_probe=FAIL\n");
                AZ_Printf("STWGameplay", "MANTLE_DIAG clearance_probe=NOT_REACHED\n");
            }
            break;
        }
        if (!forwardProbeReported)
        {
            AZ_Printf("STWGameplay", "MANTLE_DIAG forward_probe=FAIL origin=(%.3f,%.3f,%.3f) direction=(%.3f,%.3f,%.3f) distance=%.3f\n",
                forward.m_start.GetX(), forward.m_start.GetY(), forward.m_start.GetZ(), forward.m_direction.GetX(),
                forward.m_direction.GetY(), forward.m_direction.GetZ(), forward.m_distance);
            AZ_Printf("STWGameplay", "MANTLE_DIAG top_probe=NOT_REACHED\n");
            AZ_Printf("STWGameplay", "MANTLE_DIAG clearance_probe=NOT_REACHED\n");
        }
        return false;
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
