#include <gtest/gtest.h>

#include <STWGameplay/ArenaPresentation.h>
#include <STWGameplay/EnvironmentPresentation.h>

namespace STWGameplay
{
    TEST(EnvironmentPresentationTests, DrivesACachedHdriLightingPreset)
    {
        const AZStd::string presetPath = EnvironmentPresentation::GetLightingPresetPath();
        EXPECT_NE(presetPath.find("lightingpreset.azasset"), AZStd::string::npos);
        EXPECT_NE(presetPath.find("lightingpresets/"), AZStd::string::npos);

        const AZStd::string fallbackPath = EnvironmentPresentation::GetFallbackLightingPresetPath();
        EXPECT_NE(fallbackPath.find("lightingpreset.azasset"), AZStd::string::npos);
        EXPECT_NE(presetPath, fallbackPath);
    }

    TEST(EnvironmentPresentationTests, AccentRigIsRestrainedAndPhysicallyPlausible)
    {
        const auto rig = EnvironmentPresentation::GetAccentLightRig();
        EXPECT_EQ(rig.size(), EnvironmentPresentation::AccentLightCount);
        EXPECT_TRUE(EnvironmentPresentation::IsAccentRigPhysicallyPlausible(rig));

        // The accents must not overpower a sun-scale key light.
        for (const auto& spec : rig)
        {
            EXPECT_GT(spec.m_candela, 0.0f);
            EXPECT_LE(spec.m_candela, 2000.0f);
            EXPECT_GT(spec.m_attenuationRadiusMeters, spec.m_bulbRadiusMeters);
        }
    }

    TEST(EnvironmentPresentationTests, AccentLightsNeverOverpowerTheKeyLight)
    {
        // A point light of intensity I at height h lights the floor beneath it with I / h^2 lux.
        // Accents that outshine the sun blow the deck to white whatever the sun is set to.
        const float sunLux = ArenaPresentation::GetSunIlluminanceLux();
        for (const auto& spec : EnvironmentPresentation::GetAccentLightRig())
        {
            ASSERT_GT(spec.m_position.GetZ(), 0.5f);
            const float floorLux = spec.m_candela / (spec.m_position.GetZ() * spec.m_position.GetZ());
            EXPECT_LT(floorLux, sunLux);
        }
    }

    TEST(EnvironmentPresentationTests, IblExposureTrimIsAModestStopDown)
    {
        // At most three stops down and never brighter than the preset; a deeper cut would black out the
        // ambient fill and make shadowed areas unreadable.
        EXPECT_LE(EnvironmentPresentation::GetIblExposureTrim(), 0.0f);
        EXPECT_GE(EnvironmentPresentation::GetIblExposureTrim(), -3.0f);
    }

    TEST(EnvironmentPresentationTests, AccentRigRejectsImplausibleIntensities)
    {
        auto rig = EnvironmentPresentation::GetAccentLightRig();
        rig[0].m_candela = 250000.0f; // sun-bright interior practical is not plausible
        EXPECT_FALSE(EnvironmentPresentation::IsAccentRigPhysicallyPlausible(rig));

        rig = EnvironmentPresentation::GetAccentLightRig();
        rig[1].m_bulbRadiusMeters = rig[1].m_attenuationRadiusMeters + 1.0f;
        EXPECT_FALSE(EnvironmentPresentation::IsAccentRigPhysicallyPlausible(rig));
    }

    TEST(EnvironmentPresentationTests, PostProcessTuningIsSubtleAndBounded)
    {
        // SSAO present but not crushing.
        EXPECT_GT(EnvironmentPresentation::GetSsaoStrength(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetSsaoStrength(), 2.0f);
        EXPECT_GT(EnvironmentPresentation::GetSsaoSamplingRadius(), 0.0f);

        // Bloom is a restrained highlight lift, not a glow bath.
        EXPECT_GT(EnvironmentPresentation::GetBloomIntensity(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetBloomIntensity(), 0.25f);
        EXPECT_GE(EnvironmentPresentation::GetBloomThreshold(), 1.0f);

        // Colour grade is a gentle contrast/saturation nudge.
        EXPECT_GT(EnvironmentPresentation::GetColorGradingContrast(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetColorGradingContrast(), 0.35f);
        EXPECT_GE(EnvironmentPresentation::GetColorGradingPostSaturation(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetColorGradingPostSaturation(), 0.35f);

        // Exposure trim pulls the over-lit deck down, never up.
        EXPECT_LT(EnvironmentPresentation::GetExposureCompensationTrim(), 0.0f);
        EXPECT_GT(EnvironmentPresentation::GetExposureCompensationTrim(), -3.0f);
    }

    TEST(EnvironmentPresentationTests, LensStackIsVisibleButRestrained)
    {
        // Every Atom lens slider is 0..1; the look must be present but never dominant.
        EXPECT_GT(EnvironmentPresentation::GetVignetteIntensity(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetVignetteIntensity(), 0.40f);

        // Atom's own default strength is 0.01; stay within 2x so the fringing stays subtle.
        EXPECT_GT(EnvironmentPresentation::GetChromaticAberrationStrength(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetChromaticAberrationStrength(), 0.02f);
        EXPECT_GE(EnvironmentPresentation::GetChromaticAberrationBlend(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetChromaticAberrationBlend(), 1.0f);

        // Grain must sit below Atom's 0.2 default so it never masks distant targets.
        EXPECT_GT(EnvironmentPresentation::GetFilmGrainIntensity(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetFilmGrainIntensity(), 0.15f);
        EXPECT_GE(EnvironmentPresentation::GetFilmGrainLuminanceDampening(), 0.0f);
        EXPECT_LE(EnvironmentPresentation::GetFilmGrainLuminanceDampening(), 1.0f);
    }

    TEST(EnvironmentPresentationTests, DepthOfFieldApertureIsSpecifiedAsARealFNumber)
    {
        // Photographic, shallow-but-legible: not wide open, not effectively pinhole.
        EXPECT_GE(EnvironmentPresentation::GetDepthOfFieldFNumber(), 2.8f);
        EXPECT_LE(EnvironmentPresentation::GetDepthOfFieldFNumber(), 8.0f);

        // The Atom slider is normalised to 0..1 and must round-trip to the requested f-number.
        const float aperture = EnvironmentPresentation::GetDepthOfFieldApertureF();
        EXPECT_GT(aperture, 0.0f);
        EXPECT_LT(aperture, 1.0f);
        EXPECT_NEAR(
            EnvironmentPresentation::FNumberForApertureF(aperture), EnvironmentPresentation::GetDepthOfFieldFNumber(), 0.01f);
    }

    TEST(EnvironmentPresentationTests, ApertureConversionIsMonotonicAndClamped)
    {
        // A wider aperture (smaller f-number) must map to a larger slider value.
        EXPECT_GT(EnvironmentPresentation::ApertureFForFNumber(2.0f), EnvironmentPresentation::ApertureFForFNumber(4.0f));
        EXPECT_GT(EnvironmentPresentation::ApertureFForFNumber(4.0f), EnvironmentPresentation::ApertureFForFNumber(8.0f));

        // Out-of-range requests clamp into the engine's 0..1 slider instead of escaping it.
        EXPECT_LE(EnvironmentPresentation::ApertureFForFNumber(0.001f), 1.0f);
        EXPECT_GE(EnvironmentPresentation::ApertureFForFNumber(100000.0f), 0.0f);
        EXPECT_LE(EnvironmentPresentation::FNumberForApertureF(-1.0f), 256.0f);
        EXPECT_GE(EnvironmentPresentation::FNumberForApertureF(2.0f), 0.12f);
    }
}
