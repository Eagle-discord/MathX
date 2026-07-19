#include "MemoryUsage.h"

#include <psapi.h>

MemoryUsage getMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};

    if (GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
        sizeof(pmc)))
    {
        return {
            pmc.WorkingSetSize,
            pmc.PrivateUsage,
            pmc.PeakWorkingSetSize
        };
    }

    // Return zeros if the API call fails.
    return {
        0,
        0,
        0
    };
}   