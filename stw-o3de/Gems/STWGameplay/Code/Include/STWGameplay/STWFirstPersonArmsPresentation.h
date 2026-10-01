#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/string/string.h>

#include <STWGameplay/FirstPersonArmCatalog.h>

namespace AZ
{
    class Entity;
}

namespace STWGameplay
{
    //! Presentation-only EMotionFX/Atom integration for the first-person arms.
    //! Camera-relative, not world-relative: it consumes the same first-person basis
    //! (center/right/aim/up) that drives the existing static viewmodel meshes. The actor,
    //! motion, and hip/ADS/reload/inspect offset come from FirstPersonArmCatalog::Select
    //! for the active profile. It never writes gameplay state and owns no ammo, magazine
    //! or damage data.
    class STWFirstPersonArmsPresentation final
    {
    public:
        STWFirstPersonArmsPresentation() = default;
        ~STWFirstPersonArmsPresentation();

        STWFirstPersonArmsPresentation(const STWFirstPersonArmsPresentation&) = delete;
        STWFirstPersonArmsPresentation& operator=(const STWFirstPersonArmsPresentation&) = delete;

        //! Creates the presentation entity on first call, positions it from the camera-relative
        //! basis plus the selected pose offset, and plays that pose's motion.
        //! Returns true only when this selection's actor instance is visible and its motion is ready.
        //! The frame that swaps actor or motion returns false so a stale clip cannot count.
        bool Update(
            float deltaTime, const AZ::Vector3& center, const AZ::Vector3& right, const AZ::Vector3& aim,
            const AZ::Vector3& up, const FirstPersonArmSelection& selection);
        void SetVisible(bool visible);
        void Shutdown();

        bool IsActorAssetReady() const { return m_actorAssetReady; }
        bool IsActorInstanceReady() const { return m_actorInstanceReady; }
        bool IsSkinnedMeshVisible() const { return m_skinnedMeshVisible; }
        bool IsMotionAssetReady() const { return m_motionAssetReady; }
        const char* LoadedActorPath() const { return m_actorPath; }
        const char* LoadedMotionPath() const { return m_motionPath; }

    private:
        bool ResolveProducts();
        bool TryCreatePresentationEntity();
        void SelectMotion(const AZ::Data::AssetId& motionAssetId, bool loop);
        void SampleRuntimeDiagnostics();

        AZ::Entity* m_entity = nullptr;
        AZ::EntityId m_entityId;
        AZ::Data::AssetId m_actorAssetId;
        AZ::Data::AssetId m_motionAssetId;
        AZ::Data::AssetId m_currentMotionAssetId;
        const char* m_actorPath = "";
        const char* m_motionPath = "";
        const char* m_resolvedActorPath = nullptr;
        const char* m_resolvedMotionPath = nullptr;
        float m_poseRight = 0.0f;
        float m_poseForward = 0.0f;
        float m_poseUp = 0.0f;

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
