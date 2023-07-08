/*
 * os_mem_drivers.h
 *
 * Created: 2023/5/15 14:31:39
 *  Author: 11481
 */ 

#ifndef _OS_MEM_DRIVERS_H
#define _OS_MEM_DRIVERS_H

#include <inttypes.h>
#include "defines.h"

#define intSRAM (&intSRAM__)
#define extSRAM (&extSRAM__)

// 23LC1024 Instructions
#define LC1024READ 0x03
#define LC1024WRITE 0x02
#define LC1024RDMR 0x05 //Read Mode Register
#define LC1024WRMR 0x01 //Write Mode Register

//Adresse von den Daten im SRAM
typedef uint16_t MemAddr;

//durch MemAddr referenzierte Speicheratome
typedef uint8_t MemValue;

typedef void MemoryInitHnd(void);

typedef MemValue MemoryReadHnd(MemAddr addr);

typedef void MemoryWriteHnd(MemAddr addr, MemValue value);

//Speichertreiber
typedef struct MemDriver{
	MemAddr start; //erste verf¨¹gbare Adresses
	uint16_t size; //Gr??e des Speichermediums
	MemoryInitHnd *init; 
	MemoryReadHnd *read; 
	MemoryWriteHnd *write; 
} MemDriver;


void initMemoryDevices(void);

MemDriver intSRAM__;
MemDriver extSRAM__;

#endif