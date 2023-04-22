#include "os_input.h"

#include <avr/io.h>
#include <stdint.h>

/*! \file
 * Everything that is necessary to get the input from the Buttons in a clean format.
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
    // filter out unwanted bits
    const uint8_t pressed = (~PINC) & 0b00000010;
    return pressed;
}

/*!
 *  Initializes DDR and PORT for input
 */
void os_initInput() {
     // Pin C1 als Eingang konfigurieren (Bit 1 von DDRC auf 0 setzen)
     DDRC &= 0b11111101;
     // Pullup -Widerstand an Pin C1 aktivieren (Bit 1 von PORTC auf 1 setzen)
     PORTC |= 0b00000010;
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
}
