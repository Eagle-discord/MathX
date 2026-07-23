#include "MemoryUsage.h"

#if defined(_WIN32)

#include <windows.h>
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
            static_cast<std::size_t>(pmc.WorkingSetSize),
            static_cast<std::size_t>(pmc.PrivateUsage),
            static_cast<std::size_t>(pmc.PeakWorkingSetSize)
        };
    }

    // Return zeros if the API call fails.
    return { 0, 0, 0 };
}

#elif defined(__linux__)

#include <cstdio>
#include <cstring>

// Linux exposes process memory through /proc/self/status as human-readable
// "VmXXX:   N kB" lines. We pull the three fields that map onto the Windows
// counters. VmData is the closest analogue to Windows' private commit.
MemoryUsage getMemoryUsage()
{
    MemoryUsage usage{ 0, 0, 0 };

    std::FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return usage;

    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        unsigned long kb = 0;
        if (std::sscanf(line, "VmRSS: %lu kB", &kb) == 1)
            usage.workingSet = static_cast<std::size_t>(kb) * 1024;
        else if (std::sscanf(line, "VmData: %lu kB", &kb) == 1)
            usage.privateBytes = static_cast<std::size_t>(kb) * 1024;
        else if (std::sscanf(line, "VmHWM: %lu kB", &kb) == 1)
            usage.peakWorkingSet = static_cast<std::size_t>(kb) * 1024;
    }

    std::fclose(f);
    return usage;
}

#else

// Unknown platform: no portable way to read process memory - report zeros.
MemoryUsage getMemoryUsage()
{
    return { 0, 0, 0 };
}

#endif
