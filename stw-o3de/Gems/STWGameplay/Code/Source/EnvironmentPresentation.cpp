#include <STWGameplay/EnvironmentPresentation.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Quaternion.h>

#include <AzFramework/Components/CameraBus.h>

#include <AtomLyIntegration/CommonFeatures/Mesh/MeshComponentBus.h>

#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

#include <Atom/Feature/CoreLights/PhotometricValue.h>
#include <Atom/Feature/ImageBasedLights/ImageBasedLightFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessSettingsInterface.h>
#include <Atom/Feature/PostProcess/Bloom/BloomSettingsInterface.h>
#include <Atom/Feature/PostProcess/ColorGrading/HDRColorGradingSettingsInterface.h>
#include <Atom/Feature/PostProcess/ExposureControl/ExposureControlSettingsInterface.h>
#include <Atom/Feature/PostProcess/Ssao/SsaoSettingsInterface.h>
#include <Atom/Feature/SkyBox/SkyBoxFeatureProcessorInterface.h>
#include <Atom/Feature/Utils/LightingPreset.h>

namespace STWGameplay
{
    namespace
    {
        // A cinematic warm-key / cool-sky HDRI already present in the project cache
        // (skybox + ibldiffuse + iblspecular products verified in Block 26C).
        constexpr const char* LightingPresetPath =
            "lightingpresets/lowcontrast/blouberg_sunrise_1.lightingpreset.azasset";
        constexpr const char* FallbackLightingPresetPath =
            "lightingpresets/default.lightingpreset.azasset";

        // Kept in one place so the deferred lighting preset copy loads its cubemaps
        // before ApplyLightingPreset() is invoked on a headless client.
        AZ::Render::LightingPreset g_environmentPreset;
        bool g_environmentPresetValid = false;
    }

    const char* EnvironmentPresentation::GetLightingPresetPath()
    {
        return LightingPresetPath;
    }

    const char* EnvironmentPresentation::GetFallbackLightingPresetPath()
    {
        return FallbackLightingPresetPath;
    }

    AZStd::array<EnvironmentPresentation::AccentLightSpec, EnvironmentPresentation::AccentLightCount>
    EnvironmentPresentation::GetAccentLightRig()
    {
        // Restrained interior practicals layered on top of the HDRI key/fill:
        // two cool ceiling fills over the lanes and one warm bounce near the
        // player spawn. Intensities are modest (interior fixtures, not the sun).
        AZStd::array<AccentLightSpec, AccentLightCount> rig;
        rig[0] = AccentLightSpec{ AZ::Vector3(-5.0f, 3.0f, 3.6f), AZ::Color(0.62f, 0.74f, 1.0f, 1.0f), 850.0f, 16.0f, 0.20f };
        rig[1] = AccentLightSpec{ AZ::Vector3(5.0f, 3.0f, 3.6f), AZ::Color(0.62f, 0.74f, 1.0f, 1.0f), 850.0f, 16.0f, 0.20f };
        rig[2] = AccentLightSpec{ AZ::Vector3(0.0f, -8.0f, 2.4f), AZ::Color(1.0f, 0.82f, 0.60f, 1.0f), 500.0f, 12.0f, 0.15f };
        return rig;
    }

    float EnvironmentPresentation::GetSsaoStrength() { return 1.15f; }
    float EnvironmentPresentation::GetSsaoSamplingRadius() { return 0.05f; }
    float EnvironmentPresentation::GetBloomIntensity() { return 0.06f; }
    float EnvironmentPresentation::GetBloomThreshold() { return 1.05f; }
    float EnvironmentPresentation::GetColorGradingContrast() { return 0.10f; }
    float EnvironmentPresentation::GetColorGradingPostSaturation() { return 0.06f; }
    // R1: firm stop-down for the sunlit metallic deck (Block 26D captured mean luminance ~206,
    // large pinned-white regions). Applied as ManualOnly exposure compensation in EV.
    float EnvironmentPresentation::GetExposureCompensationTrim() { return -1.75f; }

    bool EnvironmentPresentation::IsAccentRigPhysicallyPlausible(
        const AZStd::array<AccentLightSpec, AccentLightCount>& rig)
    {
        for (const AccentLightSpec& spec : rig)
        {
            const bool finite = spec.m_position.IsFinite() && spec.m_color.IsFinite();
            const bool restrained = spec.m_candela > 0.0f && spec.m_candela <= 5000.0f;
            const bool bounded = spec.m_attenuationRadiusMeters > spec.m_bulbRadiusMeters
                && spec.m_attenuationRadiusMeters <= 40.0f && spec.m_bulbRadiusMeters > 0.0f;
            if (!finite || !restrained || !bounded)
            {
                return false;
            }
        }
        return true;
    }

    EnvironmentPresentation::~EnvironmentPresentation()
    {
        Shutdown();
    }

    void EnvironmentPresentation::Initialize(const AZ::Uuid& contextId)
    {
        if (m_initialized || contextId.IsNull())
        {
            return;
        }
        ResolveFeatureProcessors(contextId);
        m_postSettingsEntityId = AZ::Entity::MakeId();
        m_initialized = m_postProcessFeatureProcessor != nullptr || m_skyboxFeatureProcessor != nullptr
            || m_iblFeatureProcessor != nullptr;
    }

    void EnvironmentPresentation::ResolveFeatureProcessors(const AZ::Uuid& contextId)
    {
        m_iblFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::ImageBasedLightFeatureProcessorInterface>(
                contextId);
        m_skyboxFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::SkyBoxFeatureProcessorInterface>(contextId);
        m_postProcessFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::PostProcessFeatureProcessorInterface>(
                contextId);
        m_pointLightFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::PointLightFeatureProcessorInterface>(
                contextId);
        m_directionalLightFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::DirectionalLightFeatureProcessorInterface>(
                contextId);
    }

    void EnvironmentPresentation::Update()
    {
        if (!m_initialized)
        {
            return;
        }

        NeutraliseDefaultLevelVisuals();

        if (!m_lightingPresetApplied)
        {
            TryApplyLightingPreset();
        }

        if (m_lightingPresetApplied && !m_postProcessApplied)
        {
            ApplyPostProcess();
        }

        if (m_lightingPresetApplied && !m_accentRigApplied)
        {
            ApplyAccentRig();
        }

        if (!m_readyReported && IsReady())
        {
            m_readyReported = true;
            AZ_Printf(
                "STWGameplay",
                "STW_ENVIRONMENT_PRESENTATION_ACTIVE=1 lighting_preset=%d post_process=%d accent_rig=%d "
                "defaultlevel_neutralised=%d fallback_preset=%d\n",
                m_lightingPresetApplied ? 1 : 0, m_postProcessApplied ? 1 : 0, m_accentRigApplied ? 1 : 0,
                m_shaderBallHidden ? 1 : 0, m_usingFallbackPreset ? 1 : 0);
            AZ_Printf("STWGameplay", "STW_ENVIRONMENT_HDRI_IBL_READY=1\n");
            AZ_Printf("STWGameplay", "STW_ENVIRONMENT_POST_PROCESS_READY=1\n");
            AZ_Printf("STWGameplay", "STW_ENVIRONMENT_LIGHT_RIG_READY=1\n");
        }
    }

    void EnvironmentPresentation::TryApplyLightingPreset()
    {
        if (m_skyboxFeatureProcessor == nullptr && m_iblFeatureProcessor == nullptr)
        {
            return;
        }

        if (!m_lightingPresetRequested)
        {
            m_lightingPresetRequested = true;
            m_lightingPresetAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::AnyAsset>(
                LightingPresetPath, AZ::RPI::AssetUtils::TraceLevel::Warning);
            if (!m_lightingPresetAsset.GetId().IsValid())
            {
                m_usingFallbackPreset = true;
                m_lightingPresetAsset = AZ::RPI::AssetUtils::GetAssetByProductPath<AZ::RPI::AnyAsset>(
                    FallbackLightingPresetPath, AZ::RPI::AssetUtils::TraceLevel::Warning);
            }
            if (m_lightingPresetAsset.GetId().IsValid())
            {
                m_lightingPresetAsset.QueueLoad();
            }
            return;
        }

        if (!m_lightingPresetAsset.IsReady())
        {
            return;
        }

        if (!g_environmentPresetValid)
        {
            const AZ::Render::LightingPreset* preset =
                m_lightingPresetAsset->GetDataAs<AZ::Render::LightingPreset>();
            if (preset == nullptr)
            {
                return;
            }
            g_environmentPreset = *preset;
            // R1: the preset ships one shadowed directional key; ArenaPresentation already owns the
            // sole authoritative shadowed directional, so drop the preset's to avoid double-key cost.
            g_environmentPreset.m_lights.clear();
            g_environmentPresetValid = true;
            g_environmentPreset.m_iblDiffuseImageAsset.QueueLoad();
            g_environmentPreset.m_iblSpecularImageAsset.QueueLoad();
            g_environmentPreset.m_skyboxImageAsset.QueueLoad();
        }

        const bool cubemapsReady =
            (!g_environmentPreset.m_iblDiffuseImageAsset.GetId().IsValid() || g_environmentPreset.m_iblDiffuseImageAsset.IsReady())
            && (!g_environmentPreset.m_iblSpecularImageAsset.GetId().IsValid() || g_environmentPreset.m_iblSpecularImageAsset.IsReady())
            && (!g_environmentPreset.m_skyboxImageAsset.GetId().IsValid() || g_environmentPreset.m_skyboxImageAsset.IsReady());
        if (!cubemapsReady)
        {
            return;
        }

        if (m_skyboxFeatureProcessor != nullptr)
        {
            m_skyboxFeatureProcessor->Enable(true);
            m_skyboxFeatureProcessor->SetSkyboxMode(AZ::Render::SkyBoxMode::Cubemap);
        }

        AZ::Render::ExposureControlSettingsInterface* exposureSettings = nullptr;
        if (m_postProcessFeatureProcessor != nullptr)
        {
            AZ::Render::PostProcessSettingsInterface* settings =
                m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_postSettingsEntityId);
            if (settings != nullptr)
            {
                exposureSettings = settings->GetOrCreateExposureControlSettingsInterface();
            }
        }

        Camera::Configuration cameraConfig;
        Camera::ActiveCameraRequestBus::BroadcastResult(
            cameraConfig, &Camera::ActiveCameraRequests::GetActiveCameraConfiguration);

        g_environmentPreset.ApplyLightingPreset(
            m_iblFeatureProcessor, m_skyboxFeatureProcessor, exposureSettings, m_directionalLightFeatureProcessor,
            cameraConfig, m_presetLightHandles, false);

        if (exposureSettings != nullptr)
        {
            // R1: Block 26D clipped the sunlit metallic deck to white (mean luminance ~206 with
            // large pinned-white regions). The blouberg preset carries no exposure config, so apply
            // a firm manual stop-down derived from that evidence -- deterministic and independent of
            // any histogram / eye-adaptation pipeline pass.
            exposureSettings->SetEnabled(true);
            exposureSettings->SetExposureControlType(
                AZ::Render::ExposureControl::ExposureControlType::ManualOnly);
            exposureSettings->SetManualCompensation(GetExposureCompensationTrim());
            exposureSettings->OnConfigChanged();
            m_postProcessFeatureProcessor->OnPostProcessSettingsChanged();
        }

        m_lightingPresetApplied = true;
    }

    void EnvironmentPresentation::ApplyPostProcess()
    {
        if (m_postProcessFeatureProcessor == nullptr)
        {
            return;
        }
        AZ::Render::PostProcessSettingsInterface* settings =
            m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_postSettingsEntityId);
        if (settings == nullptr)
        {
            return;
        }

        if (AZ::Render::SsaoSettingsInterface* ssao = settings->GetOrCreateSsaoSettingsInterface())
        {
            ssao->SetEnabled(true);
            ssao->SetStrength(GetSsaoStrength());
            ssao->SetSamplingRadius(GetSsaoSamplingRadius());
            ssao->SetEnableBlur(true);
            ssao->OnConfigChanged();
        }

        if (AZ::Render::BloomSettingsInterface* bloom = settings->GetOrCreateBloomSettingsInterface())
        {
            bloom->SetEnabled(true);
            bloom->SetThreshold(GetBloomThreshold());
            bloom->SetIntensity(GetBloomIntensity());
            bloom->SetBicubicEnabled(false); // R1: cheaper upsample; the bloom lift is already subtle
            bloom->OnConfigChanged();
        }

        if (AZ::Render::HDRColorGradingSettingsInterface* grading = settings->GetOrCreateHDRColorGradingSettingsInterface())
        {
            grading->SetEnabled(true);
            grading->SetColorGradingContrast(GetColorGradingContrast());
            grading->SetColorGradingPostSaturation(GetColorGradingPostSaturation());
            grading->OnConfigChanged();
        }

        // Leaving m_perViewBlendWeights empty makes these the scene-global level
        // settings, applied to the main view (see PostProcessFeatureProcessor::Simulate).
        settings->OnConfigChanged();
        m_postProcessFeatureProcessor->OnPostProcessSettingsChanged();
        m_postProcessApplied = true;
    }

    void EnvironmentPresentation::ApplyAccentRig()
    {
        if (m_pointLightFeatureProcessor == nullptr)
        {
            // Not fatal: the HDRI key/fill already lights the scene.
            m_accentRigApplied = true;
            return;
        }

        const AZStd::array<AccentLightSpec, AccentLightCount> rig = GetAccentLightRig();
        for (size_t index = 0; index < AccentLightCount; ++index)
        {
            if (m_accentLightHandles[index].IsValid())
            {
                continue;
            }
            const AccentLightSpec& spec = rig[index];
            AZ::Render::PointLightFeatureProcessorInterface::LightHandle handle =
                m_pointLightFeatureProcessor->AcquireLight();
            if (!handle.IsValid())
            {
                continue;
            }
            const AZ::Render::PhotometricColor<AZ::Render::PointLightFeatureProcessorInterface::PhotometricUnitType>
                intensity(spec.m_color * spec.m_candela);
            m_pointLightFeatureProcessor->SetPosition(handle, spec.m_position);
            m_pointLightFeatureProcessor->SetRgbIntensity(handle, intensity);
            m_pointLightFeatureProcessor->SetAttenuationRadius(handle, spec.m_attenuationRadiusMeters);
            m_pointLightFeatureProcessor->SetBulbRadius(handle, spec.m_bulbRadiusMeters);
            m_accentLightHandles[index] = handle;
        }
        m_accentRigApplied = true;
    }

    void EnvironmentPresentation::NeutraliseDefaultLevelVisuals()
    {
        if (m_shaderBallHidden)
        {
            return;
        }
        AZ::ComponentApplicationBus::Broadcast(
            [this](AZ::ComponentApplicationRequests* application)
            {
                if (application == nullptr)
                {
                    return;
                }
                application->EnumerateEntities(
                    [this](AZ::Entity* entity)
                    {
                        if (entity == nullptr || m_shaderBallHidden)
                        {
                            return;
                        }
                        if (entity->GetName() == "Shader Ball"
                            && AZ::Render::MeshComponentRequestBus::FindFirstHandler(entity->GetId()) != nullptr)
                        {
                            AZ::Render::MeshComponentRequestBus::Event(
                                entity->GetId(), &AZ::Render::MeshComponentRequestBus::Events::SetVisibility, false);
                            m_shaderBallHidden = true;
                        }
                    });
            });
    }

    bool EnvironmentPresentation::IsReady() const
    {
        return m_initialized && m_lightingPresetApplied && m_postProcessApplied && m_accentRigApplied;
    }

    void EnvironmentPresentation::Shutdown()
    {
        if (m_directionalLightFeatureProcessor != nullptr)
        {
            for (auto& handle : m_presetLightHandles)
            {
                if (handle.IsValid())
                {
                    m_directionalLightFeatureProcessor->ReleaseLight(handle);
                }
            }
        }
        m_presetLightHandles.clear();

        if (m_pointLightFeatureProcessor != nullptr)
        {
            for (auto& handle : m_accentLightHandles)
            {
                if (handle.IsValid())
                {
                    m_pointLightFeatureProcessor->ReleaseLight(handle);
                }
            }
        }
        m_accentLightHandles = {};

        if (m_postProcessFeatureProcessor != nullptr && m_postSettingsEntityId.IsValid())
        {
            m_postProcessFeatureProcessor->RemoveSettingsInterface(m_postSettingsEntityId);
        }

        m_iblFeatureProcessor = nullptr;
        m_skyboxFeatureProcessor = nullptr;
        m_postProcessFeatureProcessor = nullptr;
        m_pointLightFeatureProcessor = nullptr;
        m_directionalLightFeatureProcessor = nullptr;
        m_lightingPresetAsset = {};
        m_lightingPresetRequested = false;
        m_usingFallbackPreset = false;
        m_postSettingsEntityId = AZ::EntityId();
        m_initialized = false;
        m_lightingPresetApplied = false;
        m_postProcessApplied = false;
        m_accentRigApplied = false;
        m_shaderBallHidden = false;
        m_readyReported = false;
        g_environmentPresetValid = false;
        g_environmentPreset = AZ::Render::LightingPreset{};
    }
}
