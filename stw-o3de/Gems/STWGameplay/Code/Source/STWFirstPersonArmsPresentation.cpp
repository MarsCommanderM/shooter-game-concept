#include <STWGameplay/STWFirstPersonArmsPresentation.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>

#include <Integration/Assets/ActorAsset.h>
#include <Integration/Assets/MotionAsset.h>
#include <Integration/Components/ActorComponent.h>
#include <Integration/Components/SimpleMotionComponent.h>
#include <Integration/ActorComponentBus.h>
#include <Integration/SimpleMotionComponentBus.h>

#include <cstring>

namespace STWGameplay
{
    namespace
    {
        template<class AssetType>
        AZ::Data::AssetId FindFirstPersonAssetId(const char* path)
        {
            AZ::Data::AssetId assetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                assetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath,
                path, azrtti_typeid<AssetType>(), false);
            return assetId;
        }
    }

    STWFirstPersonArmsPresentation::~STWFirstPersonArmsPresentation()
    {
        Shutdown();
    }

    bool STWFirstPersonArmsPresentation::ResolveProducts()
    {
        if (m_productsMissing)
        {
            return false;
        }

        if (m_actorPath == nullptr || m_actorPath[0] == '\0' || m_motionPath == nullptr || m_motionPath[0] == '\0')
        {
            return false;
        }
        if (m_productsResolved && m_resolvedActorPath == m_actorPath && m_resolvedMotionPath == m_motionPath
            && m_actorAssetId.IsValid() && m_motionAssetId.IsValid())
        {
            return true;
        }
        if (m_resolvedActorPath != m_actorPath || !m_actorAssetId.IsValid())
        {
            m_actorAssetId = FindFirstPersonAssetId<EMotionFX::Integration::ActorAsset>(m_actorPath);
            m_resolvedActorPath = m_actorAssetId.IsValid() ? m_actorPath : nullptr;
        }
        if (m_resolvedMotionPath != m_motionPath || !m_motionAssetId.IsValid())
        {
            m_motionAssetId = FindFirstPersonAssetId<EMotionFX::Integration::MotionAsset>(m_motionPath);
            m_resolvedMotionPath = m_motionAssetId.IsValid() ? m_motionPath : nullptr;
        }
        if (!m_actorAssetId.IsValid() || !m_motionAssetId.IsValid())
        {
            m_productsResolved = false;
            if (!m_missReported)
            {
                m_missReported = true;
                AZ_Printf(
                    "STWGameplay",
                    "ATOM_FIRSTPERSON_ARMS_ACTOR result=MISS actor=%s motion=%s\n",
                    m_actorPath, m_motionPath);
            }
            return false;
        }

        m_missReported = false;
        m_productsResolved = true;
        return true;
    }

    bool STWFirstPersonArmsPresentation::TryCreatePresentationEntity()
    {
        if (!ResolveProducts())
        {
            return false;
        }
        if (m_entity)
        {
            return true;
        }

        auto actorConfiguration = EMotionFX::Integration::ActorComponent::Configuration{};
        actorConfiguration.m_actorAsset = AZ::Data::Asset<EMotionFX::Integration::ActorAsset>(
            m_actorAssetId, azrtti_typeid<EMotionFX::Integration::ActorAsset>(), m_actorPath);

        auto motionConfiguration = EMotionFX::Integration::SimpleMotionComponent::Configuration{};
        motionConfiguration.m_motionAsset = AZ::Data::Asset<EMotionFX::Integration::MotionAsset>(
            m_motionAssetId, azrtti_typeid<EMotionFX::Integration::MotionAsset>(), m_motionPath);
        motionConfiguration.m_loop = m_motionLoop;
        motionConfiguration.m_retarget = false;
        motionConfiguration.m_playOnActivation = true;
        motionConfiguration.m_inPlace = true;
        motionConfiguration.m_freezeAtLastFrame = true;

        AzFramework::EntityContextId gameContextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            gameContextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        if (gameContextId.IsNull())
        {
            return false;
        }

        AZ::Entity* entity = aznew AZ::Entity("STW First Person Arms");
        entity->SetRuntimeActiveByDefault(false);
        entity->CreateComponent<AzFramework::TransformComponent>();
        entity->CreateComponent<EMotionFX::Integration::ActorComponent>(&actorConfiguration);
        entity->CreateComponent<EMotionFX::Integration::SimpleMotionComponent>(&motionConfiguration);
        m_entityId = entity->GetId();

        AzFramework::GameEntityContextRequestBus::Broadcast(
            &AzFramework::GameEntityContextRequests::AddGameEntity, entity);
        m_entity = entity;
        AzFramework::GameEntityContextRequestBus::Broadcast(
            &AzFramework::GameEntityContextRequests::ActivateGameEntity, m_entityId);
        m_currentMotionAssetId = m_motionAssetId;
        m_actorAssetReady = true;
        return true;
    }

    void STWFirstPersonArmsPresentation::SelectMotion(const AZ::Data::AssetId& motionAssetId, bool loop)
    {
        if (!m_entity || !motionAssetId.IsValid() || m_currentMotionAssetId == motionAssetId)
        {
            return;
        }
        EMotionFX::Integration::SimpleMotionComponentRequestBus::Event(
            m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::LoopMotion, loop);
        EMotionFX::Integration::SimpleMotionComponentRequestBus::Event(
            m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::Motion, motionAssetId);
        EMotionFX::Integration::SimpleMotionComponentRequestBus::Event(
            m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::PlayMotion);
        m_currentMotionAssetId = motionAssetId;
        m_motionAssetReady = false;
    }

    void STWFirstPersonArmsPresentation::SampleRuntimeDiagnostics()
    {
        if (!m_entity)
        {
            return;
        }

        EMotionFX::ActorInstance* actorInstance = nullptr;
        EMotionFX::Integration::ActorComponentRequestBus::EventResult(
            actorInstance, m_entityId, &EMotionFX::Integration::ActorComponentRequests::GetActorInstance);
        m_actorInstanceReady = actorInstance != nullptr;
        if (!m_actorInstanceReady)
        {
            return;
        }

        EMotionFX::Integration::ActorComponentRequestBus::EventResult(
            m_skinnedMeshVisible, m_entityId,
            &EMotionFX::Integration::ActorComponentRequests::GetRenderActorVisible);

        float duration = 0.0f;
        EMotionFX::Integration::SimpleMotionComponentRequestBus::EventResult(
            duration, m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::GetDuration);
        m_motionAssetReady = m_currentMotionAssetId.IsValid() && duration > 0.0f;

        if (!m_readyReported && m_actorInstanceReady && m_skinnedMeshVisible && m_motionAssetReady)
        {
            m_readyReported = true;
            AZ_Printf(
                "STWGameplay",
                "ATOM_FIRSTPERSON_ARMS_ACTOR result=READY actor=%s motion=%s mesh=ready material=bound motion=ready\n",
                m_actorPath != nullptr ? m_actorPath : "",
                m_motionPath != nullptr ? m_motionPath : "");
        }
    }

    bool STWFirstPersonArmsPresentation::Update(
        float deltaTime, const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim,
        const AZ::Vector3& up, const FirstPersonArmSelection& selection)
    {
        (void)deltaTime;
        const bool actorChanged = m_entity != nullptr && selection.m_actorPath != m_actorPath;
        const bool motionChanged = m_entity != nullptr && selection.m_motionPath != m_motionPath;
        if (actorChanged)
        {
            Shutdown();
        }
        else if (motionChanged)
        {
            m_missReported = false;
            m_productsResolved = false;
            m_readyReported = false;
        }
        m_actorPath = selection.m_actorPath != nullptr ? selection.m_actorPath : "";
        m_motionPath = selection.m_motionPath != nullptr ? selection.m_motionPath : "";
        m_motionLoop = selection.m_loop;
        m_poseRight = selection.m_right;
        m_poseForward = selection.m_forward;
        m_poseUp = selection.m_up;
        const AZ::Vector3 posedCenter = center + right * m_poseRight + aim * m_poseForward + up * m_poseUp;

        if (!selection.m_owned)
        {
            SetVisible(false);
            return false;
        }
        if (!TryCreatePresentationEntity())
        {
            return false;
        }
        if (m_currentMotionAssetId != m_motionAssetId)
        {
            SelectMotion(m_motionAssetId, m_motionLoop);
        }

        if (actorChanged || motionChanged)
        {
            m_actorInstanceReady = false;
            m_skinnedMeshVisible = false;
            m_motionAssetReady = false;
            return false;
        }

        // Assimp imports the rig's OBJ-style axes as (-X, Z, Y), the same mapping
        // UpdateViewmodelMeshTransform uses for the static per-profile weapon meshes.
        const AZ::Quaternion orientation =
            AZ::Quaternion::CreateFromMatrix3x3(AZ::Matrix3x3::CreateFromColumns(-right, up, aim));
        const AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(orientation, posedCenter);
        AZ::TransformBus::Event(m_entityId, &AZ::TransformBus::Events::SetWorldTM, transform);

        SampleRuntimeDiagnostics();
        return m_actorInstanceReady && m_skinnedMeshVisible && m_motionAssetReady && m_productsResolved;
    }

    void STWFirstPersonArmsPresentation::SetVisible(bool visible)
    {
        if (visible == m_visible || !m_entity)
        {
            m_visible = visible;
            return;
        }
        m_visible = visible;
        EMotionFX::Integration::ActorComponentRequestBus::Event(
            m_entityId, &EMotionFX::Integration::ActorComponentRequests::SetRenderCharacter, visible);
    }

    void STWFirstPersonArmsPresentation::Shutdown()
    {
        if (m_entity)
        {
            AzFramework::GameEntityContextRequestBus::Broadcast(
                &AzFramework::GameEntityContextRequests::DestroyGameEntity, m_entityId);
        }
        m_entity = nullptr;
        m_entityId = AZ::EntityId();
        m_actorAssetId = AZ::Data::AssetId();
        m_motionAssetId = AZ::Data::AssetId();
        m_currentMotionAssetId = AZ::Data::AssetId();
        m_resolvedActorPath = nullptr;
        m_resolvedMotionPath = nullptr;
        m_productsResolved = false;
        m_actorAssetReady = false;
        m_actorInstanceReady = false;
        m_skinnedMeshVisible = false;
        m_motionAssetReady = false;
        m_readyReported = false;
        m_missReported = false;
    }
}
