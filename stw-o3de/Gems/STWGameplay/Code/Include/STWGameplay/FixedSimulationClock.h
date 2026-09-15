#pragma once

#include <AzCore/base.h>

#include <cmath>
#include <limits>

namespace STWGameplay
{
    struct FixedSimulationAdvanceResult final
    {
        AZ::u32 m_stepCount = 0;
        bool m_inputValid = false;
        bool m_catchUpClamped = false;
    };

    //! Owns fixed-step scheduling state without owning gameplay, physics, or presentation state.
    //!
    //! Advance() reports how many fixed simulation steps are available and advances the
    //! simulation-tick identity by that same amount. The caller is responsible for executing the
    //! reported steps. A bounded catch-up policy prevents an unusually large frame from creating
    //! an unbounded hot-loop; discarded time is exposed through GetDroppedSimulationTime().
    class FixedSimulationClock final
    {
    public:
        // Matches AzPhysics::SystemConfiguration::DefaultFixedTimestep in the checked-out O3DE
        // engine (0.0166667f, documented there as 1/60th). PhysX retains ownership of scene ticks.
        static constexpr float FixedDeltaTime = 0.0166667f;
        static constexpr AZ::u32 MaxSubstepsPerFrame = 8;
        static constexpr float MaximumFrameDelta = FixedDeltaTime * static_cast<float>(MaxSubstepsPerFrame);

        FixedSimulationAdvanceResult Advance(float frameDelta)
        {
            FixedSimulationAdvanceResult result;
            m_catchUpClamped = false;

            if (!std::isfinite(frameDelta) || frameDelta < 0.0f)
            {
                return result;
            }

            result.m_inputValid = true;
            float acceptedFrameDelta = frameDelta;
            if (acceptedFrameDelta > MaximumFrameDelta)
            {
                AddDroppedSimulationTime(acceptedFrameDelta - MaximumFrameDelta);
                acceptedFrameDelta = MaximumFrameDelta;
                m_catchUpClamped = true;
            }

            float accumulatedTime = m_accumulator + acceptedFrameDelta;
            if (!std::isfinite(accumulatedTime))
            {
                AddDroppedSimulationTime(acceptedFrameDelta);
                accumulatedTime = MaximumFrameDelta;
                m_catchUpClamped = true;
            }
            else if (accumulatedTime > MaximumFrameDelta)
            {
                AddDroppedSimulationTime(accumulatedTime - MaximumFrameDelta);
                accumulatedTime = MaximumFrameDelta;
                m_catchUpClamped = true;
            }
            m_accumulator = accumulatedTime;

            const AZ::u32 availableSteps = static_cast<AZ::u32>(m_accumulator / FixedDeltaTime);
            result.m_stepCount = availableSteps > MaxSubstepsPerFrame ? MaxSubstepsPerFrame : availableSteps;
            m_accumulator -= static_cast<float>(result.m_stepCount) * FixedDeltaTime;
            if (m_accumulator < 0.0f)
            {
                m_accumulator = 0.0f;
            }
            if (m_accumulator >= FixedDeltaTime)
            {
                AddDroppedSimulationTime(m_accumulator - std::nextafter(FixedDeltaTime, 0.0f));
                m_accumulator = 0.0f;
                m_catchUpClamped = true;
            }

            m_simulationTick += result.m_stepCount;
            result.m_catchUpClamped = m_catchUpClamped;
            return result;
        }

        float GetInterpolationAlpha() const
        {
            if (!std::isfinite(m_accumulator) || m_accumulator <= 0.0f)
            {
                return 0.0f;
            }
            const float alpha = m_accumulator / FixedDeltaTime;
            if (!std::isfinite(alpha) || alpha <= 1.0e-6f)
            {
                return 0.0f;
            }
            return alpha >= 1.0f ? 1.0f : alpha;
        }

        float GetAccumulator() const { return m_accumulator; }
        AZ::u64 GetSimulationTick() const { return m_simulationTick; }
        float GetDroppedSimulationTime() const { return m_droppedSimulationTime; }
        bool WasCatchUpClamped() const { return m_catchUpClamped; }

        void Reset()
        {
            m_accumulator = 0.0f;
            m_simulationTick = 0;
            m_droppedSimulationTime = 0.0f;
            m_catchUpClamped = false;
        }

    private:
        void AddDroppedSimulationTime(float amount)
        {
            if (!std::isfinite(amount) || amount <= 0.0f)
            {
                return;
            }
            const float maximum = std::numeric_limits<float>::max();
            m_droppedSimulationTime = amount >= maximum - m_droppedSimulationTime
                ? maximum : m_droppedSimulationTime + amount;
        }

        float m_accumulator = 0.0f;
        AZ::u64 m_simulationTick = 0;
        float m_droppedSimulationTime = 0.0f;
        bool m_catchUpClamped = false;
    };
}
