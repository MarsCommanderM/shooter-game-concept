#pragma once

#include <STWGameplay/PerformanceTelemetryModel.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace STWGameplay
{
    struct STWPerformanceTelemetryFrame
    {
        double m_cpuMilliseconds = 0.0;
        double m_gpuMilliseconds = 0.0;
        double m_drawCalls = 0.0;
        double m_vramMiB = 0.0;
        double m_ramMiB = 0.0;
        double m_rhiCpuFrameMilliseconds = 0.0;
        double m_presentMilliseconds = 0.0;
        bool m_presentAvailable = false;
    };

    //! Test seam for the opt-in telemetry adapter. The production implementation
    //! uses Atom/RHI and AzCore providers; inactive adapters never call this seam.
    class STWPerformanceTelemetryProvider
    {
    public:
        virtual ~STWPerformanceTelemetryProvider() = default;

        virtual bool CollectFrame(STWPerformanceTelemetryFrame& frame) = 0;
    };

    //! Opt-in runtime adapter for the machine-readable performance evidence CSV.
    //! No engine queries, allocations, logging, or file I/O are performed unless
    //! STW_PERF_TELEMETRY=1 and STW_PERF_TELEMETRY_CSV is a non-empty path.
    class STWPerformanceTelemetryAdapter
    {
    public:
        explicit STWPerformanceTelemetryAdapter(STWPerformanceTelemetryProvider* provider = nullptr);

        void Activate();
        void Deactivate();
        void RecordFrame(float deltaTime);

        bool IsEnabled() const { return m_enabled; }

    private:
        void Finalize();

        STWPerformanceTelemetryProvider* m_provider = nullptr;
        PerformanceTelemetryModel m_model;
        AZStd::string m_csvPath;
        AZStd::vector<double> m_rhiCpuFrameMilliseconds;
        AZStd::vector<double> m_presentMilliseconds;
        double m_elapsedSeconds = 0.0;
        double m_postWarmupStartSeconds = -1.0;
        bool m_enabled = false;
        bool m_reported = false;
        bool m_invalidFrameObserved = false;
    };
} // namespace STWGameplay
