/*
 * os_memheap_drivers.c
 *
 * Created: 2023/5/15 16:21:48
 *  Author: 11481
 */ 

#include "os_memheap_drivers.h"
#include "defines.h"
#include "os_memory_strategies.h"
#include <avr/pgmspace.h>

const PROGMEM char intStr[] = "internal";
const PROGMEM char extStr[] = "external";

Heap intHeap__ ={
	.driver = intSRAM,
	.mapStart = 0x100 + HEAPOFFSET, //Anfang des SRAM + Sicherheitsabstand
	.mapSize = ((AVR_MEMORY_SRAM/2) - (0x100+HEAPOFFSET))/3, //H?lfte von dem restlichen verf¨¹gbaren Speicher
	.useStart = (0x100 + HEAPOFFSET) + (((AVR_MEMORY_SRAM/2) - (0x100+HEAPOFFSET))/3), //mapstart + mapsize
	.useSize = (((AVR_MEMORY_SRAM/2) - (0x100+HEAPOFFSET))/3)*2, //2/3 von dem restlichen verf¨¹gbaren Speicher
	.currentStrat = OS_MEM_FIRST,
	.name = intStr,
	.allocFrameStart = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF},
	.allocFrameEnd = {0,0,0,0,0,0,0,0},
	.lastAllocation = (0x100 + HEAPOFFSET) + (((AVR_MEMORY_SRAM/2) - (0x100+HEAPOFFSET))/3)
};

Heap extHeap__ = {
	.driver = extSRAM,
	.mapStart = 0,
	.mapSize = 0xFFFF / 3, //21.845 Bytes; 21.844 is the address of the highest Byte in the Heap Map. 
	.useStart = 0xFFFF / 3, 
	.useSize = (0xFFFF/3) * 2, 
	.currentStrat = OS_MEM_FIRST,
	.name =  extStr,
	.allocFrameStart = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF},
	.allocFrameEnd = {0,0,0,0,0,0,0,0},
	.lastAllocation = (0xFFFF/3)
};


//alle Adressesn des Map-Bereichs auf 0
void os_initHeaps(void){
	uint16_t i = 0;
	MemAddr start = intHeap__.mapStart;
	//IntHeap
	for(i=0; i<intHeap__.mapSize;i++){ 
		intHeap__.driver->write(start+i,0); 
	}
	//Ext Heap
	for (i = 0; i < extHeap__.mapSize; i++ ) {
		extHeap__.driver->write(extHeap__.mapStart + i, 0);
	}
}

//Anzahl an existierenden Heaps 
size_t os_getHeapListLength(void){
	return 2;
}

//Geben den Zeiger auf den entsprechenden Heap zur¨¹ck
Heap* os_lookupHeap(uint8_t index){
	switch (index) {
		case 0: return intHeap;
		case 1: return extHeap;
		default: return NULL; 
	}
}
uint8_t os_lookupHeapIndex(Heap* heap) {
	for (uint8_t i = 0; i <= os_getHeapListLength(); i++) {
		if (heap == os_lookupHeap(i)) {return i;}
	}
	return 0xFF;
}