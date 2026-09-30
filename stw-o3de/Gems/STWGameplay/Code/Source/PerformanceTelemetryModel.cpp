#include <STWGameplay/PerformanceTelemetryModel.h>

#include <AzCore/std/algorithm.h>

#include <cmath>

namespace STWGameplay
{
    namespace
    {
        constexpr const char* PerformanceTelemetryCsvHeader =
            "time_s,frame_ms,cpu_ms,gpu_ms,draw_calls,vram_mib,ram_mib\n";

        bool PerformanceTelemetrySampleIsFinite(const PerformanceTelemetrySample& sample)
        {
            return std::isfinite(sample.m_timeSeconds)
                && std::isfinite(sample.m_frameMilliseconds)
                && std::isfinite(sample.m_cpuMilliseconds)
                && std::isfinite(sample.m_gpuMilliseconds)
                && std::isfinite(sample.m_drawCalls)
                && std::isfinite(sample.m_vramMiB)
                && std::isfinite(sample.m_ramMiB);
        }

        double PerformanceTelemetryNearestRank(const AZStd::vector<double>& values, double quantile)
        {
            AZStd::vector<double> sortedValues = values;
            AZStd::sort(sortedValues.begin(), sortedValues.end());
            const size_t rank = static_cast<size_t>(std::ceil(sortedValues.size() * quantile));
            const size_t index = rank == 0 ? 0 : rank - 1;
            return sortedValues[index];
        }

        AZStd::string PerformanceTelemetryError(const char* message)
        {
            return AZStd::string(message);
        }
    } // namespace

    void PerformanceTelemetryModel::AddSample(
        double timeSeconds,
        double frameMilliseconds,
        double cpuMilliseconds,
        double gpuMilliseconds,
        double drawCalls,
        double vramMiB,
        double ramMiB)
    {
        m_samples.push_back({
            timeSeconds,
            frameMilliseconds,
            cpuMilliseconds,
            gpuMilliseconds,
            drawCalls,
            vramMiB,
            ramMiB});
    }

    PerformanceTelemetrySummary PerformanceTelemetryModel::Validate(
        double warmupSeconds, double sampleSeconds, size_t minimumSamples) const
    {
        PerformanceTelemetrySummary summary;
        if (m_samples.empty())
        {
            summary.m_error = PerformanceTelemetryError("empty benchmark");
            return summary;
        }

        for (size_t index = 0; index < m_samples.size(); ++index)
        {
            const PerformanceTelemetrySample& sample = m_samples[index];
            if (!PerformanceTelemetrySampleIsFinite(sample))
            {
                summary.m_error = PerformanceTelemetryError("non-finite telemetry");
                return summary;
            }
            if (index > 0 && sample.m_timeSeconds <= m_samples[index - 1].m_timeSeconds)
            {
                summary.m_error = PerformanceTelemetryError("timestamps must increase");
                return summary;
            }
        }

        // This is the same forge.py rule: the first timestamp must start near zero.
        if (m_samples.front().m_timeSeconds > 1.0)
        {
            summary.m_error = PerformanceTelemetryError("timestamps must start near zero");
            return summary;
        }

        AZStd::vector<const PerformanceTelemetrySample*> postWarmup;
        postWarmup.reserve(m_samples.size());
        for (const PerformanceTelemetrySample& sample : m_samples)
        {
            if (sample.m_timeSeconds >= warmupSeconds)
            {
                postWarmup.push_back(&sample);
            }
        }

        summary.m_postWarmupSamples = postWarmup.size();
        if (postWarmup.empty())
        {
            summary.m_error = PerformanceTelemetryError("no post-warmup samples");
            return summary;
        }
        if (postWarmup.size() < minimumSamples)
        {
            summary.m_error = PerformanceTelemetryError("insufficient post-warmup samples");
            return summary;
        }

        const double firstTime = postWarmup.front()->m_timeSeconds;
        const double lastTime = postWarmup.back()->m_timeSeconds;
        summary.m_durationSeconds = lastTime - firstTime;
        if (summary.m_durationSeconds < sampleSeconds)
        {
            summary.m_error = PerformanceTelemetryError("capture too short");
            return summary;
        }

        AZStd::vector<double> frameMilliseconds;
        AZStd::vector<double> cpuMilliseconds;
        AZStd::vector<double> gpuMilliseconds;
        frameMilliseconds.reserve(postWarmup.size());
        cpuMilliseconds.reserve(postWarmup.size());
        gpuMilliseconds.reserve(postWarmup.size());
        double frameDurationSeconds = 0.0;
        for (size_t index = 0; index < postWarmup.size(); ++index)
        {
            const PerformanceTelemetrySample& sample = *postWarmup[index];
            if (sample.m_frameMilliseconds <= 0.0
                || sample.m_cpuMilliseconds <= 0.0
                || sample.m_gpuMilliseconds <= 0.0
                || sample.m_drawCalls <= 0.0
                || sample.m_vramMiB <= 0.0
                || sample.m_ramMiB <= 0.0)
            {
                summary.m_error = PerformanceTelemetryError("missing/zero telemetry");
                return summary;
            }
            frameMilliseconds.push_back(sample.m_frameMilliseconds);
            cpuMilliseconds.push_back(sample.m_cpuMilliseconds);
            gpuMilliseconds.push_back(sample.m_gpuMilliseconds);
            if (index > 0)
            {
                frameDurationSeconds += sample.m_frameMilliseconds / 1000.0;
            }
        }

        if (std::abs(frameDurationSeconds - summary.m_durationSeconds) > summary.m_durationSeconds * 0.05)
        {
            summary.m_error = PerformanceTelemetryError("CSV must contain contiguous per-frame telemetry");
            return summary;
        }

        summary.m_frameP50Milliseconds = PerformanceTelemetryNearestRank(frameMilliseconds, 0.50);
        summary.m_frameP95Milliseconds = PerformanceTelemetryNearestRank(frameMilliseconds, 0.95);
        summary.m_frameP99Milliseconds = PerformanceTelemetryNearestRank(frameMilliseconds, 0.99);
        summary.m_cpuP95Milliseconds = PerformanceTelemetryNearestRank(cpuMilliseconds, 0.95);
        summary.m_gpuP95Milliseconds = PerformanceTelemetryNearestRank(gpuMilliseconds, 0.95);
        for (const PerformanceTelemetrySample* sample : postWarmup)
        {
            summary.m_drawCallsPeak = AZStd::max(summary.m_drawCallsPeak, sample->m_drawCalls);
            summary.m_vramMiBPeak = AZStd::max(summary.m_vramMiBPeak, sample->m_vramMiB);
            summary.m_ramMiBPeak = AZStd::max(summary.m_ramMiBPeak, sample->m_ramMiB);
        }

        summary.m_valid = true;
        return summary;
    }

    void PerformanceTelemetryModel::WriteCsv(AZStd::string& output) const
    {
        output.clear();
        output += PerformanceTelemetryCsvHeader;
        for (const PerformanceTelemetrySample& sample : m_samples)
        {
            output += AZStd::string::format(
                "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
                sample.m_timeSeconds,
                sample.m_frameMilliseconds,
                sample.m_cpuMilliseconds,
                sample.m_gpuMilliseconds,
                sample.m_drawCalls,
                sample.m_vramMiB,
                sample.m_ramMiB);
        }
    }
} // namespace STWGameplay
