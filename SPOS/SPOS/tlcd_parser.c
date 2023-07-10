#include <avr/interrupt.h>
#include "tlcd_core.h"
#include "tlcd_parser.h"
#include "tlcd_button.h"
#include "util.h"
#include "lcd.h"
#include "os_core.h"



static EventCallback* eventCallback = NULL;

void tlcd_setEventCallback(EventCallback* callback) {
	eventCallback = callback;
}

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
    // Prüfen, ob der SBUF-Pin einen Low-Pegel hat
    if (!(TLCD_PIN & (1 << TLCD_SEND_BUFFER_IND_BIT))) {
	    // Solange den Sendepuffer des TLCDs anfordern, bis am SBUF-Pin wieder ein High-Pegel anliegt
	    while (!(TLCD_PIN & (1 << TLCD_SEND_BUFFER_IND_BIT))) {
		    // Anfordern des Sendepuffers
		    tlcd_requestData();

		    // Lesen der empfangenen Daten und Verarbeiten des Eingabepuffers
		    tlcd_readData();
		    tlcd_parseInputBuffer();
	    }
    }
}

/*!
 *  Display a TouchEvent to the Character-LCD.
 *  \param event The TouchEvent to display
 */
void tlcd_displayEvent(TouchEvent event) {
    lcd_clear();
	lcd_writeString("X: ");
	lcd_writeDec(event.x);
	lcd_goto(1, 9);
	lcd_writeString("Y: ");
	lcd_writeDec(event.y);
	lcd_goto(2, 1);
	lcd_writeString("Type: ");
	lcd_writeDec(event.type);
}

/*!
 *  This function parses the input buffer content by iterating over
 *  it packet wise and calling the specific parser functions.
 */
void tlcd_parseInputBuffer() {
    while (tlcd_hasNextBufferElement()) {
	    if (tlcd_readNextBufferElement() == ESC_BYTE)
	    {
			if (tlcd_readNextBufferElement() == H_BYTE)
			{
				tlcd_parseTouchEvent();
			}
			else
			{
				tlcd_parseUnknownEvent();
			}
	    }
    }
}

/*!
 *  This function is called when a free touch panel event packet
 *  has been received. The content of the subframe corresponding
 *  to that event is parsed in this function and delegated to the
 *  event handler tlcd_eventHandler(WithCorrection).
 */
void tlcd_parseTouchEvent() {
    uint8_t five = tlcd_readNextBufferElement();
	if (five != 5)
	{
		os_error("wrong touchEvent length");
		return;
	}
	TouchEventType eventType = (TouchEventType)tlcd_readNextBufferElement();
	uint16_t x = tlcd_readNextBufferElement() | (tlcd_readNextBufferElement() << 8);
	uint16_t y = tlcd_readNextBufferElement() | (tlcd_readNextBufferElement() << 8);

	TouchEvent touchEvent;
	touchEvent.x = x;
	touchEvent.y = y;
	touchEvent.type = eventType;
	tlcd_displayEvent(touchEvent);
	if (!tlcd_handleButtons(touchEvent))
	{
		if (eventCallback != NULL) {
			eventCallback(touchEvent);
		}
	}
	
}


/*!
 *  This function is called when an unknown event packet
 *  has been received and skips the payload by manipulating
 *  the tail of the input buffer.
 */
void tlcd_parseUnknownEvent() {
    uint8_t length = tlcd_readNextBufferElement();
	for (uint8_t i = 0; i < length; i++)
	{
		tlcd_readNextBufferElement();
	}
}