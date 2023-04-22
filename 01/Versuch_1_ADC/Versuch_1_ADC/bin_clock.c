#include "bin_clock.h"
#include <avr/io.h>
#include <avr/interrupt.h>

//! Global variables
uint8_t hour;
uint8_t minute;
uint8_t second;
uint16_t millisecond;

/*!
 * \return The milliseconds counter of the current time.
 */
uint16_t getTimeMilliseconds() {
    return millisecond;
}

/*!
 * \return The seconds counter of the current time.
 */
uint8_t getTimeSeconds() {
    return second;
}

/*!
 * \return The minutes counter of the current time.
 */
uint8_t getTimeMinutes() {
    return minute;
}

/*!
 * \return The hour counter of the current time.
 */
uint8_t getTimeHours() {
    return hour;
}

/*!
 *  Initializes the binary clock (ISR and global variables)
 */
void initClock(void) {
    // Set timer mode to CTC
    TCCR0A &= ~(1 << WGM00);
    TCCR0A |= (1 << WGM01);
    TCCR0B &= ~(1 << WGM02);

    // Set prescaler to 1024
    TCCR0B |= (1 << CS02) | (1 << CS00);
    TCCR0B &= ~(1 << CS01);

    // Set compare register to 195 -> match every 10ms
    OCR0A = 195;

    // Init variables
    hour = 12;
	minute = 0;
	second = 0;
	millisecond = 0;

    // Enable timer and global interrupts
    TIMSK0 |= (1 << OCIE0A);
    sei();
}

/*!
 *  Updates the global variables to get a valid 12h-time
 */
void updateClock(void) {
	second++; // Increase the second counter
	if (second >= 60) // check if 1 minute has passed
	{
		second = 0;
		minute++;
		if (minute >= 60) // check if 1 hour has passed
		{
			minute = 0;
			hour++;
			if (hour > 12) // check if 12 hours have passed
			{
				hour = 1;
			}
		}
	}
}

/*!
 *  ISR to increase second-counter of the clock
 */
ISR(TIMER0_COMPA_vect) {
    millisecond += 10; // Increase the millisecond counter
	if (millisecond >= 1000){ // check if 1 second has passed
		millisecond = 0;
		updateClock();
	}
}
