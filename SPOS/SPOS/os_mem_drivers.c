#include "os_mem_drivers.h"
#include "defines.h"
#include "os_scheduler.h"
#include "os_spi.h"
#include "util.h"
#include <avr/io.h>
#include <avr/interrupt.h>


void initSRAM_internal(void){}
	
MemValue readSRAM_internal(MemAddr addr){
	return *((uint8_t*)addr);
}
	
void writeSRAM_internal(MemAddr addr, MemValue value){
	*((uint8_t*)addr) = value;
}
	
MemDriver intSRAM__={
	.init = &initSRAM_internal,
	.read = &readSRAM_internal,
	.write = &writeSRAM_internal
};

// Activates the external SRAM as SPI slave.
void select_memory(){
	PORTB &= 0b11101111; // Set CS pin low
}
	
// Deactivates the external SRAM as SPI slave.
void deselect_memory(){
	PORTB |= 0b00010000; // Set CS pin high
}
	
// Sets the operation mode of the external SRAM.
void set_operation_mode(uint8_t mode){
	os_enterCriticalSection();
	select_memory();
	os_spi_send(0x01); // Sende den Befehl, um MODE register zu schreiben
	os_spi_send(mode); // Sende den aktualisierten MODE register Wert
	deselect_memory();
	os_leaveCriticalSection();
}
	
// Transmitts a 24bit memory address to the external SRAM.
void transfer_address(MemAddr addr){
	 os_spi_send(0x00);
	 os_spi_send((addr >> 8) & 0xFF);
	 os_spi_send(addr & 0xFF);
}
	
void initSRAM_external(void){
	os_spi_init();
	select_memory();
	set_operation_mode(0);
	deselect_memory();
}

// Private function to read a single byte to the external SRAM It will not check if its call is valid.
MemValue readSRAM_external(MemAddr addr){
	os_enterCriticalSection();
	select_memory();
	os_spi_send(0x03);
	transfer_address(addr);
	MemValue data = os_spi_receive();
	deselect_memory();
	os_leaveCriticalSection();
	return data;
}
	
// Private function to write a single byte to the external SRAM It will not check if its call is valid.	
void writeSRAM_external(MemAddr addr, MemValue value){
	os_enterCriticalSection();
	select_memory();
	os_spi_send(0x02);
	transfer_address(addr);
	os_spi_send(value);
	deselect_memory();
	os_leaveCriticalSection();
}	
	
// Function that needs to be called once in order to initialise all used memories such as the internal SRAM etc
void initMemoryDevices(void){
	initSRAM_internal();
	initSRAM_external();
}	
	
MemDriver extSRAM__={
	.init = &initSRAM_external,
	.read = &readSRAM_external,
	.write = &writeSRAM_external
};	