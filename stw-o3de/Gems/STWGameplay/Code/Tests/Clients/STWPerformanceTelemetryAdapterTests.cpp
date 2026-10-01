#include <AzTest/AzTest.h>

#include "../../Source/Clients/STWPerformanceTelemetryAdapter.h"

namespace STWGameplay
{
    namespace
    {
        class FakePerformanceTelemetryProvider final
            : public STWPerformanceTelemetryProvider
        {
        public:
            bool CollectFrame(STWPerformanceTelemetryFrame& frame) override
            {
                ++m_collectCalls;
                frame.m_cpuMilliseconds = 1.0;
                frame.m_gpuMilliseconds = 1.0;
                frame.m_drawCalls = 1.0;
                frame.m_vramMiB = 1.0;
                frame.m_ramMiB = 1.0;
                return true;
            }

            size_t m_collectCalls = 0;
        };
    } // namespace

    TEST(STWPerformanceTelemetryAdapterTests, DoesNotQueryProviderWhenTelemetryIsNotActivated)
    {
        FakePerformanceTelemetryProvider provider;
        STWPerformanceTelemetryAdapter adapter(&provider);

        adapter.RecordFrame(1.0f / 60.0f);

        EXPECT_FALSE(adapter.IsEnabled());
        EXPECT_EQ(provider.m_collectCalls, 0);
    }
} // namespace STWGameplay
