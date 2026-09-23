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

namespace STWGameplay
{
    namespace
    {
        constexpr const char* FirstPersonActorPath =
            "assets/industrialyard/stw_industrial_yard_01/firstperson/stw_fp_01.actor";
        constexpr const char* FirstPersonIdleMotionPath =
            "assets/industrialyard/stw_industrial_yard_01/firstperson/stw_fp_01_idle.motion";
        constexpr const char* FirstPersonAdsMotionPath =
            "assets/industrialyard/stw_industrial_yard_01/firstperson/stw_fp_01_ads.motion";
        constexpr const char* FirstPersonReloadMotionPath =
            "assets/industrialyard/stw_industrial_yard_01/firstperson/stw_fp_01_reload.motion";
        // Same convention UpdateViewmodelMeshTransform uses for the static viewmodel meshes:
        // the rig's own forward/up axes are mapped to the authored STW convention at presentation time.
        constexpr float AdsBlendThreshold = 0.5f;

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
        if (m_productsResolved)
        {
            return true;
        }
        if (m_productsMissing)
        {
            return false;
        }

        m_actorAssetId = FindFirstPersonAssetId<EMotionFX::Integration::ActorAsset>(FirstPersonActorPath);
        m_idleMotionAssetId = FindFirstPersonAssetId<EMotionFX::Integration::MotionAsset>(FirstPersonIdleMotionPath);
        m_adsMotionAssetId = FindFirstPersonAssetId<EMotionFX::Integration::MotionAsset>(FirstPersonAdsMotionPath);
        m_reloadMotionAssetId = FindFirstPersonAssetId<EMotionFX::Integration::MotionAsset>(FirstPersonReloadMotionPath);
        if (!m_actorAssetId.IsValid() || !m_idleMotionAssetId.IsValid() || !m_adsMotionAssetId.IsValid()
            || !m_reloadMotionAssetId.IsValid())
        {
            // The asset catalog has not finished scanning yet, or the products genuinely do
            // not exist. Either way, retry is cheap: leave m_productsMissing false so the next
            // Update() call re-resolves instead of latching a false negative.
            return false;
        }

        m_productsResolved = true;
        return true;
    }

    bool STWFirstPersonArmsPresentation::TryCreatePresentationEntity()
    {
        if (m_entity)
        {
            return true;
        }
        if (!ResolveProducts())
        {
            return false;
        }

        auto actorConfiguration = EMotionFX::Integration::ActorComponent::Configuration{};
        actorConfiguration.m_actorAsset = AZ::Data::Asset<EMotionFX::Integration::ActorAsset>(
            m_actorAssetId, azrtti_typeid<EMotionFX::Integration::ActorAsset>(), FirstPersonActorPath);

        auto motionConfiguration = EMotionFX::Integration::SimpleMotionComponent::Configuration{};
        motionConfiguration.m_motionAsset = AZ::Data::Asset<EMotionFX::Integration::MotionAsset>(
            m_idleMotionAssetId, azrtti_typeid<EMotionFX::Integration::MotionAsset>(), FirstPersonIdleMotionPath);
        motionConfiguration.m_loop = true;
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

        AZ::Entity* entity = aznew AZ::Entity("STW_FP_01 Presentation");
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
        m_currentMotionAssetId = m_idleMotionAssetId;
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
                "ATOM_FIRSTPERSON_ARMS_MESH result=PASS actor=%s mesh=ready material=bound motion=ready\n",
                FirstPersonActorPath);
        }
    }

    void STWFirstPersonArmsPresentation::Update(
        float deltaTime, const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim,
        const AZ::Vector3& up, ViewmodelState viewmodelState, float adsBlend)
    {
        (void)deltaTime;
        if (!TryCreatePresentationEntity())
        {
            return;
        }

        if (viewmodelState == ViewmodelState::Reload)
        {
            SelectMotion(m_reloadMotionAssetId, false);
        }
        else if (adsBlend > AdsBlendThreshold)
        {
            SelectMotion(m_adsMotionAssetId, true);
        }
        else
        {
            SelectMotion(m_idleMotionAssetId, true);
        }

        // Assimp imports the rig's OBJ-style axes as (-X, Z, Y), the same mapping
        // UpdateViewmodelMeshTransform uses for the static per-profile weapon meshes.
        const AZ::Quaternion orientation =
            AZ::Quaternion::CreateFromMatrix3x3(AZ::Matrix3x3::CreateFromColumns(-right, up, aim));
        const AZ::Transform transform = AZ::Transform::CreateFromQuaternionAndTranslation(orientation, center);
        AZ::TransformBus::Event(m_entityId, &AZ::TransformBus::Events::SetWorldTM, transform);

        SampleRuntimeDiagnostics();
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
        m_currentMotionAssetId = AZ::Data::AssetId();
        m_actorInstanceReady = false;
        m_skinnedMeshVisible = false;
        m_motionAssetReady = false;
    }
}
