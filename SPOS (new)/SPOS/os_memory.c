/*
 * os_memory.c
 *
 * Created: 2023/5/15 23:56:03
 *  Author: 11481
 */ 
#include "os_memory.h"
#include "os_memory_strategies.h"
#include "util.h"
#include "os_core.h"
#include <stdint.h>


//Included for Debugging
//#include "lcd.h"



// Helper Function to Decrease the AllocFrame after freeing a Chunk if necessary

void decAllocFrame(Heap *heap, MemAddr firstAddr, ProcessID pid) {
	// The removed Chunk was the left most Chunk. Therefore the AllocFrame need to be adjusted.
	if (firstAddr <= heap->allocFrameStart[pid]) {
		for (MemAddr i = firstAddr; i <= heap->allocFrameEnd[pid]; i++) {
			if (os_getMapEntry(heap,i) == pid) {heap->allocFrameStart[pid] = i; return;}
		}
		// If no other Chunk exists, set allocFrame back to default.
		heap->allocFrameStart[pid] = 0xFFFF;
	}
	//The Removed Chunk was the right-outermost Chunk and the AllocFrame needs to be adjusted.
	if (firstAddr >= heap->allocFrameEnd[pid]) {
		for (MemAddr i = firstAddr; i >= heap->allocFrameStart[pid]; i--) {
			if (os_getMapEntry(heap,pid) == pid) {heap->allocFrameEnd[pid] = i; return;}
		}
		// If no other Chunk exists, set allocFrame back to default.
		heap->allocFrameEnd[pid] = 0;
	}
}

// Helper Function To Increase the AllocFrame if necessary after allocating a Chunk if necessary

void incAllocFrame (Heap *heap, MemAddr firstAddr, ProcessID pid) {
	if (firstAddr < heap->allocFrameStart[pid]) {
		heap->allocFrameStart[pid] = firstAddr;
	}
	if (firstAddr > heap->allocFrameEnd[pid]) {
		heap->allocFrameEnd[pid] = firstAddr;
	}
}

void setLowNibble (Heap const *heap, MemAddr addr, MemValue value){
	MemValue origin = (heap->driver->read(addr)) & 0b11110000;
	origin |= (value & 0b00001111);
	heap->driver->write(addr,origin);
}

void setHighNibble(Heap const *heap, MemAddr addr, MemValue value){
	MemValue origin = (heap->driver->read(addr)) & 0b00001111;
	origin |= (value << 4);
	heap->driver->write(addr,origin);
}

MemValue getLowNibble (Heap const *heap, MemAddr addr){
	return (heap->driver->read(addr)) & 0b00001111;
}

MemValue getHighNibble(Heap const *heap, MemAddr addr){
	return heap->driver->read(addr) >> 4;
}

void setMapEntry(Heap const *heap, MemAddr addr, MemValue value){
	MemAddr i = addr - heap->useStart;
	MemAddr j = i/2;
	
	//Falls der Wert zu gro?, dann Fehlermeldung
	if(value > 0x0F){
		os_errorPStr(PSTR("Value is too large"));
	}
	else{
		//feststellen, dass die Adresse im Use-Bereich liegt
		if(addr >= heap->useStart && addr < (heap->useStart+heap->useSize)){
			if(i % 2 == 1){
				setLowNibble(heap,j+heap->mapStart,value);
			}
			else{
				setHighNibble(heap,j+heap->mapStart,value);
			}
		}
	}
}

//bekommt Adresse vom Use-Bereich und den Wert von der zugehörigen Map-Adresse zurückgeben
MemValue os_getMapEntry(Heap const *heap, MemAddr addr){
	MemAddr i = addr - heap->useStart;
	MemAddr j = i/2; //bestimmt die Adresse beim Map-Bereich
	if(i % 2 == 1){
		return getLowNibble(heap,j+heap->mapStart);
	}
	return getHighNibble(heap,j+heap->mapStart);
}

//bekommt eine Adresse innerhalb eines Chunks(Use-Bereich) und findet die erste Adresse des Chunks heraus
MemAddr os_getFirstByteOfChunk(Heap const *heap, MemAddr addr){
	MemAddr i = addr;
	while(os_getMapEntry(heap,i) == 0x0F){
		i--;
	}
	if (i < heap->mapStart) {os_errorPStr(PSTR("Error: getFirstByste"));}
// 	if(os_getMapEntry(heap,i) == 0x00){
// 		os_error("no first Byte");
// 	}
	return i;
}

//das Prozess des Chunks herausfinden
ProcessID getOwnerOfChunk(Heap const *heap, MemAddr addr){
	return os_getMapEntry(heap,os_getFirstByteOfChunk(heap,addr));
}

uint16_t os_getChunkSize(Heap const* heap, MemAddr addr){
	if(os_getMapEntry(heap,addr) == 0x00){
		return 0;
	}
	MemAddr i = os_getFirstByteOfChunk(heap,addr);
	uint16_t size = 1;
	while(os_getMapEntry(heap,i+size) == 0x0F && i+size <= HIGHESTADDRESSUSERHEAP(heap)){
		size++;
	}
	return size;
}

void os_freeOwnerRestricted(Heap *heap, MemAddr addr, ProcessID owner){
	os_enterCriticalSection();
	MemAddr i = os_getFirstByteOfChunk(heap, addr);
	MemAddr firstByteOfChunk = i;
	//checkt ob der owner (Prozessid) gleich ist
	if(os_getMapEntry(heap,i) != owner){
		os_errorPStr(PSTR("Falsche Speicherfreigabe"));
	}
	else{
		do {
			setMapEntry(heap, i, 0);
			i++;
		} while (os_getMapEntry(heap, i) == 0x0F && i < os_getUseSize(heap) + os_getUseStart(heap));
	}
	decAllocFrame(heap,firstByteOfChunk,owner);
	os_leaveCriticalSection();
}

MemAddr os_malloc(Heap* heap, uint16_t size){
	os_enterCriticalSection();
	if(size > heap->useSize){
		return 0;
	}
	volatile MemAddr current = 0;
	switch(heap->currentStrat){
		case OS_MEM_FIRST:
			current = os_Memory_FirstFit(heap,size);
			break;
		case OS_MEM_NEXT:
			current = os_Memory_NextFit(heap,size);
			break;
		case OS_MEM_WORST:
			current = os_Memory_WorstFit(heap,size);
			break;
		case OS_MEM_BEST:
			current = os_Memory_BestFit(heap,size);
			break;
		default:
			break;
	}
	if(current != 0){
		ProcessID pid = os_getCurrentProc();
		incAllocFrame(heap,current,pid);
		setMapEntry(heap,current,pid);
		for(uint16_t i=1;i<size;i++){
			setMapEntry(heap,current+i,0x0F);
		}
	}
	
	os_leaveCriticalSection();
	if (current != 0 && (current < heap->useStart || current > heap->useStart+heap->useSize-1)) {os_errorPStr(PSTR("malloc OOB"));}
	return current;
}

void os_free(Heap* heap, MemAddr addr){
	os_freeOwnerRestricted(heap,addr,os_getCurrentProc());
}

size_t os_getMapSize(Heap const* heap){
	return heap->mapSize;
}

size_t os_getUseSize(Heap const* heap){
	return heap->useSize;
}

MemAddr os_getMapStart(Heap const* heap){
	return heap->mapStart;
}

MemAddr os_getUseStart(Heap const* heap){
	return heap->useStart;
}

void os_setAllocationStrategy(Heap* heap, AllocStrategy allocStrat){
	heap->currentStrat = allocStrat;
}

AllocStrategy os_getAllocationStrategy(Heap const* heap){
	return heap->currentStrat;
}

//free all the memory of pid
void os_freeProcessMemory(Heap *heap, ProcessID pid){
	os_enterCriticalSection();
	//festlegen, dass der Leerlaufprozess nicht vorhanden ist
	if(pid != 0){
		/*if(os_lookupHeap(0) == heap && os_getWroteToIntSRAM(pid)) {
			for(MemAddr i=heap->useStart;i<(heap->useStart+heap->useSize);i++){
				if(os_getMapEntry(heap,i) == pid){
					os_freeOwnerRestricted(heap,i,pid);
				}
			}
		} else if (os_lookupHeap(1) == heap) */ 
		
		for (MemAddr i = heap->allocFrameStart[pid]; i <= heap->allocFrameEnd[pid]; i++) {
			if (os_getMapEntry(heap,i) == pid) {
				os_freeOwnerRestricted(heap,i,pid);
			}
		}
		
	}
	os_leaveCriticalSection();
}

/* This Function moves a Chunk to an completely new Place or slides a Chunk backwards. 
 Do not use this Function to slide a Chunk forward.
 This Function does not account for invalid addresses or sizes. 
 In Order to avoid redundancy this Function only operates in the UserHeap. 
 The Heap Map needs to be adjusted manually.  */
void moveChunk (Heap *heap, MemAddr oldChunk, size_t oldSize, MemAddr newChunk, size_t newSize) {
	oldChunk = os_getFirstByteOfChunk(heap,oldChunk);
	for (MemAddr i = 0; i < oldSize; i++) {
		(heap->driver->write)(newChunk + i, (heap->driver->read)(oldChunk + i) );
	}
}

/* This is an efficient reallocation routine. It is used to resize an existing allocated chunk of memory. 
 If possible, the position of the chunk remains the same. 
 It is only searched for a completely new chunk if everything else does not fit*/
MemAddr os_realloc(Heap *heap, MemAddr addr, uint16_t size) {
	os_enterCriticalSection();
	if (addr < heap->useStart || addr > HIGHESTADDRESSUSERHEAP(heap)) {os_errorPStr(PSTR("Addr OutofB. in Realloc"));os_leaveCriticalSection(); return 0;}
	if (os_getMapEntry(heap,addr) == 0) { os_leaveCriticalSection(); return 0;}
	if (getOwnerOfChunk(heap,addr) != os_getCurrentProc()) {os_errorPStr(PSTR("Unauthorized Reallocation")); os_leaveCriticalSection(); return 0;}
	if (size == 0) {os_errorPStr(PSTR("Cannot Realloc to 0."));os_leaveCriticalSection(); return 0;}
	// 
	addr = os_getFirstByteOfChunk(heap, addr);
	size_t oldsize = os_getChunkSize(heap,addr);
	
	// Increase Allocation 
	if (oldsize < size) {	
		
		// Look for Space behind the Chunk
		uint16_t freeSpaceBehindeTheChunk = 0;
		for (MemAddr i = addr + oldsize; i <= HIGHESTADDRESSUSERHEAP(heap); i++) {
			if (os_getMapEntry(heap,i) == 0) {freeSpaceBehindeTheChunk++;}
			else {break;}
		}
		if ( size <= oldsize + freeSpaceBehindeTheChunk) {
			for (MemAddr i = addr + oldsize; i < addr + size; i++) {
				setMapEntry(heap,i,0x0F);
			}
			os_leaveCriticalSection();
			return addr;
		}
		
		//If there was not enough Space behind the Chunk, check if there is enough Space in Front of the Chunk
		//Slides the Chunk back as far as possible to defragment the Heap if there is enough Space.
		uint16_t freeSpaceInFrontOfChunk = 0;
		if (addr != 0) {
			for (MemAddr i = addr - 1; i >= heap->useStart; i-- ) {
				if (os_getMapEntry(heap,i) == 0) {freeSpaceInFrontOfChunk++;}
				else {break;}
			}
		}
		if ( size <= oldsize + freeSpaceBehindeTheChunk + freeSpaceInFrontOfChunk) {
			moveChunk(heap,addr,oldsize,addr-freeSpaceInFrontOfChunk,size);
			setMapEntry(heap,addr-freeSpaceInFrontOfChunk,os_getCurrentProc());
			for (MemAddr i = addr-freeSpaceInFrontOfChunk+1; i < addr - freeSpaceInFrontOfChunk + size; i++) {
				setMapEntry(heap,i,0x0F);
			} 
			for (MemAddr i = addr-freeSpaceInFrontOfChunk+size; i < addr + oldsize; i++) {
				setMapEntry(heap,i,0);
			}
			os_leaveCriticalSection();
			return addr-freeSpaceInFrontOfChunk;
		}
		
		//If the Space around the Chunk is insufficient, search for an completely new Place.
		MemAddr newAddr = os_malloc(heap,size);
		if (newAddr == 0) { os_leaveCriticalSection(); return 0;}
		moveChunk(heap,addr,oldsize,newAddr,size);
		os_freeOwnerRestricted(heap,addr,os_getCurrentProc());
		os_leaveCriticalSection();
		return newAddr;
	}
	
	// Decrease Allocation
	else {
		for (MemAddr i = addr + size; i < addr + oldsize; i++) {
			setMapEntry(heap,i,0);
		}
		os_leaveCriticalSection();
		return addr;
	}
}

MemAddr os_sh_malloc(Heap *heap, size_t size) {
	os_enterCriticalSection();
	if(size > heap->useSize){
		return 0;
	}
	volatile MemAddr res = 0;
	switch(heap->currentStrat){
		case OS_MEM_FIRST:
			res = os_Memory_FirstFit(heap,size);
			break;
		case OS_MEM_NEXT:
			res = os_Memory_NextFit(heap,size);
			break;
		case OS_MEM_WORST:
			res = os_Memory_WorstFit(heap,size);
			break;
		case OS_MEM_BEST:
			res = os_Memory_BestFit(heap,size);
			break;
		default:
		break;
	}
	if(res != 0){
		setMapEntry(heap,res,SHARED_CLOSED);
		for(uint16_t i=1;i<size;i++){
			setMapEntry(heap,res+i,0x0F);
		}
	}
	os_leaveCriticalSection();
	if (res != 0 && (res < heap->useStart || res > heap->useStart+heap->useSize-1)) {os_errorPStr(PSTR("sh_malloc OOB"));}
	return res;	
}

void os_sh_free(Heap *heap, MemAddr *addr) {
	os_enterCriticalSection();
	while (1) {
		if (*addr < heap->mapStart || *addr > HIGHESTADDRESSUSERHEAP(heap)) {os_errorPStr(PSTR("ShFree Addr OOB"));}
		MemAddr internalAddrerss = os_getFirstByteOfChunk(heap,*addr);
		MemValue status = os_getMapEntry(heap,internalAddrerss);
		if (status == SHARED_WRITE_OPEN || (status >= SHARED_READ_OPEN && status < 0x0F) ) {
			os_yield();
		}
		else if (status != SHARED_CLOSED) {
			os_errorPStr(PSTR("shFree: not a shared chunk")); 
			os_leaveCriticalSection();
			return;
		}
		else {
			// Condition status == shared_closed is met
			setMapEntry(heap,internalAddrerss,0x00);
			internalAddrerss++;
			/*
			The condition addr <= HIGHESTADDRESSUSERHEAP is bearly sufficient to prevent Out of Bounds Access. 
			HIGHESTADDRESSUSERHEAP is 0xFFFE for the external Heap and addr is a MemAddr (uint_16) that overflows to 0 once it has reached 0xFFFF.
			Only the exact instance addr == 0xFFFF is caught. 
			*/
			while (os_getMapEntry(heap,internalAddrerss) == 0x0F && internalAddrerss <= HIGHESTADDRESSUSERHEAP(heap)) {
				setMapEntry(heap,internalAddrerss,0x00);
				internalAddrerss++;
			}
			os_leaveCriticalSection();
			return;
		}
	}
}

void os_sh_write(Heap const *heap, MemAddr const *ptr, uint16_t offset, MemValue const *dataScr, uint16_t length) {
	MemAddr firstAdress = os_sh_writeOpen(heap,ptr);
	uint16_t sharedChunkSize = os_getChunkSize(heap,firstAdress);
	if (sharedChunkSize < (uint32_t)offset + length) {
		os_errorPStr(PSTR("shWrite: sharedMem too  small"));
		os_sh_close(heap,firstAdress);
	} else {
		for (uint16_t i = 0; i < length; i++) {
			heap->driver->write(firstAdress + offset + i, *(dataScr+i));
		}
		os_sh_close(heap,firstAdress);
	}
}

void os_sh_read(Heap const *heap, MemAddr const *ptr, uint16_t offset, MemValue *dataDest, uint16_t length) {
	MemAddr firstAdress = os_sh_readOpen(heap,ptr);
	uint16_t sharedChunkSize = os_getChunkSize(heap,firstAdress);
	MemValue *iter = dataDest;
	if (sharedChunkSize < (uint32_t)offset + length) {
		os_errorPStr(PSTR("shRead: sharedMem too small"));
		os_sh_close(heap,firstAdress);
	} else {
		for (uint16_t i = 0; i < length; i++) {
			*iter = heap->driver->read(firstAdress + offset + i);
			iter++;
		}
		os_sh_close(heap,firstAdress);
	}
}

MemAddr os_sh_readOpen(Heap const *heap, MemAddr const *ptr) {
	os_enterCriticalSection();
	MemValue status = 0;
	MemAddr internalAddress = 0;
	while (1) {
		if (*ptr < heap->mapStart || *ptr > HIGHESTADDRESSUSERHEAP(heap)) {os_errorPStr(PSTR("shReadOpen Ptr OOB")); os_leaveCriticalSection(); return 0;}
		internalAddress = *ptr;
		internalAddress = os_getFirstByteOfChunk(heap,internalAddress);
		status = os_getMapEntry(heap,internalAddress);
		if (status == SHARED_CLOSED) {
			setMapEntry(heap,internalAddress,SHARED_READ_OPEN);
			os_leaveCriticalSection();
			return internalAddress;
		} else if (status >= SHARED_READ_OPEN && status < 0x0E) { 
			/*
			Up to 5 Processes may read at the same time. 
			status == 10 (SHARED_READ_OPEN) --> One Process is Reading
			status == 11 --> Two Processes are Reading
			status == 12 --> Three Processes are Reading
			status == 13 --> Four Processes are Reading
			status == 14 --> Five Processes are Reading
			*/
			status++;
			setMapEntry(heap,internalAddress,status);
			os_leaveCriticalSection();
			return internalAddress;
		} else if (status == SHARED_WRITE_OPEN || status == 0x0E) {
			os_yield(); //The opened Critical Section is preserved and restored after returning from os_yield.
		} else {
			os_errorPStr(PSTR("shReadOpen: Not a shared Chunk")); //if this error was thrown the Chunk may have been freed prematurely in the user Program
			os_leaveCriticalSection();
			return 0;
		}
	}
}

MemAddr os_sh_writeOpen(Heap const *heap, MemAddr const *ptr) {
	os_enterCriticalSection();
	MemValue status = 0;
	MemAddr internalAddress = 0;
	while (1) {
		if (*ptr < heap->mapStart || *ptr > HIGHESTADDRESSUSERHEAP(heap)) {os_errorPStr(PSTR("shWriteOpen Ptr OOB")); os_leaveCriticalSection(); return 0;}
		internalAddress = *ptr;
		internalAddress = os_getFirstByteOfChunk(heap,internalAddress);
		status = os_getMapEntry(heap,internalAddress);
		if (status == SHARED_CLOSED) {
			setMapEntry(heap, internalAddress,SHARED_WRITE_OPEN);
			os_leaveCriticalSection();
			return internalAddress;
		} else if (status == SHARED_WRITE_OPEN || (status >= SHARED_READ_OPEN && status < 0x0F) ) {
			os_yield(); //The opened Critical Section is preserved and restored after returning from os_yield.
		} else {
			os_errorPStr(PSTR("shWrite: Not a Shared Chunk")); //if this error was thrown the Chunk may have been freed prematurely in the user Program
			os_leaveCriticalSection();
			return 0;
		}
	}
}

void os_sh_close(Heap const *heap, MemAddr addr) {
	os_enterCriticalSection();
	if (addr < heap->useStart || addr > HIGHESTADDRESSUSERHEAP(heap)) {os_errorPStr(PSTR("shClose: addr OOB")); os_leaveCriticalSection(); return;}
	addr = os_getFirstByteOfChunk(heap,addr);
	MemValue status = os_getMapEntry(heap,addr);
	if (status == SHARED_WRITE_OPEN || status == SHARED_READ_OPEN) {
		setMapEntry(heap,addr,SHARED_CLOSED);
	} else if (status > SHARED_READ_OPEN && status < 0x0F) {
		status--;
		setMapEntry(heap,addr,status);
	} else if (status == SHARED_CLOSED) {
		os_errorPStr(PSTR("shClose: Already Closed"));
	} else {
		os_errorPStr(PSTR("shClose: Not a shared Chunk"));
	}
	os_leaveCriticalSection();
	
}
