/*
 * os_memory.h
 *
 * Created: 2023/5/15 23:56:24
 *  Author: 11481
 */ 

#ifndef _OS_MEMORY_H
#define _OS_MEMORY_H

#include "os_mem_drivers.h"
#include "os_memheap_drivers.h"
#include "os_scheduler.h"

#define SHARED_CLOSED		8
#define SHARED_WRITE_OPEN	9
#define SHARED_READ_OPEN	10

MemAddr os_sh_malloc(Heap *heap, size_t size);

void os_sh_free(Heap *heap, MemAddr *addr);

void os_sh_write(Heap const *heap, MemAddr const *ptr, uint16_t offset, MemValue const *dataScr, uint16_t length);

void os_sh_read(Heap const *heap, MemAddr const *ptr, uint16_t offset, MemValue *dataDest, uint16_t length);

MemAddr os_sh_readOpen(Heap const *heap, MemAddr const *ptr);

MemAddr os_sh_writeOpen(Heap const *heap, MemAddr const *ptr);

void os_sh_close(Heap const *heap, MemAddr addr);

MemAddr os_malloc(Heap *heap, size_t size);

void os_free(Heap *heap, MemAddr addr);

void os_freeOwnerRestricted(Heap *heap, MemAddr addr, ProcessID owner);

MemAddr os_realloc (Heap *heap, MemAddr addr, uint16_t size);

MemValue os_getMapEntry(Heap const *heap, MemAddr addr);

void os_freeProcessMemory(Heap *heap, ProcessID pid);

size_t os_getMapSize(Heap const *heap);

size_t os_getUseSize(Heap const *heap);

MemAddr os_getMapStart(Heap const *heap);

MemAddr os_getUseStart(Heap const *heap);

uint16_t os_getChunkSize(Heap const *heap, MemAddr addr);

MemAddr os_getFirstByteOfChunk(Heap const *heap, MemAddr addr);

void os_setAllocationStrategy(Heap *heap, AllocStrategy allocStrat);

AllocStrategy os_getAllocationStrategy(Heap const *heap);

#endif