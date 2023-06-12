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
	MemAddr temp = addr - (heap->useStart);
	if (temp % 2 == 0)
	{
		setHighNibble(heap, (heap->mapStart + temp / 2), value);
	}else{
		setLowNibble(heap, (heap->mapStart + temp / 2), value);
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
MemAddr os_malloc(Heap* heap, size_t size){
	os_enterCriticalSection();
	MemAddr allocStart = 0;
	ProcessID current = os_getCurrentProc();
	if (current == 0)
	{
		os_error("Current Process idle");
		os_leaveCriticalSection();
		return 0;
	}
	if ((size > heap->useSize) || (size == 0))
	{
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
		// Optimierung: renew the starting/ending frame of the process
		if ((heap->allocFrameStart[current] == 0) || ( heap->allocFrameStart[current] > allocStart))
		{
			heap->allocFrameStart[current] = allocStart;
		}
		if ((heap->allocFrameEnd[current] == 0) || ( heap->allocFrameEnd[current] < allocStart + size -1))
		{
			heap->allocFrameEnd[current] = allocStart + size -1;
		}
		os_leaveCriticalSection();
		return allocStart;
	}
	os_leaveCriticalSection();
	return 0;
	
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
		return;
	}
	MemAddr firstByte = os_getFirstByteOfChunk(heap, addr);
	if (owner == os_getMapEntry(heap, firstByte) ) {
		MemAddr lastByte = os_getFirstByteOfChunk(heap, addr) + os_getChunkSize(heap, addr);
		for( ; firstByte < lastByte; firstByte++) {
			setMapEntry(heap, firstByte, 0);
		}
	}
}

// Function used by processes to free their own allocated memory.
void os_free (Heap *heap, MemAddr addr){
	os_enterCriticalSection();
	ProcessID owner = os_getCurrentProc();
	MemAddr firstByte = os_getFirstByteOfChunk(heap,addr);
	MemAddr lastByte = os_getFirstByteOfChunk(heap, addr) + os_getChunkSize(heap, addr);
	os_freeOwnerRestricted(heap, addr, owner);
	// renew the starting/ending frame of the process
	// find the new start frame of the process
	if (heap->allocFrameStart[owner] == firstByte)
	{
		heap->allocFrameStart[owner] = 0;
		MemAddr x = firstByte + 1;
		while (x < heap->useStart + heap->useSize)
		{
			if (os_getMapEntry(heap, x) == owner)
			{
				heap->allocFrameStart[owner] = x;
				break;
			}
			x++;
		}
	}
	// find the new end frame of the process
	if (heap->allocFrameEnd[owner] == lastByte - 1)
	{
		heap->allocFrameEnd[owner] = 0;
		MemAddr y = firstByte - 1;
		while (y >= heap->useStart)
		{
			if (getOwnerOfChunk(heap, y) == owner)
			{
				heap->allocFrameEnd[owner] = y;
				break;
			}
			y--;
		}
	}
	os_leaveCriticalSection();
}

// Function that realises the garbage collection.
void os_freeProcessMemory(Heap *heap, ProcessID pid){
	os_enterCriticalSection();
	for (MemAddr i = heap->allocFrameStart[pid]; i <= heap->allocFrameEnd[pid]; i++)
	{
		os_freeOwnerRestricted(heap, i, pid);
	}
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

// This will move one Chunk to a new location,
// To provide this the content of the old one is copied to the new location,
// as well as all Map Entries are set properly since this is a helper function for reallocation,
// it only works if the new Chunk is bigger than the old one.
void moveChunk(Heap *heap, MemAddr oldChunk, size_t oldSize, MemAddr newChunk, size_t newSize){
	if (newSize > oldSize)
	{
		ProcessID pid = getOwnerOfChunk(heap, oldChunk);
		os_free(heap, oldChunk);
		// copy the values stored in the old chunk
		for(MemAddr i = oldChunk; i < oldChunk + oldSize; ++i) {
			heap->driver->write(newChunk + (i - oldChunk), heap->driver->read(i));
		}
		// renew the map entries
		setMapEntry(heap, newChunk, pid);
		for(MemAddr i = newChunk + 1; i < newChunk + newSize; ++i) {
			setMapEntry(heap, i, 15);
		}
		// Optimierung: renew the starting/ending frame of the process
		if ((heap->allocFrameStart[pid] == 0) || ( heap->allocFrameStart[pid] > newChunk))
		{
			heap->allocFrameStart[pid] = newChunk;
		}
		if ((heap->allocFrameEnd[pid] == 0) || ( heap->allocFrameEnd[pid] < newChunk + newSize -1))
		{
			heap->allocFrameEnd[pid] = newChunk + newSize -1;
		}
	}
}

// This is an efficient reallocation routine.
// It is used to resize an existing allocated chunk of memory.
// If possible, the position of the chunk remains the same.
// It is only searched for a completely new chunk if everything else does not fit.
MemAddr os_realloc(Heap *heap, MemAddr addr, uint16_t size){
	os_enterCriticalSection();
	ProcessID pid = os_getCurrentProc();
	if (pid == 0)
	{
		os_error("Current Process idle");
		os_leaveCriticalSection();
		return 0;
		}else{
		MemAddr firstByte = os_getFirstByteOfChunk(heap, addr);
		// Ein Prozess darf ausschlie�lich von ihm selbst allozierten Speicher reallozieren.
		if(os_getMapEntry(heap,firstByte) != pid) {
			os_leaveCriticalSection();
			return 0;
		}
		uint16_t oldSize = os_getChunkSize(heap, addr);
		// If the new size is smaller than the old size, the chunk does not need to move.
		// We only need to free the redundant memory.
		if (size <= oldSize) {
			for(MemAddr i = firstByte + size; i < firstByte + oldSize; ++i) {
				setMapEntry(heap ,i, 0); // free the redundant memory by setting the corresponding map nibble
			}
			os_leaveCriticalSection();
			return addr;
		}
		// If the new size is not smaller than the old size, we need to find a new chunk to reallocate.
		// 1. We check if there is enough space directly after the original chunk.
		MemAddr firstAfterOld = firstByte + oldSize;
		// find out the first occupied byte after the old chunk and so the free space after the old chunk.
		for(; firstAfterOld < heap->useStart+heap->useSize; ++firstAfterOld) {
			if(os_getMapEntry(heap,firstAfterOld) != 0) {
				break;
			}
		}
		uint16_t spaceAfterOld = firstAfterOld - (firstByte + oldSize);
		if(size - oldSize <= spaceAfterOld) {
			moveChunk(heap, firstByte, oldSize, firstByte, size);
			// no need to free anything
			os_leaveCriticalSection();
			return firstByte;
		}
		// 2. If there is no enough space after the old chunk,
		// We continue to check the space right before the chunk to see if the space after and the space before together is enough?
		MemAddr firstBeforeOld = firstByte - 1;
		// find out the first occupied byte before the old chunk and so the free space before the old chunk.
		for(; firstBeforeOld >= heap->useStart; firstBeforeOld--) {
			if(os_getMapEntry(heap,firstBeforeOld) != 0) {
				break;
			}
		}
		uint16_t spaceBeforeOld = firstByte - firstBeforeOld - 1;
		if (size - oldSize <= spaceAfterOld + spaceBeforeOld){
			moveChunk(heap, firstByte, oldSize, firstBeforeOld + 1, size);
			os_leaveCriticalSection();
			return firstBeforeOld + 1;
		}
		// 3. If the space before and after together is still not enough, we use malloc to allocate a new chunk and release the old chunk.
		MemAddr realloc = os_malloc(heap, size);
		// nicht gen�gend Speicher zur Verf�gung steht
		if (realloc == 0) {
			os_leaveCriticalSection();
			return 0;
			}else{
			moveChunk(heap, firstByte, oldSize, realloc, size);
			os_leaveCriticalSection();
			return realloc;
		}
	}
}

