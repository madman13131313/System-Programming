/*
 * os_memory_strategies.c
 *
 * Created: 2023/5/16 1:11:45
 *  Author: 11481
 */ 
#include "os_memory_strategies.h"
#include "os_memory.h"
#include "os_core.h"


/* MemAddr firstFit_Hilfe(Heap *heap, size_t size, MemAddr start, MemAddr end){
	for (MemAddr addr = start; addr + size - 1 < end; addr++){
		size_t s = 0;
		while (os_getMapEntry(heap,addr + s) == 0x00 && addr + s < end){
			++s;
		}
		if (s >= size){
			return addr;
		}
		addr += s;
	}
	return 0;
}
*/

MemAddr firstFit_Hilfe(Heap *heap, size_t size, MemAddr start, MemAddr end) {
	size_t s = 0;
	for (uint32_t i = start; i < end; i++) {
		if (os_getMapEntry(heap,i) != 0x00) {
			s = 0;
		} else {
			s++;
			if (s == size) {
				return i-s+1;
			}
		}
	}
	return 0;
}




MemAddr os_Memory_FirstFit (Heap *heap, size_t size){
	return firstFit_Hilfe(heap,size,heap->useStart,heap->useStart+heap->useSize);
}

MemAddr MemNextHelper(Heap *heap, size_t size, MemAddr start, uint32_t until) {
	size_t s = 0;
	for (uint32_t i = start; i <= until; i++ ) {
		if (os_getMapEntry(heap,i) == 0x00) {
			s++;
			if (s == size) {return i-size+1;}
		} else {
			s = 0;
		}
	}
	return 0;
}

MemAddr os_Memory_NextFit(Heap *heap, size_t size) {
	MemAddr res =  MemNextHelper(heap,size,heap->lastAllocation,HIGHESTADDRESSUSERHEAP(heap));
	if (res == 0) {
		res = MemNextHelper(heap,size,heap->useStart,(uint32_t)heap->lastAllocation + size < HIGHESTADDRESSUSERHEAP(heap) ? heap->lastAllocation + size : HIGHESTADDRESSUSERHEAP(heap));
	}
	if (res == 0) {return 0;}
	heap->lastAllocation = (uint32_t)res + size < HIGHESTADDRESSUSERHEAP(heap) ? res + size : HIGHESTADDRESSUSERHEAP(heap);
	/*if (heap->lastAllocation < heap->useStart || heap->lastAllocation > HIGHESTADDRESSUSERHEAP(heap)) {
		os_errorPStr(PSTR("Addr OoB. in MEM_Next."));
	} */
	return res;
} 

/*
MemAddr os_Memory_NextFit (Heap *heap, size_t size){
	if(letzt == 0){
		if(os_Memory_FirstFit(heap,size) == 0){
			return 0;
		}
		letzt = os_Memory_FirstFit(heap,size) - heap->useStart + size;
		return os_Memory_FirstFit(heap,size);
	}	
	MemAddr result = firstFit_Hilfe(heap,size,letzt+heap->useStart,heap->useStart+heap->useSize);	
	if(result != 0){
		return result;
	}
	return firstFit_Hilfe(heap,size,heap->useStart,letzt+heap->useStart);
}
*/

MemAddr os_Memory_BestFit (Heap *heap, size_t size){
	MemAddr addr =  heap->useStart;
	MemAddr result = 0;
	size_t min = heap->useSize;
	
	while (addr < heap->useSize +  heap->useStart){
		if(os_getChunkSize(heap,addr) != 0){
			addr += os_getChunkSize(heap,addr);
		}else{
			size_t s = 0;
			while(os_getMapEntry(heap,addr) == 0x00 && addr < heap->useSize +  heap->useStart){
				s++;
				addr++;
			}
			if(s == size){
				result = addr - s;
				return result;
			}
			if(s < min && s >=size){
				min = s;
				result = addr - s;
			}
		}
	}
	return result;
}

MemAddr os_Memory_WorstFit (Heap *heap, size_t size){
	MemAddr addr =  heap->useStart;
	MemAddr result = 0;
	size_t max = 0;
	
	while (addr < heap->useSize +  heap->useStart){
		if(os_getChunkSize(heap,addr) != 0){
			addr += os_getChunkSize(heap,addr);
		}else{
			size_t s = 0;
			while(os_getMapEntry(heap,addr) == 0x00 && addr < heap->useSize +  heap->useStart){
				s++;
				addr++;
			}
			if(s*2 >= heap->useSize){
				result = addr - s;
				return result;
			}
			if(s > max){
				max = s;
				result = addr - s;
			}
		}
	}
	if(max < size){
		return 0;
	}
	return result;
}
