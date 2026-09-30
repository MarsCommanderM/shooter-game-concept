#pragma once

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

#include <cstddef>

namespace STWGameplay
{
    //! One row of the machine-readable performance evidence contract.
    //! The model deliberately contains no engine, file, or environment API.
    struct PerformanceTelemetrySample
    {
        double m_timeSeconds = 0.0;
        double m_frameMilliseconds = 0.0;
        double m_cpuMilliseconds = 0.0;
        double m_gpuMilliseconds = 0.0;
        double m_drawCalls = 0.0;
        double m_vramMiB = 0.0;
        double m_ramMiB = 0.0;
    };

    struct PerformanceTelemetrySummary
    {
        bool m_valid = false;
        AZStd::string m_error;
        size_t m_postWarmupSamples = 0;
        double m_durationSeconds = 0.0;
        double m_frameP50Milliseconds = 0.0;
        double m_frameP95Milliseconds = 0.0;
        double m_frameP99Milliseconds = 0.0;
        double m_cpuP95Milliseconds = 0.0;
        double m_gpuP95Milliseconds = 0.0;
        double m_drawCallsPeak = 0.0;
        double m_vramMiBPeak = 0.0;
        double m_ramMiBPeak = 0.0;
    };

    //! Engine-free collector for the Visual Forge performance CSV contract.
    //! Validation intentionally mirrors stw-o3de/Tools/visual_forge/forge.py
    //! scene_gate(): warmup filtering, nearest-rank percentiles, positive
    //! finite telemetry, and contiguous frame-time accounting.
    class PerformanceTelemetryModel
    {
    public:
        static constexpr double DefaultWarmupSeconds = 30.0;
        static constexpr double DefaultSampleSeconds = 60.0;
        static constexpr size_t DefaultMinimumSamples = 600;

        void AddSample(
            double timeSeconds,
            double frameMilliseconds,
            double cpuMilliseconds,
            double gpuMilliseconds,
            double drawCalls,
            double vramMiB,
            double ramMiB);

        PerformanceTelemetrySummary Validate(
            double warmupSeconds = DefaultWarmupSeconds,
            double sampleSeconds = DefaultSampleSeconds,
            size_t minimumSamples = DefaultMinimumSamples) const;

        //! Replaces output with the exact Forge CSV header and all collected rows.
        void WriteCsv(AZStd::string& output) const;

        const AZStd::vector<PerformanceTelemetrySample>& GetSamples() const { return m_samples; }

    private:
        AZStd::vector<PerformanceTelemetrySample> m_samples;
    };
} // namespace STWGameplay
