#include "os_mem_drivers.h"
#include "defines.h"

MemDriver intSRAM__;

void init(void){}
	
MemValue read(MemAddr addr){
	MemValue value = 0;
	return value;
}
	
void write(MemAddr addr, MemValue value){}
	
MemDriver intSRAM__={
	.init = init,
	.read = read,
	.write = write
};