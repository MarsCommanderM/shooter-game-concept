#include <gtest/gtest.h>

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
}
