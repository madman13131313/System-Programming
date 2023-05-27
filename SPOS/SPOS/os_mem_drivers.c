#include "os_mem_drivers.h"
#include "defines.h"


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