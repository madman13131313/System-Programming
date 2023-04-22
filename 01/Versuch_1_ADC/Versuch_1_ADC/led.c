#include "led.h"
#include <avr/io.h>

uint16_t activateLedMask;

/*!
 *  Initializes the led bar. Note: All PORTs will be set to output.
 */
void initLedBar(void) {
    DDRA = 0b11111111;
	DDRD = 0b11111111;
}

/*!
 *  Sets the passed value as states of the led bar (1 = on, 0 = off).
 */
void setLedBar(uint16_t value) {
	value &= activateLedMask;
    PORTA = ~(value & 0b11111111);
	PORTD = ~((value >> 8) & 0b00111111);
}
