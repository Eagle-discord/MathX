#ifndef MEMORYUSAGE_H
#define MEMORYUSAGE_H

#include <windows.h>

struct MemoryUsage
{
    SIZE_T workingSet;
    SIZE_T privateBytes;
    SIZE_T peakWorkingSet;
};

MemoryUsage getMemoryUsage();

#endif // MEMORYUSAGE_H