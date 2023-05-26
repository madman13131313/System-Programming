#ifndef OS_MEM_DRIVERS_H
#define OS_MEM_DRIVERS_H

#include <inttypes.h>
#include "defines.h"

typedef uint16_t MemAddr;
typedef uint8_t MemValue;

typedef struct MemDriver {
	// Constants for the characteristics of the memory medium
	
	// Function pointers for access routines
	void (*init)(void);
	MemValue (*read)(MemAddr addr);
	void (*write)(MemAddr addr, MemValue value);
} MemDriver;


#define intSRAM (&intSRAM__)

#endif