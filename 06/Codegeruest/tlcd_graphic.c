#include <stdint.h>
#include "tlcd_core.h"
#include "tlcd_graphic.h"

/*!
 *  Define a touch area for which the TLCD will send touch events
 *  \param x1 X-coordinate of the starting point
 *  \param y1 Y-coordinate of the starting point
 *  \param x2 X-coordinate of the ending point
 *  \param y2 Y-coordinate of the ending point
 */
void tlcd_defineTouchArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function draws a point (x1,y1) on the display.
 *  \param x1 The position on the x-axis.
 *  \param y1 The position on the y-axis.
 */
void tlcd_drawPoint(uint16_t x1, uint16_t y1) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function draws a line in graphic mode from the point (x1,y1) to (x2,y2)
 *  \param x1 X-coordinate of the starting point
 *  \param y1 Y-coordinate of the starting point
 *  \param x2 X-coordinate of the ending point
 *  \param y2 Y-coordinate of the ending point
 */
void tlcd_drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Draw a colored box at given coordinates
 *  \param x1 X-coordinate of the starting point
 *  \param y1 Y-coordinate of the starting point
 *  \param x2 X-coordinate of the ending point
 *  \param y2 Y-coordinate of the ending point
 */
void tlcd_drawBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Change the size of lines being drawn
 *  \param size The new size for the lines
 */
void tlcd_changePenSize(uint8_t size) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Define the color at a given index. Not all bits are used, refer to datasheet.
 *  \param colorID The color index to change.
 *  \param The color to set.
 */
void tlcd_defineColor(uint8_t colorID, Color color) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Change the color lines are drawn in
 *  \param color Index of the color lines should be drawn in
 */
void tlcd_changeDrawColor(uint8_t color) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Draw a character c at position (x1,y1).
 *  \param c Character to draw
 *  \param x1 X coordiate
 *  \param x1 Y coordinate
 */
void tlcd_drawChar(uint16_t x1, uint16_t y1, char c) {
    #warning IMPLEMENT STH. HERE
}

/*! 
 *  Enable or disable the Cursor.
 *  \param enable Enable(1) or disable(0) the cursor.
 */
void tlcd_setCursor(uint8_t enabled) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function clears the display (fills hole display with background color).
 */
void tlcd_clearDisplay() {
    #warning IMPLEMENT STH. HERE
}
