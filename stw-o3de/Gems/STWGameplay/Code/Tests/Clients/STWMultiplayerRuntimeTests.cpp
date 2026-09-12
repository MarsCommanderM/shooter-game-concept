#include <AzTest/AzTest.h>

#include "Clients/STWMultiplayerRuntime.h"

namespace STWGameplay
{
    TEST(STWMultiplayerRuntimeTests, StartsUnavailableUntilTheEngineInterfaceExists)
    {
        STWMultiplayerRuntime runtime;

        EXPECT_EQ(runtime.GetState(), STWMultiplayerTransportState::Unavailable);
        const bool initialized = runtime.Initialize();
        EXPECT_EQ(initialized, runtime.GetState() != STWMultiplayerTransportState::Unavailable);
    }

    TEST(STWMultiplayerRuntimeTests, ShutdownIsSafeBeforeInitializationAndAfterInitializationFailure)
    {
        STWMultiplayerRuntime runtime;

        runtime.Shutdown();
        const bool initialized = runtime.Initialize();
        EXPECT_EQ(runtime.GetState(), initialized
            ? STWMultiplayerTransportState::Idle
            : STWMultiplayerTransportState::Unavailable);
        runtime.Shutdown();
        runtime.Shutdown();

        EXPECT_EQ(runtime.GetState(), STWMultiplayerTransportState::Unavailable);
    }
} // namespace STWGameplay
