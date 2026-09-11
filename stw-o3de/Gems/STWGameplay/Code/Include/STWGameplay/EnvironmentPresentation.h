#pragma once

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Uuid.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <Atom/RPI.Reflect/System/AnyAsset.h>
#include <Atom/Feature/CoreLights/DirectionalLightFeatureProcessorInterface.h>
#include <Atom/Feature/CoreLights/PointLightFeatureProcessorInterface.h>

namespace AZ::Render
{
    class ImageBasedLightFeatureProcessorInterface;
    class SkyBoxFeatureProcessorInterface;
    class PostProcessFeatureProcessorInterface;
}

namespace STWGameplay
{
    //! Presentation-only cinematic environment layer for STW.
    //!
    //! Wires native Atom presentation features (HDRI/IBL lighting preset, exposure
    //! control, SSAO, bloom, HDR colour grading, and a small accent-light rig) and
    //! neutralises leftover DefaultLevel visual content (the stock "Shader Ball").
    //! It owns no gameplay, collision, spawn, AI, or ArenaLayout state and never
    //! writes any of it. Everything here is scene-global rendering configuration.
    class EnvironmentPresentation final
    {
    public:
        //! A restrained interior accent light layered on top of the HDRI key/fill.
        struct AccentLightSpec
        {
            AZ::Vector3 m_position = AZ::Vector3::CreateZero();
            AZ::Color m_color = AZ::Color::CreateOne();
            float m_candela = 0.0f;
            float m_attenuationRadiusMeters = 0.0f;
            float m_bulbRadiusMeters = 0.0f;
        };

        static constexpr size_t AccentLightCount = 3;

        EnvironmentPresentation() = default;
        ~EnvironmentPresentation();

        EnvironmentPresentation(const EnvironmentPresentation&) = delete;
        EnvironmentPresentation& operator=(const EnvironmentPresentation&) = delete;

        void Initialize(const AZ::Uuid& contextId);
        void Update();
        void Shutdown();

        bool IsLightingPresetApplied() const { return m_lightingPresetApplied; }
        bool IsPostProcessApplied() const { return m_postProcessApplied; }
        bool IsAccentRigApplied() const { return m_accentRigApplied; }
        bool IsDefaultLevelNeutralised() const { return m_shaderBallHidden; }
        bool IsReady() const;

        //! Product path of the cached HDRI lighting preset this layer drives.
        static const char* GetLightingPresetPath();
        static const char* GetFallbackLightingPresetPath();

        //! Deterministic, engine-free tuning used by both Update() and the unit tests.
        static AZStd::array<AccentLightSpec, AccentLightCount> GetAccentLightRig();
        static float GetSsaoStrength();
        static float GetSsaoSamplingRadius();
        static float GetBloomIntensity();
        static float GetBloomThreshold();
        static float GetColorGradingContrast();
        static float GetColorGradingPostSaturation();
        //! A plausible manual exposure trim (EV) layered under the preset's own control.
        static float GetExposureCompensationTrim();
        static bool IsAccentRigPhysicallyPlausible(const AZStd::array<AccentLightSpec, AccentLightCount>& rig);

    private:
        void ResolveFeatureProcessors(const AZ::Uuid& contextId);
        void TryApplyLightingPreset();
        void ApplyPostProcess();
        void ApplyAccentRig();
        void NeutraliseDefaultLevelVisuals();

        AZ::Render::ImageBasedLightFeatureProcessorInterface* m_iblFeatureProcessor = nullptr;
        AZ::Render::SkyBoxFeatureProcessorInterface* m_skyboxFeatureProcessor = nullptr;
        AZ::Render::PostProcessFeatureProcessorInterface* m_postProcessFeatureProcessor = nullptr;
        AZ::Render::PointLightFeatureProcessorInterface* m_pointLightFeatureProcessor = nullptr;
        AZ::Render::DirectionalLightFeatureProcessorInterface* m_directionalLightFeatureProcessor = nullptr;

        AZ::Data::Asset<AZ::RPI::AnyAsset> m_lightingPresetAsset;
        bool m_lightingPresetRequested = false;
        bool m_usingFallbackPreset = false;

        AZ::EntityId m_postSettingsEntityId;
        AZStd::vector<AZ::Render::DirectionalLightFeatureProcessorInterface::LightHandle> m_presetLightHandles;
        AZStd::array<AZ::Render::PointLightFeatureProcessorInterface::LightHandle, AccentLightCount> m_accentLightHandles;

        bool m_initialized = false;
        bool m_lightingPresetApplied = false;
        bool m_postProcessApplied = false;
        bool m_accentRigApplied = false;
        bool m_shaderBallHidden = false;
        bool m_readyReported = false;
    };
}
