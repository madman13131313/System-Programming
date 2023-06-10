#include "os_memory_strategies.h"
#include "os_memory.h"

MemAddr lastAddr = 0;

MemAddr os_Memory_FirstFit(Heap *heap, size_t size){
	size_t freeSize = 0;
	for (MemAddr start = heap->useStart; start < heap->useStart + heap->useSize; start++){
		if (os_getMapEntry(heap, start) == 0){
			freeSize++;
			if (freeSize == size)
			{
				return (start - size + 1);
			}
		}
		else
		{
			freeSize = 0;
		}
	}
	return 0;
}

MemAddr os_Memory_NextFit(Heap *heap, size_t size){
	if (lastAddr == 0)
	{
		lastAddr = heap->useStart;
	}
	size_t freeSize = 0;
	for (MemAddr start = lastAddr; start < heap->useStart + heap->useSize; start++){
		if (os_getMapEntry(heap, start) == 0){
			freeSize++;
			if (freeSize == size)
			{
				lastAddr = start + 1;
				return (start - size + 1);
			}
		}
		else
		{
			freeSize = 0;
		}
	}
	for (MemAddr start = heap->useStart; start < lastAddr; start++){
		if (os_getMapEntry(heap, start) == 0){
			freeSize++;
			if (freeSize == size)
			{
				lastAddr = start + 1;
				return (start - size + 1);
			}
		}
		else
		{
			freeSize = 0;
		}
	}
	return 0;
}

MemAddr os_Memory_BestFit(Heap *heap, size_t size){
	size_t freeSize = 0;
	size_t min = heap->useSize;
	MemAddr addr = 0;
	for (MemAddr start = heap->useStart; start < heap->useStart + heap->useSize; start++){
		if (os_getMapEntry(heap, start) == 0){
			freeSize++;
			
		}
		else
		{
			if ((freeSize >= size) && (freeSize < min))
			{
				min = freeSize;
				addr = start - freeSize;
			}
			freeSize = 0;
		}
	}
	return addr;
}

MemAddr os_Memory_WorstFit(Heap *heap, size_t size){
	size_t freeSize = 0;
	size_t max = 0;
	MemAddr addr = 0;
	for (MemAddr start = heap->useStart; start < heap->useStart + heap->useSize; start++){
		if (os_getMapEntry(heap, start) == 0){
			freeSize++;
			
		}
		else
		{
			if ((freeSize >= size) && (freeSize > max))
			{
				max = freeSize;
				addr = start - freeSize;
			}
			freeSize = 0;
		}
	}
	return addr;
}