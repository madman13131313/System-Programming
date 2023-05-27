#include "os_memory_strategies.h"
#include "os_memory.h"


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
	
}

MemAddr os_Memory_BestFit(Heap *heap, size_t size){
	return 0;
}

MemAddr os_Memory_WorstFit(Heap *heap, size_t size){
	return 0;
}