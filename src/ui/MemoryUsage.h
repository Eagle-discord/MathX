#ifndef MEMORYUSAGE_H
#define MEMORYUSAGE_H

#include <cstddef>

// Process memory figures, in bytes. Portable across Windows and Linux; a
// platform without an implementation returns all zeros. Fields mirror what the
// underlying OS exposes:
//   workingSet    - resident memory (Win: WorkingSetSize, Linux: VmRSS)
//   privateBytes  - private commit    (Win: PrivateUsage,  Linux: VmData≈private)
//   peakWorkingSet- high-water resident (Win: PeakWorkingSetSize, Linux: VmHWM)
struct MemoryUsage
{
    std::size_t workingSet;
    std::size_t privateBytes;
    std::size_t peakWorkingSet;
};

MemoryUsage getMemoryUsage();

#endif // MEMORYUSAGE_H
