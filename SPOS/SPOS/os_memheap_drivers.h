#ifndef OS_MEMHEAP_DRIVERS_H
#define OS_MEMHEAP_DRIVERS_H

#include "os_mem_drivers.h"
#include <stddef.h>

typedef enum AllocStrategy
{
	OS_MEM_FIRST,
	OS_MEM_NEXT,
	OS_MEM_BEST,
	OS_MEM_WORST
} AllocStrategy;

typedef struct Heap{
	MemDriver* driver;
	MemAddr mapStart;
	size_t mapSize;
	MemAddr useStart;
	size_t useSize;
	AllocStrategy strategy;
	const char* name;
	// Optimierung
	MemAddr allocFrameStart[MAX_NUMBER_OF_PROCESSES];
	MemAddr allocFrameEnd[MAX_NUMBER_OF_PROCESSES];
} Heap;

extern Heap intHeap__;
extern Heap extHeap__;
#define intHeap (&intHeap__)
#define extHeap (&extHeap__)

void os_initHeaps(void);

size_t os_getHeapListLength(void);

Heap* os_lookupHeap(uint8_t index);

#endif