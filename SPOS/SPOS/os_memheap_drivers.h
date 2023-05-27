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
	char* name;
} Heap;

extern Heap intHeap__;
#define intHeap (&intHeap__)

void os_initHeaps(void);

size_t os_getHeapListLength(void);

Heap* os_lookupHeap(uint8_t index);

#endif