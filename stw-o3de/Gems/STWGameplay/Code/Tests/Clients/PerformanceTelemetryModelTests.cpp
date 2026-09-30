#include <AzTest/AzTest.h>
#include <STWGameplay/PerformanceTelemetryModel.h>

#include <cmath>

namespace STWGameplay
{
    namespace
    {
        PerformanceTelemetryModel MakeValidModel()
        {
            PerformanceTelemetryModel model;
            model.AddSample(0.0, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);

            double timeSeconds = 30.0;
            model.AddSample(timeSeconds, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
            for (size_t index = 1; index <= 600; ++index)
            {
                const double frameMilliseconds = index == 600 ? 120.0 : 100.0;
                timeSeconds += frameMilliseconds / 1000.0;
                model.AddSample(timeSeconds, frameMilliseconds, 10.0, 12.0,
                    static_cast<double>(index + 1), 1024.0 + index, 4096.0 + index);
            }
            return model;
        }
    } // namespace

    TEST(PerformanceTelemetryModelTests, ValidatesForgeCompatibleRun)
    {
        const PerformanceTelemetrySummary summary = MakeValidModel().Validate();

        ASSERT_TRUE(summary.m_valid);
        EXPECT_EQ(summary.m_postWarmupSamples, 601);
        EXPECT_NEAR(summary.m_durationSeconds, 60.02, 1e-9);
        EXPECT_DOUBLE_EQ(summary.m_frameP50Milliseconds, 100.0);
        EXPECT_DOUBLE_EQ(summary.m_frameP95Milliseconds, 100.0);
        EXPECT_DOUBLE_EQ(summary.m_frameP99Milliseconds, 100.0);
        EXPECT_DOUBLE_EQ(summary.m_cpuP95Milliseconds, 10.0);
        EXPECT_DOUBLE_EQ(summary.m_gpuP95Milliseconds, 12.0);
        EXPECT_DOUBLE_EQ(summary.m_drawCallsPeak, 601.0);
        EXPECT_DOUBLE_EQ(summary.m_vramMiBPeak, 1624.0);
        EXPECT_DOUBLE_EQ(summary.m_ramMiBPeak, 4696.0);
    }

    TEST(PerformanceTelemetryModelTests, MatchesForgeNearestRankReference)
    {
        // Reference values are from forge.py percentile(values, quantile):
        // sorted(values)[ceil(len(values) * quantile) - 1]. The fixture has
        // 600 post-warmup frame values: 599x100 ms and 1x120 ms.
        const PerformanceTelemetrySummary summary = MakeValidModel().Validate();
        ASSERT_TRUE(summary.m_valid);
        EXPECT_DOUBLE_EQ(summary.m_frameP95Milliseconds, 100.0);
        EXPECT_DOUBLE_EQ(summary.m_frameP99Milliseconds, 100.0);
        EXPECT_DOUBLE_EQ(summary.m_drawCallsPeak, 601.0);
        EXPECT_DOUBLE_EQ(summary.m_vramMiBPeak, 1624.0);
        EXPECT_DOUBLE_EQ(summary.m_ramMiBPeak, 4696.0);
    }

    TEST(PerformanceTelemetryModelTests, RejectsNonMonotoneOrNonFiniteSamples)
    {
        PerformanceTelemetryModel model;
        model.AddSample(0.0, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
        model.AddSample(0.0, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
        EXPECT_FALSE(model.Validate().m_valid);

        PerformanceTelemetryModel nonFinite;
        nonFinite.AddSample(0.0, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
        nonFinite.AddSample(30.0, std::nan(""), 10.0, 12.0, 1.0, 1024.0, 4096.0);
        EXPECT_FALSE(nonFinite.Validate(30.0, 0.0, 1).m_valid);
    }

    TEST(PerformanceTelemetryModelTests, RejectsShortRunsAndZeroTelemetry)
    {
        PerformanceTelemetryModel shortRun;
        shortRun.AddSample(0.0, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
        shortRun.AddSample(30.0, 100.0, 10.0, 12.0, 0.0, 1024.0, 4096.0);
        shortRun.AddSample(30.1, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);

        const PerformanceTelemetrySummary summary = shortRun.Validate(30.0, 0.0, 1);
        EXPECT_FALSE(summary.m_valid);
        EXPECT_STREQ(summary.m_error.c_str(), "missing/zero telemetry");
    }

    TEST(PerformanceTelemetryModelTests, RejectsFrameDurationGaps)
    {
        PerformanceTelemetryModel model;
        model.AddSample(0.0, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
        model.AddSample(30.0, 10.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
        model.AddSample(90.0, 10.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);

        const PerformanceTelemetrySummary summary = model.Validate(30.0, 60.0, 2);
        EXPECT_FALSE(summary.m_valid);
        EXPECT_STREQ(summary.m_error.c_str(), "CSV must contain contiguous per-frame telemetry");
    }

    TEST(PerformanceTelemetryModelTests, WritesExactForgeCsvHeader)
    {
        PerformanceTelemetryModel model;
        model.AddSample(0.0, 100.0, 10.0, 12.0, 1.0, 1024.0, 4096.0);
        AZStd::string csv;
        model.WriteCsv(csv);

        EXPECT_TRUE(csv.starts_with("time_s,frame_ms,cpu_ms,gpu_ms,draw_calls,vram_mib,ram_mib\n"));
        EXPECT_NE(csv.find("0.000000000,100.000000000,10.000000000,12.000000000,1.000000000,1024.000000000,4096.000000000\n"), AZStd::string::npos);
    }
} // namespace STWGameplay
