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

MemAddr os_Memory_NextFit(Heap *heap, size_t size)
{
	if (heap->lastAddr == 0)
	{
		heap->lastAddr = heap->useStart;
	}
	size_t freeSize = 0;
	for (MemAddr start = heap->lastAddr; start < heap->useStart + heap->useSize; start++){
		if (os_getMapEntry(heap, start) == 0){
			freeSize++;
			if (freeSize == size)
			{
				if (start != heap->useStart + heap->useSize - 1)
				{
					heap->lastAddr = start + 1;
				}else{
					heap->lastAddr = heap->useStart;
				}
					return (start - size + 1);				
			}
		}
		else
		{
			freeSize = 0;
		}
	}
	freeSize = 0;
	for (MemAddr start = heap->useStart; start < heap->useStart + heap->useSize; start++){
		if (os_getMapEntry(heap, start) == 0){
			freeSize++;
			if (freeSize == size)
			{
				if (start != heap->useStart + heap->useSize - 1)
				{
					heap->lastAddr = start + 1;
				}else{
					heap->lastAddr = heap->useStart;
				}
				return (start - size + 1);
			}
		}
		else
		{
			freeSize = 0;
		}
	}
	heap->lastAddr = 0;
	return 0;
}

MemAddr os_Memory_BestFit(Heap *heap, size_t size)
{
	MemAddr bestInd = 0;
	size_t bestArea = heap->useSize + 1;
	
	for (size_t i = 0; i < heap->useSize;) {
		size_t currSize = 0;
		for (size_t j = i; j < heap->useSize; j++)
		{
			MemValue val = os_getMapEntry(heap, heap->useStart + j);
			if  (val != 0)
			{
				break;
			}
			currSize++;
		}
		
		if (currSize == size)
		{
			return heap->useStart + i;
		}
		
		if (currSize > size && currSize < bestArea)
		{
			bestInd = i;
			bestArea = currSize;
		}
		
		i += currSize + 1;
	}
	
	if (bestArea == heap->useSize + 1) {
		return 0;
	}
	
	return heap->useStart + bestInd;
}

MemAddr os_Memory_WorstFit(Heap *heap, size_t size)
{
	MemAddr worstInd = 0;
	size_t worstArea = 0;
	
	for (size_t i = 0; i < heap->useSize;)
	{
		size_t currSize = 0;
		for (size_t j = i; j < heap->useSize; j++) {
			MemValue val = os_getMapEntry(heap, heap->useStart + j);
			
			if  (val != 0)
			{
				break;
			}
			currSize++;
		}
		if (currSize >= heap->useSize / 2)
		{
			return heap->useStart + i;
		}
		if (currSize > size && currSize > worstArea)
		{
			worstInd = i;
			worstArea = currSize;
		}
		
		
		i += currSize + 1;
	}
	
	
	
	if (worstArea == 0)
	{
		return 0;
	}
	
	return heap->useStart + worstInd;
}