#ifndef OS_MEMORY_STRATEGIES_H
#define  OS_MEMORY_STRATEGIES_H

#include "os_memheap_drivers.h"

MemAddr os_Memory_FirstFit(Heap *heap, size_t size);

MemAddr os_Memory_NextFit(Heap *heap, size_t size);

MemAddr os_Memory_BestFit(Heap *heap, size_t size);

MemAddr os_Memory_WorstFit(Heap *heap, size_t size);

#endif