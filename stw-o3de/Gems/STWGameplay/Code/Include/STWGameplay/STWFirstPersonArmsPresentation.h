#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/string/string.h>

#include <STWGameplay/ViewmodelPresentation.h>

namespace AZ
{
    class Entity;
}

namespace STWGameplay
{
    //! Presentation-only EMotionFX/Atom integration for the STW_FP_01 first-person arms
    //! rig (skinned arms, gloves and integrated rifle body). Camera-relative, not
    //! world-relative: it consumes the same first-person basis (center/right/aim/up) that
    //! drives the existing static viewmodel meshes, and reacts only to ViewmodelPresentation's
    //! read-only state. It never writes gameplay state and owns no ammo, magazine or damage
    //! data. Scope: STW_RIFLE_02 only, the equipment profile the STW_FP_01 asset was authored
    //! for; other profiles keep the existing static per-profile viewmodel mesh unchanged.
    class STWFirstPersonArmsPresentation final
    {
    public:
        STWFirstPersonArmsPresentation() = default;
        ~STWFirstPersonArmsPresentation();

        STWFirstPersonArmsPresentation(const STWFirstPersonArmsPresentation&) = delete;
        STWFirstPersonArmsPresentation& operator=(const STWFirstPersonArmsPresentation&) = delete;

        //! Creates the presentation entity on first call, positions it from the camera-relative
        //! basis, and selects the idle/ads/reload motion from the current viewmodel state.
        void Update(
            float deltaTime, const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim,
            const AZ::Vector3& up, ViewmodelState viewmodelState, float adsBlend);
        void SetVisible(bool visible);
        void Shutdown();

        bool IsActorAssetReady() const { return m_actorAssetReady; }
        bool IsActorInstanceReady() const { return m_actorInstanceReady; }
        bool IsSkinnedMeshVisible() const { return m_skinnedMeshVisible; }
        bool IsMotionAssetReady() const { return m_motionAssetReady; }

    private:
        bool ResolveProducts();
        bool TryCreatePresentationEntity();
        void SelectMotion(const AZ::Data::AssetId& motionAssetId, bool loop);
        void SampleRuntimeDiagnostics();

        AZ::Entity* m_entity = nullptr;
        AZ::EntityId m_entityId;
        AZ::Data::AssetId m_actorAssetId;
        AZ::Data::AssetId m_idleMotionAssetId;
        AZ::Data::AssetId m_adsMotionAssetId;
        AZ::Data::AssetId m_reloadMotionAssetId;
        AZ::Data::AssetId m_currentMotionAssetId;

        bool m_productsResolved = false;
        bool m_productsMissing = false;
        bool m_actorAssetReady = false;
        bool m_actorInstanceReady = false;
        bool m_skinnedMeshVisible = false;
        bool m_motionAssetReady = false;
        bool m_visible = true;
        bool m_readyReported = false;
    };
}
