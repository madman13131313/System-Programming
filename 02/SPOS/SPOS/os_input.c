#include "os_input.h"

#include <avr/io.h>
#include <stdint.h>

/*! \file

Everything that is necessary to get the input from the Buttons in a clean format.

*/

/*!
 *  A simple "Getter"-Function for the Buttons on the evaluation board.\n
 *
 *  \returns The state of the button(s) in the lower bits of the return value.\n
 *  example: 1 Button:  -pushed:   00000001
 *                      -released: 00000000
 *           4 Buttons: 1,3,4 -pushed: 000001101
 *
 */
uint8_t os_getInput(void) {
    // Invert PINC and filter out unwanted bits
    const uint8_t pressed = (~PINC) & 0b11000011;
    // Combine the two bottom bits with the inverted upper bits
    return (pressed & 0b00000011) | (pressed >> 4);
}

/*!
 *  Initializes DDR and PORT for input
 */
void os_initInput() {
     // Set the input pins of PORTC to input
     DDRC &= 0b11000011;
     // Enable the pull-up resistors for the input pins
     PORTC |= 0b11000011;
}

/*!
 *  Endless loop as long as at least one button is pressed.
 */
void os_waitForNoInput() {
    // Wait until no button is pressed
    while (os_getInput() != 0);
}

/*!
 *  Endless loop until at least one button is pressed.
 */
void os_waitForInput() {
    // Wait until a button is pressed
    while (os_getInput() == 0);
