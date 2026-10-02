#pragma once

#include <cmath>

namespace STWGameplay
{
    enum class LightingRecipeId
    {
        Day = 0,
        Night,
        Overcast
    };

    struct LightingRecipe
    {
        float m_physicalSunLux = 0.0f;
        float m_sunElevationDegrees = 0.0f;
        float m_sunAzimuthDegrees = 0.0f;
        float m_exposureEv100 = 0.0f;
        float m_practicalTemperatureK = 0.0f;
    };

    //! Physical targets from Config/VisualForge/lighting_recipes.json.
    //!
    //! A raw 25,000 lux sun clipped 47% of a captured frame. 25 lux clipped 0.1% and is
    //! the working day key. The recipe's 100,000 lux day value stays a physical reference.
    //! Manual exposure compensation does not drive the image, so exposure_ev100 is not
    //! written to Atom's manual slider. The stop difference from the day recipe is folded
    //! into the sun illuminance, which is the value the slice actually responds to.
    //! The default bound recipe remains Day. Night falls below the unscaled accent band
    //! and is applied only with AccentScale during an explicit capture sequence.
    class LightingRecipeSet final
    {
    public:
        static constexpr float MeasuredSafeAtomDayLux = 25.0f;
        static constexpr float DayPhysicalSunLux = 100000.0f;
        static constexpr float AtomLuxPerPhysicalLux = MeasuredSafeAtomDayLux / DayPhysicalSunLux;
        static constexpr float SafeAtomLuxMinimum = 4.0f;
        static constexpr float SafeAtomLuxMaximum = 100.0f;

        static constexpr LightingRecipe Get(LightingRecipeId id)
        {
            switch (id)
            {
            case LightingRecipeId::Day:
                return {100000.0f, 45.0f, 135.0f, 15.0f, 3200.0f};
            case LightingRecipeId::Night:
                return {0.2f, 25.0f, 135.0f, 1.0f, 3200.0f};
            case LightingRecipeId::Overcast:
                return {10000.0f, 45.0f, 135.0f, 12.0f, 3200.0f};
            }
            return {};
        }

        static constexpr float ExposureRatioAgainstDay(LightingRecipeId id)
        {
            const float stops = Get(LightingRecipeId::Day).m_exposureEv100 - Get(id).m_exposureEv100;
            if (stops <= 0.0f)
            {
                return 1.0f;
            }
            float ratio = 1.0f;
            const int wholeStops = static_cast<int>(stops);
            for (int step = 0; step < wholeStops; ++step)
            {
                ratio *= 2.0f;
            }
            return ratio;
        }

        //! Sun lux after folding the recipe's exposure stop gap into illuminance.
        //! Day stays at the measured 25 lux anchor. The physical lux is not returned.
        static constexpr float ExposureRelativeAtomLux(LightingRecipeId id)
        {
            return Get(id).m_physicalSunLux * ExposureRatioAgainstDay(id) * AtomLuxPerPhysicalLux;
        }

        static constexpr float EffectiveAtomLux(LightingRecipeId id)
        {
            return ExposureRelativeAtomLux(id);
        }

        static constexpr float AccentScale(LightingRecipeId id)
        {
            return ExposureRelativeAtomLux(id) / ExposureRelativeAtomLux(LightingRecipeId::Day);
        }

        static float IlluminanceStopDeltaFromDay(LightingRecipeId id)
        {
            const float day = ExposureRelativeAtomLux(LightingRecipeId::Day);
            const float recipe = ExposureRelativeAtomLux(id);
            if (!(day > 0.0f) || !(recipe > 0.0f))
            {
                return 0.0f;
            }
            return std::log2(recipe / day);
        }

        static constexpr bool AppliesManualExposure()
        {
            return false;
        }

        static constexpr bool IsSafeForCurrentDirectionalLight(LightingRecipeId id)
        {
            const float lux = EffectiveAtomLux(id);
            return lux >= SafeAtomLuxMinimum && lux <= SafeAtomLuxMaximum;
        }

        static constexpr LightingRecipeId GetBoundRecipe()
        {
            return LightingRecipeId::Day;
        }

        static constexpr LightingRecipeId RecipeAtSequenceIndex(int index)
        {
            if (index < 0)
            {
                return LightingRecipeId::Day;
            }
            switch (index % 3)
            {
            case 0:
                return LightingRecipeId::Day;
            case 1:
                return LightingRecipeId::Night;
            default:
                return LightingRecipeId::Overcast;
            }
        }

        static constexpr const char* Name(LightingRecipeId id)
        {
            switch (id)
            {
            case LightingRecipeId::Day:
                return "Day";
            case LightingRecipeId::Night:
                return "Night";
            case LightingRecipeId::Overcast:
                return "Overcast";
            }
            return "Day";
        }
    };
}
