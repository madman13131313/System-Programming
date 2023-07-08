/*
 * os_memory_strategies.h
 *
 * Created: 2023/5/16 1:12:04
 *  Author: 11481
 */ 

#ifndef OS_MEMORY_STRATEGIES_H_
#define OS_MEMORY_STRATEGIES_H_

#include "os_memheap_drivers.h"

MemAddr os_Memory_FirstFit (Heap *heap, size_t size);

MemAddr os_Memory_NextFit (Heap *heap, size_t size);

MemAddr os_Memory_BestFit (Heap *heap, size_t size);

MemAddr os_Memory_WorstFit (Heap *heap, size_t size);

#endif /* OS_MEMORY_STRATEGIES_H_ */