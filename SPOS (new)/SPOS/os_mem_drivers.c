/*
 * os_mem_drivers.c
 *
 * Created: 2023/5/15 14:37:46
 *  Author: 11481
 */ 
#include "os_mem_drivers.h"
#include "defines.h"
#include "os_scheduler.h"
#include "os_spi.h"
#include "util.h"
#include <avr/io.h>
#include <avr/interrupt.h>

//initialisieren
void initSRAM_internal(void){}
	


//Wert an addr auslesen und return
MemValue readSRAM_internal(MemAddr addr){
	return *((MemValue*)addr);
}

//Speichern value an Adresse addr
void writeSRAM_internal(MemAddr addr, MemValue value){
	*((MemValue*)addr) = value;
}

//Eine Instanz des Speichertreibers mit Name intSRAM__
MemDriver intSRAM__ = {
	.start = 0x100,
	.size = AVR_MEMORY_SRAM,
	.init = initSRAM_internal,
	.read = readSRAM_internal,
	.write = writeSRAM_internal
};

//Activates the external SRAM as SPI slave.
void select_memory() {
	//Assumes that SPI is correctly initialized and connected to PINS B4-B7
	PORTB &= 0b11101111;
}

//Deactivates the external SRAM as SPI slave.
void deselect_memory() {
	PORTB |= 0b00010000;
}

//Sets the operation mode of the external SRAM. Does not select External SRAM. This needs to be done in the calling function.
void set_operation_mode (uint8_t mode) {
	os_spi_send(LC1024WRMR);
	os_spi_send(mode);
}

//Transmits a 24bit memory address to the external SRAM.
void transfer_address (MemAddr addr) {
	os_spi_send(0x00);
	os_spi_send((uint8_t)(addr >> 8));
	os_spi_send((uint8_t)addr);
}

// Initialize the external SRAM
void initSRAM_external() {
	os_spi_init();
	select_memory();
	set_operation_mode(0x00);
	deselect_memory();
}

void initMemoryDevices (void){
	initSRAM_internal();
	initSRAM_external();
}

//Private function to read a single byte to the external SRAM It will not check if its call is valid. This has to be done on a higher level.
MemValue readSRAM_external(MemAddr addr) {
	volatile MemAddr res;
	os_enterCriticalSection();
	select_memory();
	os_spi_send(LC1024READ);
	transfer_address(addr);
	res = os_spi_recive();
	deselect_memory();
	os_leaveCriticalSection();
	return res;
}

//Private function to write a single byte to the external SRAM It will not check if its call is valid.
void writeSRAM_external(MemAddr addr, MemValue value){
	os_enterCriticalSection();
	select_memory();
	os_spi_send(LC1024WRITE);
	transfer_address(addr);
	os_spi_send(value);
	deselect_memory();
	os_leaveCriticalSection();
}

//MemDriver for external SRAM using 23LC1024 connected to Pin B4-B7.
MemDriver extSRAM__ = {
	.start = 0,
	.size = 0xFFFF, 
	//0xFF is the bets Approximation of the Size. The lowest address is 0. The highest address is 65535. Limited by size of MemAddr.
	//Actual Size including Byte 0 is 0x10000 (65.536) 
	.init = initSRAM_external,
	.read = readSRAM_external,
	.write = writeSRAM_external,
};



