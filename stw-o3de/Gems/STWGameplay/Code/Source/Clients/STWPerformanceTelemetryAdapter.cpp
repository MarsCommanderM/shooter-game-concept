#include "STWPerformanceTelemetryAdapter.h"

#include <AzCore/IO/SystemFile.h>
#include <AzCore/Process/ProcessInfo.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/string/string.h>
#include <Atom/RHI/RHIMemoryStatisticsInterface.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Public/Pass/ParentPass.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>

#include <cmath>
#include <cstdlib>

#if !AZ_TRAIT_SERVER
#include <ctime>
#endif

namespace STWGameplay
{
    namespace
    {
        constexpr double STWPerformanceTelemetryWarmupSeconds = 30.0;
        constexpr double STWPerformanceTelemetryWindowSeconds = 60.0;
        constexpr size_t STWPerformanceTelemetryMinimumSamples = 600;
        constexpr double STWPerformanceTelemetryBytesPerMiB = 1024.0 * 1024.0;

#if !AZ_TRAIT_SERVER
        double STWPerformanceTelemetryMainThreadCpuSeconds()
        {
#if defined(__linux__)
            timespec timeSpec{};
            if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &timeSpec) == 0)
            {
                return static_cast<double>(timeSpec.tv_sec) + static_cast<double>(timeSpec.tv_nsec) / 1.0e9;
            }
#endif
            return -1.0;
        }
#endif

#if !AZ_TRAIT_SERVER
        void STWPerformanceTelemetryAccumulatePassTimestamps(
            const AZ::RPI::Pass* pass, AZ::RPI::TimestampResult& extent, bool& hasSample)
        {
            if (!pass)
            {
                return;
            }

            const AZ::RPI::TimestampResult timestamp = pass->GetLatestTimestampResult();
            if (timestamp.GetDurationInTicks() > 0)
            {
                if (hasSample)
                {
                    extent.Add(timestamp);
                }
                else
                {
                    extent = timestamp;
                    hasSample = true;
                }
            }

            if (const auto* parentPass = azrtti_cast<const AZ::RPI::ParentPass*>(pass))
            {
                for (const AZ::RPI::Ptr<AZ::RPI::Pass>& child : parentPass->GetChildren())
                {
                    STWPerformanceTelemetryAccumulatePassTimestamps(child.get(), extent, hasSample);
                }
            }
        }
#endif

        class STWNativePerformanceTelemetryProvider final
            : public STWPerformanceTelemetryProvider
        {
        public:
            bool CollectFrame(STWPerformanceTelemetryFrame& frame) override
            {
#if AZ_TRAIT_SERVER
                AZ_UNUSED(frame);
                return false;
#else
                bool valid = true;

                const double currentCpuSeconds = STWPerformanceTelemetryMainThreadCpuSeconds();
                if (m_lastCpuSeconds >= 0.0 && currentCpuSeconds > m_lastCpuSeconds)
                {
                    frame.m_cpuMilliseconds = (currentCpuSeconds - m_lastCpuSeconds) * 1000.0;
                }
                else
                {
                    valid = false;
                }
                m_lastCpuSeconds = currentCpuSeconds;

                AZ::RPI::PassSystemInterface* passSystem = AZ::RPI::PassSystemInterface::Get();
                const AZ::RPI::Ptr<AZ::RPI::ParentPass> rootPass =
                    passSystem ? passSystem->GetRootPass() : nullptr;
                AZ::RPI::TimestampResult gpuExtent;
                bool hasGpuSample = false;
                if (rootPass)
                {
                    STWPerformanceTelemetryAccumulatePassTimestamps(rootPass.get(), gpuExtent, hasGpuSample);
                }
                if (hasGpuSample && gpuExtent.GetDurationInNanoseconds() > 0)
                {
                    frame.m_gpuMilliseconds =
                        static_cast<double>(gpuExtent.GetDurationInNanoseconds()) / 1.0e6;
                }
                else
                {
                    valid = false;
                }

                if (passSystem)
                {
                    frame.m_drawCalls = passSystem->GetFrameStatistics().m_totalDrawItemsRendered;
                }
                else
                {
                    valid = false;
                }

                if (const AZ::RHI::RHIMemoryStatisticsInterface* memoryInterface =
                        AZ::RHI::RHIMemoryStatisticsInterface::Get())
                {
                    if (const AZ::RHI::MemoryStatistics* memory = memoryInterface->GetMemoryStatistics())
                    {
                        size_t deviceResidentBytes = 0;
                        for (const AZ::RHI::MemoryStatistics::Heap& heap : memory->m_heaps)
                        {
                            if (heap.m_heapMemoryType == AZ::RHI::HeapMemoryLevel::Device)
                            {
                                deviceResidentBytes += heap.m_memoryUsage.m_totalResidentInBytes.load();
                            }
                        }
                        frame.m_vramMiB = static_cast<double>(deviceResidentBytes) / STWPerformanceTelemetryBytesPerMiB;
                    }
                    else
                    {
                        valid = false;
                    }
                }
                else
                {
                    valid = false;
                }

                AZ::ProcessMemInfo processMemory;
                if (AZ::QueryMemInfo(processMemory) && processMemory.m_workingSet > 0)
                {
                    frame.m_ramMiB = static_cast<double>(processMemory.m_workingSet) / STWPerformanceTelemetryBytesPerMiB;
                }
                else
                {
                    valid = false;
                }

                if (const AZ::RHI::RHISystemInterface* rhi = AZ::RHI::RHISystemInterface::Get())
                {
                    frame.m_rhiCpuFrameMilliseconds = rhi->GetCpuFrameTime();
                }

                // The public RHI interface exposes the CPU frame time but does not expose
                // the statistical-profiler Present metric. Keep it explicitly unavailable;
                // never infer Present from frame, CPU, or GPU time.
                frame.m_presentAvailable = false;
                return valid;
#endif
            }

            void Reset()
            {
                m_lastCpuSeconds = -1.0;
            }

        private:
            double m_lastCpuSeconds = -1.0;
        };

        double STWPerformanceTelemetryNearestRank(const AZStd::vector<double>& values, double quantile)
        {
            if (values.empty())
            {
                return 0.0;
            }
            AZStd::vector<double> sortedValues = values;
            AZStd::sort(sortedValues.begin(), sortedValues.end());
            const size_t rank = static_cast<size_t>(std::ceil(sortedValues.size() * quantile));
            return sortedValues[AZStd::max<size_t>(rank, 1) - 1];
        }

        STWNativePerformanceTelemetryProvider s_stwNativePerformanceTelemetryProvider;
    } // namespace

    STWPerformanceTelemetryAdapter::STWPerformanceTelemetryAdapter(STWPerformanceTelemetryProvider* provider)
        : m_provider(provider)
    {
    }

    void STWPerformanceTelemetryAdapter::Activate()
    {
        m_enabled = false;
        m_reported = false;
        m_invalidFrameObserved = false;
        m_elapsedSeconds = 0.0;
        m_postWarmupStartSeconds = -1.0;
        m_csvPath.clear();
        m_rhiCpuFrameMilliseconds.clear();
        m_presentMilliseconds.clear();
        m_model = PerformanceTelemetryModel{};

        const char* enabled = std::getenv("STW_PERF_TELEMETRY");
        const char* csvPath = std::getenv("STW_PERF_TELEMETRY_CSV");
        if (!enabled || enabled[0] != '1' || !csvPath || csvPath[0] == '\0')
        {
            return;
        }

        m_csvPath = csvPath;
        m_enabled = true;
        if (!m_provider)
        {
            s_stwNativePerformanceTelemetryProvider.Reset();
        }

#if !AZ_TRAIT_SERVER
        if (AZ::RPI::PassSystemInterface* passSystem = AZ::RPI::PassSystemInterface::Get())
        {
            if (const AZ::RPI::Ptr<AZ::RPI::ParentPass>& rootPass = passSystem->GetRootPass())
            {
                rootPass->SetTimestampQueryEnabled(true);
            }
        }
#endif
    }

    void STWPerformanceTelemetryAdapter::Deactivate()
    {
        if (m_enabled)
        {
            Finalize();
        }
        m_enabled = false;
    }

    void STWPerformanceTelemetryAdapter::RecordFrame(float deltaTime)
    {
        if (!m_enabled || !std::isfinite(deltaTime) || deltaTime <= 0.0f || m_reported)
        {
            return;
        }

        m_elapsedSeconds += deltaTime;
        STWPerformanceTelemetryFrame frame;
        const bool valid = m_provider
            ? m_provider->CollectFrame(frame)
            : s_stwNativePerformanceTelemetryProvider.CollectFrame(frame);

        // Forge validates the complete timeline before filtering the 30 s warmup.
        // Keep one row per frame from t ~= 0 so the CSV remains gapless and its
        // first timestamp satisfies the same contract as PerformanceTelemetryModel.
        m_model.AddSample(
            m_elapsedSeconds,
            static_cast<double>(deltaTime) * 1000.0,
            frame.m_cpuMilliseconds,
            frame.m_gpuMilliseconds,
            frame.m_drawCalls,
            frame.m_vramMiB,
            frame.m_ramMiB);

        if (m_elapsedSeconds < STWPerformanceTelemetryWarmupSeconds)
        {
            // Warmup rows establish the native provider's frame-to-frame baselines,
            // but only post-warmup rows participate in validity and gap metrics.
            return;
        }

        // Validation measures the window from the first actual post-warmup row.
        // A frame can cross the warmup boundary, so using the fixed 30 s origin
        // can finalize one frame too early and produce a 59.985 s window.
        if (m_postWarmupStartSeconds < 0.0)
        {
            m_postWarmupStartSeconds = m_elapsedSeconds;
        }

        m_invalidFrameObserved = m_invalidFrameObserved || !valid;

        frame.m_rhiCpuFrameMilliseconds > 0.0
            ? m_rhiCpuFrameMilliseconds.push_back(frame.m_rhiCpuFrameMilliseconds)
            : void();
        if (frame.m_presentAvailable && frame.m_presentMilliseconds > 0.0)
        {
            m_presentMilliseconds.push_back(frame.m_presentMilliseconds);
        }

        if (m_elapsedSeconds - m_postWarmupStartSeconds >= STWPerformanceTelemetryWindowSeconds)
        {
            Finalize();
        }
    }

    void STWPerformanceTelemetryAdapter::Finalize()
    {
        if (m_reported)
        {
            return;
        }
        m_reported = true;

#if !AZ_TRAIT_SERVER
        if (AZ::RPI::PassSystemInterface* passSystem = AZ::RPI::PassSystemInterface::Get())
        {
            if (const AZ::RPI::Ptr<AZ::RPI::ParentPass>& rootPass = passSystem->GetRootPass())
            {
                rootPass->SetTimestampQueryEnabled(false);
            }
        }
#endif

        AZStd::string csv;
        m_model.WriteCsv(csv);
        AZ::IO::SystemFile outputFile;
        const bool fileWritten = outputFile.Open(
            m_csvPath.c_str(),
            AZ::IO::SystemFile::SF_OPEN_CREATE
                | AZ::IO::SystemFile::SF_OPEN_WRITE_ONLY
                | AZ::IO::SystemFile::SF_OPEN_CREATE_PATH)
            && outputFile.Write(csv.data(), csv.size()) == csv.size();
        outputFile.Close();

        const PerformanceTelemetrySummary summary = m_model.Validate(
            STWPerformanceTelemetryWarmupSeconds,
            STWPerformanceTelemetryWindowSeconds,
            STWPerformanceTelemetryMinimumSamples);
        const double rhiCpuP95 = STWPerformanceTelemetryNearestRank(m_rhiCpuFrameMilliseconds, 0.95);
        const double presentP95 = STWPerformanceTelemetryNearestRank(m_presentMilliseconds, 0.95);
        const double unattributedGap = summary.m_valid
            ? AZStd::max(0.0, summary.m_frameP95Milliseconds
                - AZStd::max(summary.m_cpuP95Milliseconds, summary.m_gpuP95Milliseconds))
            : 0.0;

        AZ_Printf(
            "STWGameplay",
            "PERFORMANCE_TELEMETRY enabled=1 valid=%s csv_written=%s samples=%zu duration_s=%.3f "
            "frame_p95_ms=%.3f cpu_p95_ms=%.3f gpu_p95_ms=%.3f draw_calls_peak=%.0f "
            "vram_mib_peak=%.3f ram_mib_peak=%.3f error=%s\n",
            summary.m_valid && !m_invalidFrameObserved ? "true" : "false",
            fileWritten ? "true" : "false",
            summary.m_postWarmupSamples,
            summary.m_durationSeconds,
            summary.m_frameP95Milliseconds,
            summary.m_cpuP95Milliseconds,
            summary.m_gpuP95Milliseconds,
            summary.m_drawCallsPeak,
            summary.m_vramMiBPeak,
            summary.m_ramMiBPeak,
            summary.m_error.empty() ? "none" : summary.m_error.c_str());

        AZ_Printf(
            "STWGameplay",
            "PERFORMANCE_FRAME_GAP frame_p95_ms=%.3f cpu_main_p95_ms=%.3f gpu_p95_ms=%.3f "
            "rhi_cpu_frame_p95_ms=%s present_p95_ms=%s unattributed_gap_ms=%.3f "
            "cpu_source=MainThreadCpuTime gpu_source=PassTimestampExtent "
            "rhi_cpu_source=RHISystemInterface::GetCpuFrameTime present_source=UNAVAILABLE\n",
            summary.m_frameP95Milliseconds,
            summary.m_cpuP95Milliseconds,
            summary.m_gpuP95Milliseconds,
            m_rhiCpuFrameMilliseconds.empty() ? "UNAVAILABLE" : AZStd::string::format("%.3f", rhiCpuP95).c_str(),
            m_presentMilliseconds.empty() ? "UNAVAILABLE" : AZStd::string::format("%.3f", presentP95).c_str(),
            unattributedGap);
    }
} // namespace STWGameplay
