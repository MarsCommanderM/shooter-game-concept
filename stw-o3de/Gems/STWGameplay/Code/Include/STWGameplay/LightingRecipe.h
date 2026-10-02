#pragma once

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

    //! Physical targets from Config/VisualForge/lighting_recipes.json, converted to the
    //! Atom illuminance that the Industrial Yard slice has actually held.
    //!
    //! A raw 25,000 lux sun clipped 47% of a captured frame. 25 lux clipped 0.1% and is
    //! the working day key. The recipe's 100,000 lux day value is the physical reference
    //! the 2026-09-25 experiment rejected as a direct Atom setting. Manual exposure
    //! compensation does not drive the image (cinematic interior v2, one stop, no change),
    //! so exposure_ev100 is retained and not applied.
    //!
    //! Night and overcast scale by the same factor and fall outside the 4..100 lux band
    //! the accent rig and the sun test require. They stay unbound until a frame exists.
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

        static constexpr float EffectiveAtomLux(LightingRecipeId id)
        {
            return Get(id).m_physicalSunLux * AtomLuxPerPhysicalLux;
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
    };
}
