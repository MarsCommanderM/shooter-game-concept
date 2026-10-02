#include <gtest/gtest.h>

#include <STWGameplay/ArenaPresentation.h>
#include <STWGameplay/EnvironmentPresentation.h>
#include <STWGameplay/LightingRecipe.h>

namespace STWGameplay
{
    TEST(LightingRecipeTests, DayPhysicalTargetIsNotTheAtomSun)
    {
        const LightingRecipe day = LightingRecipeSet::Get(LightingRecipeId::Day);
        EXPECT_FLOAT_EQ(day.m_physicalSunLux, LightingRecipeSet::DayPhysicalSunLux);
        EXPECT_FLOAT_EQ(day.m_exposureEv100, 15.0f);
        EXPECT_NE(day.m_physicalSunLux, LightingRecipeSet::EffectiveAtomLux(LightingRecipeId::Day));
        EXPECT_FLOAT_EQ(
            LightingRecipeSet::EffectiveAtomLux(LightingRecipeId::Day),
            LightingRecipeSet::ExposureRelativeAtomLux(LightingRecipeId::Day));
    }

    TEST(LightingRecipeTests, DayEffectiveLuxIsTheMeasuredAtomSun)
    {
        EXPECT_FLOAT_EQ(
            LightingRecipeSet::ExposureRelativeAtomLux(LightingRecipeId::Day),
            LightingRecipeSet::MeasuredSafeAtomDayLux);
        EXPECT_FLOAT_EQ(ArenaPresentation::GetSunIlluminanceLux(), LightingRecipeSet::MeasuredSafeAtomDayLux);
        EXPECT_EQ(LightingRecipeSet::GetBoundRecipe(), LightingRecipeId::Day);
        EXPECT_TRUE(LightingRecipeSet::IsSafeForCurrentDirectionalLight(LightingRecipeId::Day));
        EXPECT_FLOAT_EQ(LightingRecipeSet::AccentScale(LightingRecipeId::Day), 1.0f);
        EXPECT_FLOAT_EQ(LightingRecipeSet::IlluminanceStopDeltaFromDay(LightingRecipeId::Day), 0.0f);
        EXPECT_FALSE(LightingRecipeSet::AppliesManualExposure());
    }

    TEST(LightingRecipeTests, ExposureFoldMakesOvercastSafeAndKeepsNightBelowTheUnscaledBand)
    {
        const float day = LightingRecipeSet::ExposureRelativeAtomLux(LightingRecipeId::Day);
        const float overcast = LightingRecipeSet::ExposureRelativeAtomLux(LightingRecipeId::Overcast);
        const float night = LightingRecipeSet::ExposureRelativeAtomLux(LightingRecipeId::Night);
        EXPECT_GT(day, overcast);
        EXPECT_GT(overcast, night);
        EXPECT_GT(night, 0.0f);
        EXPECT_GE(overcast, LightingRecipeSet::SafeAtomLuxMinimum);
        EXPECT_LE(overcast, LightingRecipeSet::SafeAtomLuxMaximum);
        EXPECT_LT(night, LightingRecipeSet::SafeAtomLuxMinimum);
        EXPECT_TRUE(LightingRecipeSet::IsSafeForCurrentDirectionalLight(LightingRecipeId::Overcast));
        EXPECT_FALSE(LightingRecipeSet::IsSafeForCurrentDirectionalLight(LightingRecipeId::Night));
        EXPECT_NE(LightingRecipeSet::GetBoundRecipe(), LightingRecipeId::Night);
        EXPECT_LT(
            LightingRecipeSet::IlluminanceStopDeltaFromDay(LightingRecipeId::Night),
            LightingRecipeSet::IlluminanceStopDeltaFromDay(LightingRecipeId::Overcast));
        EXPECT_LT(
            LightingRecipeSet::IlluminanceStopDeltaFromDay(LightingRecipeId::Overcast),
            LightingRecipeSet::IlluminanceStopDeltaFromDay(LightingRecipeId::Day));
    }

    TEST(LightingRecipeTests, ScaledAccentsStayUnderEachRecipeSun)
    {
        const LightingRecipeId recipes[] = {
            LightingRecipeId::Day, LightingRecipeId::Night, LightingRecipeId::Overcast
        };
        for (const LightingRecipeId recipe : recipes)
        {
            const float sun = LightingRecipeSet::ExposureRelativeAtomLux(recipe);
            const float scale = LightingRecipeSet::AccentScale(recipe);
            EXPECT_GT(scale, 0.0f);
            for (const EnvironmentPresentation::AccentLightSpec& spec : EnvironmentPresentation::GetAccentLightRig())
            {
                const float floorLux = (spec.m_candela * scale) / (spec.m_position.GetZ() * spec.m_position.GetZ());
                EXPECT_LT(floorLux, sun);
            }
        }
    }

    TEST(LightingRecipeTests, CaptureSequenceCyclesDayNightOvercast)
    {
        EXPECT_EQ(LightingRecipeSet::RecipeAtSequenceIndex(0), LightingRecipeId::Day);
        EXPECT_EQ(LightingRecipeSet::RecipeAtSequenceIndex(1), LightingRecipeId::Night);
        EXPECT_EQ(LightingRecipeSet::RecipeAtSequenceIndex(2), LightingRecipeId::Overcast);
        EXPECT_EQ(LightingRecipeSet::RecipeAtSequenceIndex(3), LightingRecipeId::Day);
        EXPECT_STREQ(LightingRecipeSet::Name(LightingRecipeId::Night), "Night");
    }
}
