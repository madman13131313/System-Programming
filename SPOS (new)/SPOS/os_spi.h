#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "util.h"

void os_spi_init(void);
uint8_t os_spi_send(uint8_t data);
uint8_t os_spi_recive();