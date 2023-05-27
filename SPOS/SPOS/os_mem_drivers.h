#ifndef OS_MEM_DRIVERS_H
#define OS_MEM_DRIVERS_H

#include <inttypes.h>
#include "defines.h"

typedef uint16_t MemAddr;
typedef uint8_t MemValue;

typedef void MemoryInitHnd(void);
typedef MemValue MemoryReadHnd(MemAddr addr);
typedef void MemoryWriteHnd(MemAddr addr, MemValue value);

typedef struct MemDriver {
	// Constants for the characteristics of the memory medium
	
	// Function pointers for access routines
	MemoryInitHnd *init;
	MemoryReadHnd *read;
	MemoryWriteHnd *write;
} MemDriver;

extern MemDriver intSRAM__;
#define intSRAM (&intSRAM__)

#endif