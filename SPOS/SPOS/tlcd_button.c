#include "tlcd_button.h"
#include "tlcd_graphic.h"
#include "tlcd_parser.h"
#include "os_core.h"
#include "defines.h"

Button buttonBuffer[MAX_BUTTONS];
uint8_t numButtons = 0;


ButtonCallback* buttonCallback = NULL;
/*!
 *  Add a button to the screen and internal logic to be handled by the handleButtons function.
 *  In addition the button will present a character as label.
 *  \param x1 X-coordinate of the starting point
 *  \param y1 Y-coordinate of the starting point
 *  \param x2 X-coordinate of the ending point
 *  \param y2 Y-coordinate of the ending point
 *  \param color The color index of the color the button should be drawn in
 *  \param downCode The code to be send if the button is pressed
 *  \param c The character to display on the button
 */
void tlcd_addButtonWithChar(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, uint8_t downCode, char c) {
    if (numButtons < MAX_BUTTONS) {
	    Button button;
	    button.x1 = x1;
	    button.y1 = y1;
	    button.x2 = x2;
	    button.y2 = y2;
	    button.color = color;
	    button.downCode = downCode;
	    button.c = c;
	    
	    buttonBuffer[numButtons] = button;
	    numButtons++;
    }
	else{
		os_error("too many buttons");
	}
};

/*!
 *  Draw the buttons onto the screen.
 *  This function should be called whenever the screen was cleared.
 */
void tlcd_drawButtons() {
    for (uint8_t i = 0; i < numButtons; i++) {
	    Button button = buttonBuffer[i];
		if (buttonBuffer[i].color != 0) {
			tlcd_drawBox(button.x1, button.y1, button.x2, button.y2, button.color);
		}
		if (buttonBuffer[i].c != ' ') {
			tlcd_drawChar((button.x1 + button.x2)/2, (button.y1 + button.y2)/2, button.c);
		}
    }
}

/*!
 *  Check an event against all buttons for collision and execute button function if a button was pressed.
 *  Will return 1 if the event was inside a button and a 0 otherwise.
 *  \param event The touchevent to handle
 *  \return 1 if event was handled, 0 otherwise
 */
uint8_t tlcd_handleButtons(TouchEvent event) {
    uint8_t isHandled = 0;
    uint16_t x = event.x;
    uint16_t y = event.y;
    
    for (uint8_t i = 0; i < numButtons; i++) {
	    Button button = buttonBuffer[i];
	    
	    if (x >= button.x1 && x <= button.x2 && y >= button.y1 && y <= button.y2) {
		    // Button-Kollision festgestellt
		    if (event.type == TOUCHPANEL_DOWN) {
			    // Button-Callback aufrufen
			    if (buttonCallback != NULL) {
				    buttonCallback(button.downCode, x, y);
			    }
			    
		    }
			isHandled = 1;
		    break;
	    }
    }
    
    return isHandled;
}

void tlcd_setButtonCallback(ButtonCallback* callback) {
	buttonCallback = callback;
}
