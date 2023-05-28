#include "os_memory.h"
#include "os_memory_strategies.h"
#include "util.h"
#include "os_core.h"

// Writes a value from 0x0 to 0xF to the lower nibble of the given address.
void setLowNibble(Heap const *heap, MemAddr addr, MemValue value){
	MemValue temp = heap->driver->read(addr);
	temp &= 0b11110000;
	temp |= value;
	heap->driver->write(addr, temp);
}

// Writes a value from 0x0 to 0xF to the higher nibble of the given address.
void setHighNibble(Heap const *heap, MemAddr addr, MemValue value){
	MemValue temp = heap->driver->read(addr);
	temp &= 0b00001111;
	temp |= (value << 4);
	heap->driver->write(addr, temp);
}

// Reads the value of the lower nibble of the given address.
MemValue getLowNibble (Heap const *heap, MemAddr addr){
	return (heap->driver->read(addr) & 0b00001111);
}

// Reads the value of the higher nibble of the given address.
MemValue getHighNibble (Heap const *heap, MemAddr addr){
	return (heap->driver->read(addr) >> 4);
}

// This function is used to set a heap map entry on a specific heap.
void setMapEntry (Heap const *heap, MemAddr addr, MemValue value){
	if (addr >= heap->useStart && addr < heap->useStart + heap->useSize)
	{
		MemAddr temp = addr - (heap->useStart);
		if (temp % 2 == 0)
		{
			setHighNibble(heap, (heap->mapStart + temp / 2), value);
			}else{
			setLowNibble(heap, (heap->mapStart + temp / 2), value);
		}
		}else{
		os_error("addr not in range");
	}
}

// Function used to get the value of a single map entry, this is made public so the allocation strategies can use it.
MemValue os_getMapEntry (Heap const *heap, MemAddr addr){
	MemAddr temp = addr - (heap->useStart);
	if (temp % 2 == 0)
	{
		return getHighNibble(heap, (heap->mapStart + temp / 2));
	}
	else{
		return getLowNibble(heap, (heap->mapStart + temp / 2));
	}
}

// Function used to allocate private memory.
MemAddr os_malloc(Heap* heap, uint16_t size){
	os_enterCriticalSection();
	MemAddr allocStart = 0;
	ProcessID current = os_getCurrentProc();
	if (current == 0)
	{
		os_error("Current Process idle");
		os_leaveCriticalSection();
		return 0;
	}
	switch (heap->strategy)
	{
		case OS_MEM_FIRST:
		allocStart = os_Memory_FirstFit(heap, size);
		break;
		case OS_MEM_NEXT:
		allocStart = os_Memory_NextFit(heap, size);
		break;
		case OS_MEM_WORST:
		allocStart = os_Memory_WorstFit(heap, size);
		break;
		case OS_MEM_BEST:
		allocStart = os_Memory_BestFit(heap, size);
		break;
	}
	if (allocStart != 0)
	{
		setMapEntry(heap, allocStart, current);
		for (MemAddr i = allocStart + 1; i < allocStart + size; i++)
		{
			setMapEntry(heap, i, 0b00001111);
		}
	}
	os_leaveCriticalSection();
	return allocStart;
}

// Get the address of the first byte of chunk.
MemAddr os_getFirstByteOfChunk (Heap const *heap, MemAddr addr){
	MemAddr firstByte = addr;
	while (os_getMapEntry(heap, firstByte) == 0b00001111)
	{
		firstByte--;
	}
	return firstByte;
}

// This function determines the value of the first nibble of a chunk.
ProcessID getOwnerOfChunk (Heap const *heap, MemAddr addr){
	return os_getMapEntry(heap, os_getFirstByteOfChunk(heap, addr));
}

// Get the size of a chunk on a given address.
uint16_t os_getChunkSize (Heap const *heap, MemAddr addr){
	MemAddr firstByte = os_getFirstByteOfChunk(heap, addr);
	while (((os_getMapEntry(heap, addr) == 0b00001111) || (addr == firstByte)) && (addr < heap->useStart+heap->useSize))
	{
		addr++;
	}
	addr--;
	return (addr - firstByte + 1);
}

// Frees the chunk iff it is owned by the given owner.
void os_freeOwnerRestricted (Heap *heap, MemAddr addr, ProcessID owner) {
	if(addr < heap->useStart || addr>=heap->useStart+heap->useSize) {
		os_leaveCriticalSection();
		return;
	}
	MemAddr firstByte = os_getFirstByteOfChunk(heap,addr);
	if (owner == os_getMapEntry(heap, firstByte) ) {
		MemAddr lastByte = os_getFirstByteOfChunk(heap, addr) + os_getChunkSize(heap, addr);
		for( ; firstByte < lastByte; firstByte++) {
			setMapEntry(heap, firstByte, 0);
		}
		} else {
	}
}



// Function used by processes to free their own allocated memory.
void os_free (Heap *heap, MemAddr addr){
	os_enterCriticalSection();
	ProcessID current = os_getCurrentProc();
	os_freeOwnerRestricted(heap, addr, current);
	os_leaveCriticalSection();
}

// Get the size of the heap-map.
size_t 	os_getMapSize (Heap const *heap) {
	return heap->mapSize;
}

// Get the size of the usable heap.
size_t 	os_getUseSize (Heap const *heap) {
	return heap->useSize;
}

// Get the start of the heap-map.
MemAddr os_getMapStart (Heap const *heap) {
	return heap->mapStart;
}

// Get the start of the usable heap.
MemAddr os_getUseStart (Heap const *heap) {
	return heap->useStart;
}

// Returns the current memory management strategy.
AllocStrategy os_getAllocationStrategy(Heap const* heap){
	return heap->strategy;
}

// Changes the memory management strategy.
void os_setAllocationStrategy(Heap *heap, AllocStrategy allocStrat){
	os_enterCriticalSection();
	heap->strategy = allocStrat;
	os_leaveCriticalSection();
}