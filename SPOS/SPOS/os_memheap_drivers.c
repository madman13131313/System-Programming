#include "os_memheap_drivers.h"
#include "defines.h"
#include "os_memory_strategies.h"
#include <avr/pgmspace.h>

#define MAPSTART HEAPOFFSET + 0x100
#define MAPSIZE ((0x10FF - 0x100) / 2 - HEAPOFFSET)/3

Heap intHeap__ =
{
	.driver = intSRAM,
	.mapSize = MAPSIZE,
	.mapStart = MAPSTART,
	.name = "intHeap",
	.strategy = OS_MEM_FIRST,
	.useSize = MAPSIZE + MAPSIZE,
	.useStart= MAPSTART + MAPSIZE
};

void os_initHeaps(){
	MemAddr mapAddress;

	for (mapAddress = intHeap__.mapStart; mapAddress < (intHeap__.mapStart + intHeap__.mapSize); mapAddress++) {
		*((uint8_t *)mapAddress) = 0b00000000;
	}
	// Optimierung
	for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++)
	{
		intHeap__.allocFrameStart[i] = 0;
		intHeap__.allocFrameEnd[i] = 0;
	}
}

size_t os_getHeapListLength(void) {
	return 1;
}

Heap* os_lookupHeap(uint8_t index) {
	if (index == 0) {
		return intHeap;
		}else {
		return NULL;
	}
}