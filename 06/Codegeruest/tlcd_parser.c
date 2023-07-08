#include <avr/interrupt.h>
#include "tlcd_core.h"
#include "tlcd_parser.h"
#include "tlcd_button.h"
#include "util.h"
#include "lcd.h"

#warning IMPLEMENT STH. HERE

// Forward declarations
void tlcd_readData();
void tlcd_parseInputBuffer();
void tlcd_parseTouchEvent();
void tlcd_parseButtonEvent();
void tlcd_parseUnknownEvent();

/*!
 *  Interrupt Service Routine, which is calls on Pin Change Interrupt.
 *  This ISR handles the incoming data and calls the parsing process.
 */
ISR(PCINT1_vect) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Display a TouchEvent to the Character-LCD.
 *  \param event The TouchEvent to display
 */
void tlcd_displayEvent(TouchEvent event) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function parses the input buffer content by iterating over
 *  it packet wise and calling the specific parser functions.
 */
void tlcd_parseInputBuffer() {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function is called when a free touch panel event packet
 *  has been received. The content of the subframe corresponding
 *  to that event is parsed in this function and delegated to the
 *  event handler tlcd_eventHandler(WithCorrection).
 */
void tlcd_parseTouchEvent() {
    #warning IMPLEMENT STH. HERE
}


/*!
 *  This function is called when an unknown event packet
 *  has been received and skips the payload by manipulating
 *  the tail of the input buffer.
 */
void tlcd_parseUnknownEvent() {
    #warning IMPLEMENT STH. HERE
}