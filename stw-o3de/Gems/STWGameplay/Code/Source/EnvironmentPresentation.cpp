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
#include <Atom/Feature/PostProcess/ChromaticAberration/ChromaticAberrationSettingsInterface.h>
#include <Atom/Feature/PostProcess/DepthOfField/DepthOfFieldSettingsInterface.h>
#include <Atom/Feature/PostProcess/FilmGrain/FilmGrainSettingsInterface.h>
#include <Atom/Feature/PostProcess/Vignette/VignetteSettingsInterface.h>
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
        // player spawn. Intensities are calibrated against the key light: a light at height h
        // lights the floor beneath it with candela / h^2 lux, which must stay below the sun's
        // lux (see ArenaPresentation::GetSunIlluminanceLux). At 850 cd these lit the deck at
        // ~65 lux each and blew it to flat white regardless of the sun.
        AZStd::array<AccentLightSpec, AccentLightCount> rig;
        // Cinematic step 9: with real sun shadows and a closed roof the hall is ambient-lit (mean luma 68, RMS 49), so the
        // accents become the practical lights that shape it. Still below the sun at the floor (16.2 / 16.2 / 20.8 lux < 25).
        rig[0] = AccentLightSpec{ AZ::Vector3(-5.0f, 3.0f, 3.6f), AZ::Color(0.62f, 0.74f, 1.0f, 1.0f), 210.0f, 16.0f, 0.20f };
        rig[1] = AccentLightSpec{ AZ::Vector3(5.0f, 3.0f, 3.6f), AZ::Color(0.62f, 0.74f, 1.0f, 1.0f), 210.0f, 16.0f, 0.20f };
        rig[2] = AccentLightSpec{ AZ::Vector3(0.0f, -8.0f, 2.4f), AZ::Color(1.0f, 0.82f, 0.60f, 1.0f), 120.0f, 12.0f, 0.15f };
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
    // Cinematic step 2: the deck is lit mostly by the HDRI ambient (measured: lowering the sun 25->15 lux
    // only moved the floor 188->170). One stop less IBL lets the key and the accents shape the image.
    float EnvironmentPresentation::GetIblExposureTrim() { return -1.0f; }

    // Cinematic lens stack. Atom's own defaults are the reference points: vignette 0.01,
    // chromatic aberration 0.01 / blend 0.5, film grain 0.2. The values below are a
    // deliberate, restrained look for a first-person view where target legibility matters.
    float EnvironmentPresentation::GetVignetteIntensity() { return 0.25f; }
    float EnvironmentPresentation::GetChromaticAberrationStrength() { return 0.006f; }
    float EnvironmentPresentation::GetChromaticAberrationBlend() { return 0.5f; }
    float EnvironmentPresentation::GetFilmGrainIntensity() { return 0.06f; }
    float EnvironmentPresentation::GetFilmGrainLuminanceDampening() { return 0.5f; }
    // f/4 keeps a soft, photographic falloff without smearing the viewmodel or distant threats.
    float EnvironmentPresentation::GetDepthOfFieldFNumber() { return 4.0f; }

    float EnvironmentPresentation::ApertureFForFNumber(float fNumber)
    {
        // Inverse of DepthOfFieldSettings::UpdateFNumber(): ApertureF in [0, 1] maps linearly
        // onto the inverse f-number between 1/ApertureFMax and 1/ApertureFMin.
        constexpr float MinF = AZ::Render::DepthOfField::ApertureFMin;
        constexpr float MaxF = AZ::Render::DepthOfField::ApertureFMax;
        const float clamped = AZStd::clamp(fNumber, MinF, MaxF);
        return (1.0f / clamped - 1.0f / MaxF) / (1.0f / MinF - 1.0f / MaxF);
    }

    float EnvironmentPresentation::FNumberForApertureF(float apertureF)
    {
        constexpr float MinF = AZ::Render::DepthOfField::ApertureFMin;
        constexpr float MaxF = AZ::Render::DepthOfField::ApertureFMax;
        const float clamped = AZStd::clamp(apertureF, 0.0f, 1.0f);
        // Float rounding can land a hair outside the engine's range at the slider ends.
        return AZStd::clamp(1.0f / (1.0f / MaxF + (1.0f / MinF - 1.0f / MaxF) * clamped), MinF, MaxF);
    }

    float EnvironmentPresentation::GetDepthOfFieldApertureF()
    {
        return ApertureFForFNumber(GetDepthOfFieldFNumber());
    }

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

        if (m_postProcessApplied && !m_lensOpticApplied)
        {
            ApplyLensOptic();
        }

        if (m_postProcessApplied && !m_depthOfFieldApplied)
        {
            TryApplyDepthOfField();
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

        if (m_iblFeatureProcessor != nullptr)
        {
            m_iblFeatureProcessor->SetExposure(GetIblExposureTrim());
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
            grading->SetColorGradingContrast(m_colorGradingContrastOverride);
            grading->SetColorGradingPostSaturation(GetColorGradingPostSaturation());
            grading->OnConfigChanged();
        }

        // Leaving m_perViewBlendWeights empty makes these the scene-global level
        // settings, applied to the main view (see PostProcessFeatureProcessor::Simulate).
        settings->OnConfigChanged();
        m_postProcessFeatureProcessor->OnPostProcessSettingsChanged();
        m_postProcessApplied = true;
    }

    void EnvironmentPresentation::SetColorGradingContrastOverride(float value)
    {
        m_colorGradingContrastOverride = value;
        if (m_postProcessFeatureProcessor == nullptr || !m_postSettingsEntityId.IsValid())
        {
            // ApplyPostProcess() will pick up m_colorGradingContrastOverride
            // whenever it does run - this is not a lost write.
            return;
        }
        AZ::Render::PostProcessSettingsInterface* settings =
            m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_postSettingsEntityId);
        if (settings == nullptr)
        {
            return;
        }
        if (AZ::Render::HDRColorGradingSettingsInterface* grading = settings->GetOrCreateHDRColorGradingSettingsInterface())
        {
            grading->SetColorGradingContrast(m_colorGradingContrastOverride);
            grading->OnConfigChanged();
        }
        settings->OnConfigChanged();
        m_postProcessFeatureProcessor->OnPostProcessSettingsChanged();
    }

    void EnvironmentPresentation::ApplyLensOptic()
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

        bool vignetteOk = false;
        bool aberrationOk = false;
        bool grainOk = false;

        if (AZ::Render::VignetteSettingsInterface* vignette = settings->GetOrCreateVignetteSettingsInterface())
        {
            vignette->SetEnabled(true);
            vignette->SetIntensity(GetVignetteIntensity());
            vignette->OnConfigChanged();
            vignetteOk = true;
        }

        if (AZ::Render::ChromaticAberrationSettingsInterface* aberration =
                settings->GetOrCreateChromaticAberrationSettingsInterface())
        {
            aberration->SetEnabled(true);
            aberration->SetStrength(GetChromaticAberrationStrength());
            aberration->SetBlend(GetChromaticAberrationBlend());
            aberration->OnConfigChanged();
            aberrationOk = true;
        }

        if (AZ::Render::FilmGrainSettingsInterface* grain = settings->GetOrCreateFilmGrainSettingsInterface())
        {
            grain->SetEnabled(true);
            grain->SetIntensity(GetFilmGrainIntensity());
            grain->SetLuminanceDampening(GetFilmGrainLuminanceDampening());
            grain->OnConfigChanged();
            grainOk = true;
        }

        settings->OnConfigChanged();
        m_postProcessFeatureProcessor->OnPostProcessSettingsChanged();
        m_lensOpticApplied = vignetteOk && aberrationOk && grainOk;

        if (!m_opticReported)
        {
            m_opticReported = true;
            AZ_Printf(
                "STWGameplay",
                "STW_CINEMATIC_OPTIC lens=%d vignette=%.3f aberration=%.4f grain=%.3f dof_fnumber=%.1f\n",
                m_lensOpticApplied ? 1 : 0, GetVignetteIntensity(), GetChromaticAberrationStrength(),
                GetFilmGrainIntensity(), GetDepthOfFieldFNumber());
        }
    }

    void EnvironmentPresentation::TryApplyDepthOfField()
    {
        if (m_postProcessFeatureProcessor == nullptr)
        {
            return;
        }

        // Atom disables depth of field unless it is bound to a live camera entity, and the
        // gameplay camera is created after the environment layer, so this stays lazy.
        AZ::EntityId cameraId;
        Camera::CameraSystemRequestBus::BroadcastResult(cameraId, &Camera::CameraSystemRequests::GetActiveCamera);
        if (!cameraId.IsValid())
        {
            return;
        }

        AZ::Render::PostProcessSettingsInterface* settings =
            m_postProcessFeatureProcessor->GetOrCreateSettingsInterface(m_postSettingsEntityId);
        if (settings == nullptr)
        {
            return;
        }
        AZ::Render::DepthOfFieldSettingsInterface* dof = settings->GetOrCreateDepthOfFieldSettingsInterface();
        if (dof == nullptr)
        {
            return;
        }

        // SetEnabled() only sticks when the camera entity is already valid, so bind it first.
        dof->SetCameraEntityId(cameraId);
        dof->SetEnabled(true);
        dof->SetApertureF(GetDepthOfFieldApertureF());
        dof->SetEnableAutoFocus(true);
        dof->SetAutoFocusScreenPosition(AZ::Vector2(0.5f, 0.5f));
        dof->SetAutoFocusSensitivity(1.0f);
        dof->SetAutoFocusSpeed(AZ::Render::DepthOfField::AutoFocusSpeedMax);
        dof->SetAutoFocusDelay(0.0f);
        dof->OnConfigChanged();

        settings->OnConfigChanged();
        m_postProcessFeatureProcessor->OnPostProcessSettingsChanged();
        m_depthOfFieldApplied = true;

        AZ_Printf(
            "STWGameplay", "STW_CINEMATIC_OPTIC_DOF=1 fnumber=%.1f aperture_slider=%.4f autofocus=1\n",
            GetDepthOfFieldFNumber(), GetDepthOfFieldApertureF());
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
        m_lensOpticApplied = false;
        m_depthOfFieldApplied = false;
        m_opticReported = false;
        m_accentRigApplied = false;
        m_shaderBallHidden = false;
        m_readyReported = false;
        g_environmentPresetValid = false;
        g_environmentPreset = AZ::Render::LightingPreset{};
    }
}
