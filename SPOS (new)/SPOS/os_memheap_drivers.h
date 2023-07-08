/*
 * os_memheap_drivers.h
 *
 * Created: 2023/5/15 16:20:04
 *  Author: 11481
 */ 

#ifndef _OS_MEMHEAP_DRIVERS_H
#define _OS_MEMHEAP_DRIVERS_H

#include "os_mem_drivers.h"
#include <stddef.h>

#define intHeap (&intHeap__)
#define extHeap (&extHeap__)

typedef enum AllocStrategy {
	OS_MEM_FIRST,
	OS_MEM_NEXT,
	OS_MEM_BEST,
	OS_MEM_WORST
} AllocStrategy;

//Heaptreiber
typedef struct Heap{
	MemDriver *driver; //Zeiger auf Speichertreiber
	MemAddr mapStart;
	uint16_t mapSize;
	MemAddr useStart;
	uint16_t useSize;
	AllocStrategy currentStrat;
	const char *name; //Zeiger auf den Namen des Heaps
	MemAddr allocFrameStart[MAX_NUMBER_OF_PROCESSES];
	MemAddr allocFrameEnd[MAX_NUMBER_OF_PROCESSES];
	MemAddr lastAllocation;
} Heap;

#define HIGHESTADDRESSUSERHEAP(heap) (heap->useStart + heap->useSize - 1)
#define FIRSTADRESSPUTSIDEUSERHEAP(heap) (heap->useStart + heap->useSize)

Heap intHeap__;
Heap extHeap__;

void os_initHeaps(void);

size_t os_getHeapListLength(void);

Heap* os_lookupHeap(uint8_t index);

uint8_t os_lookupHeapIndex(Heap* heap);

#endif