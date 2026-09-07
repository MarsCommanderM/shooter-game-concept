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
        static constexpr size_t VisualAssetCount = 4;

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
        bool m_environmentLightInitialized = false;
        bool m_initialized = false;
        bool m_defaultLevelGroundHidden = false;
        bool m_defaultLevelGridHidden = false;
        bool m_defaultLevelIsolationReported = false;
    };
}
