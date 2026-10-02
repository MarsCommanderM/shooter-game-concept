#include <gtest/gtest.h>

#include <STWGameplay/ArenaPresentation.h>
#include <STWGameplay/LightingRecipe.h>

namespace STWGameplay
{
    TEST(LightingRecipeTests, DayPhysicalTargetIsTheRejectedRawLux)
    {
        const LightingRecipe day = LightingRecipeSet::Get(LightingRecipeId::Day);
        EXPECT_FLOAT_EQ(day.m_physicalSunLux, 100000.0f);
        EXPECT_FLOAT_EQ(day.m_sunElevationDegrees, 45.0f);
        EXPECT_FLOAT_EQ(day.m_sunAzimuthDegrees, 135.0f);
        EXPECT_FLOAT_EQ(day.m_exposureEv100, 15.0f);
        EXPECT_FLOAT_EQ(day.m_practicalTemperatureK, 3200.0f);
        EXPECT_NE(day.m_physicalSunLux, LightingRecipeSet::EffectiveAtomLux(LightingRecipeId::Day));
    }

    TEST(LightingRecipeTests, DayEffectiveLuxIsTheMeasuredAtomSun)
    {
        EXPECT_FLOAT_EQ(LightingRecipeSet::EffectiveAtomLux(LightingRecipeId::Day), 25.0f);
        EXPECT_FLOAT_EQ(ArenaPresentation::GetSunIlluminanceLux(), 25.0f);
        EXPECT_EQ(LightingRecipeSet::GetBoundRecipe(), LightingRecipeId::Day);
        EXPECT_TRUE(LightingRecipeSet::IsSafeForCurrentDirectionalLight(LightingRecipeId::Day));
    }

    TEST(LightingRecipeTests, NightAndOvercastStayOutsideTheMeasuredBand)
    {
        const LightingRecipe night = LightingRecipeSet::Get(LightingRecipeId::Night);
        const LightingRecipe overcast = LightingRecipeSet::Get(LightingRecipeId::Overcast);
        EXPECT_FLOAT_EQ(night.m_physicalSunLux, 0.2f);
        EXPECT_FLOAT_EQ(night.m_exposureEv100, 1.0f);
        EXPECT_FLOAT_EQ(overcast.m_physicalSunLux, 10000.0f);
        EXPECT_FLOAT_EQ(overcast.m_exposureEv100, 12.0f);

        EXPECT_NEAR(LightingRecipeSet::EffectiveAtomLux(LightingRecipeId::Overcast), 2.5f, 0.0001f);
        EXPECT_NEAR(LightingRecipeSet::EffectiveAtomLux(LightingRecipeId::Night), 0.00005f, 0.0000001f);
        EXPECT_FALSE(LightingRecipeSet::IsSafeForCurrentDirectionalLight(LightingRecipeId::Night));
        EXPECT_FALSE(LightingRecipeSet::IsSafeForCurrentDirectionalLight(LightingRecipeId::Overcast));
        EXPECT_NE(LightingRecipeSet::GetBoundRecipe(), LightingRecipeId::Night);
        EXPECT_NE(LightingRecipeSet::GetBoundRecipe(), LightingRecipeId::Overcast);
    }

    TEST(LightingRecipeTests, ExposureEvIsRecordedAndNotApplied)
    {
        EXPECT_FALSE(LightingRecipeSet::AppliesManualExposure());
        EXPECT_GT(LightingRecipeSet::Get(LightingRecipeId::Day).m_exposureEv100,
            LightingRecipeSet::Get(LightingRecipeId::Night).m_exposureEv100);
    }
}
