#include "tlcd_button.h"
#include "tlcd_graphic.h"
#include "tlcd_parser.h"

#warning IMPLEMENT STH. HERE

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
    #warning IMPLEMENT STH. HERE
};

/*!
 *  Draw the buttons onto the screen.
 *  This function should be called whenever the screen was cleared.
 */
void tlcd_drawButtons() {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Check an event against all buttons for collision and execute button function if a button was pressed.
 *  Will return 1 if the event was inside a button and a 0 otherwise.
 *  \param event The touchevent to handle
 *  \return 1 if event was handled, 0 otherwise
 */
uint8_t tlcd_handleButtons(TouchEvent event) {
    #warning IMPLEMENT STH. HERE
    return 0;
}