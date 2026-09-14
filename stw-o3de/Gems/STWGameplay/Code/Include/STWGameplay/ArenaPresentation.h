#pragma once

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/string/string.h>
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>

namespace STWGameplay
{
    // Presentation-only native arena visuals. This class never owns collision, spawn,
    // combat, encounter, or navigation state; those remain in the gameplay runtimes.
    class ArenaPresentation final
    {
    public:
        static constexpr size_t VisualAssetCount = 9;

        ArenaPresentation() = default;
        ~ArenaPresentation();

        void Initialize(const AZ::Uuid& contextId);
        void Update();
        void Shutdown();

        bool IsGeometryReady() const;
        bool IsMaterialSetReady() const;
        bool IsEnvironmentPresentationReady() const;
        bool IsReady() const;

        static constexpr bool IsVisualOnly() { return true; }
        static constexpr size_t GetVisualAssetCount() { return VisualAssetCount; }

        // Updates between catalog re-scans while any visual asset is still
        // unresolved. The catalog is populated asynchronously, so an early miss
        // must not be permanent, but enumerating it every frame would be waste.
        static constexpr uint32_t DiscoveryRetryUpdates = 60;

        // Bit per visual asset that is not yet usable. An asset counts as
        // resolved only when BOTH its model and its material were found in the
        // catalog AND both resulting asset identities are valid, so a
        // half-resolved entry keeps discovery open rather than silently shipping
        // an arena with a hole in it.
        //
        // The identity arrays are not decoration. Before Block 26E-R4 this took
        // only the path-found flags while the per-entry acquisition path also
        // required valid AssetIds, so a catalog entry that matched by path but
        // produced an invalid AssetId cleared its mask bit, permanently ended
        // discovery, and left IsGeometryReady() false forever with no retry.
        // The mask now uses the same effective criterion as runtime readiness.
        static uint32_t ComputeUnresolvedMask(
            const AZStd::array<bool, VisualAssetCount>& modelFound,
            const AZStd::array<bool, VisualAssetCount>& materialFound,
            const AZStd::array<bool, VisualAssetCount>& modelIdValid,
            const AZStd::array<bool, VisualAssetCount>& materialIdValid,
            const AZStd::array<bool, VisualAssetCount>& alreadyDiscovered);

        struct CameraSpaceRelation
        {
            bool m_valid = false;
            bool m_cameraInsideBounds = false;
            AZ::Vector3 m_cameraSpaceCenter = AZ::Vector3::CreateZero();
        };

        static CameraSpaceRelation CalculateCameraSpaceRelation(
            const AZ::Aabb& worldBounds,
            const AZ::Vector3& cameraPosition,
            const AZ::Vector3& cameraRight,
            const AZ::Vector3& cameraForward,
            const AZ::Vector3& cameraUp);

    private:
        struct VisualAssetState
        {
            bool m_discovered = false;
            bool m_meshAcquired = false;
            AZ::Data::AssetId m_modelAssetId;
            AZ::Data::AssetId m_materialAssetId;
            AZ::Data::Asset<AZ::RPI::MaterialAsset> m_materialAsset;
            AZ::Data::Instance<AZ::RPI::Material> m_material;
            AZ::Render::MeshFeatureProcessorInterface::MeshHandle m_meshHandle;
            AZStd::string m_modelPath;
            bool m_materialAppliedToModel = false;
            uint32_t m_materialRebindCount = 0;
            uint32_t m_runtimeDiagnosticAttempts = 0;
            bool m_runtimeDiagnosticReported = false;
            bool m_visibilityDiagnosticReported = false;
        };

        void DiscoverAssets();
        void ReportDiscoveryState(
            uint32_t unresolvedMask,
            const AZStd::array<bool, VisualAssetCount>& modelFound,
            const AZStd::array<bool, VisualAssetCount>& materialFound);
        void UpdateAsset(VisualAssetState& state, const char* modelPath, const char* materialPath);
        void ReportRuntimeMaterialIdentity(size_t index);
        void ReportRuntimeGeometryState(size_t index);
        void IsolateDefaultLevelScaffold();
        void InitializeEnvironmentLight();

        AZ::Render::MeshFeatureProcessorInterface* m_meshFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle m_directionalLightHandle;
        AZStd::array<VisualAssetState, VisualAssetCount> m_assets;
        bool m_assetsDiscovered = false;
        uint32_t m_discoveryAttempts = 0;
        uint32_t m_updatesSinceDiscoveryAttempt = 0;
        // Sentinel distinct from every real mask, so the first outcome always
        // reports. Diagnostics fire only when this changes, which bounds them to
        // one line per state transition instead of one per Update().
        uint32_t m_reportedUnresolvedMask = 0xFFFFFFFFu;
        bool m_environmentLightInitialized = false;
        bool m_initialized = false;
        bool m_defaultLevelGroundHidden = false;
        bool m_defaultLevelGridHidden = false;
        bool m_defaultLevelIsolationReported = false;
    };
}
