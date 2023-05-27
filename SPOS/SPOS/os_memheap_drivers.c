#include "os_memheap_drivers.h"
#include "defines.h"
#include "os_memory_strategies.h"
#include <avr/pgmspace.h>

Heap intHeap__ = {.driver = intSRAM};
	
void os_initHeaps(){
	MemAddr mapAddress;
	MemValue initialValue = 0;

	for (mapAddress = intHeap__.mapStart; mapAddress < (intHeap__.mapStart + intHeap__.mapSize); mapAddress++) {
		intSRAM__.write(mapAddress, initialValue);
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