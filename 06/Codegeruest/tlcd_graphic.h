/*! \file
 *  \brief Graphic library for the touchpanel EA eDIPTFT43-ATP
 *
 *  This module includes all functions that are needed to use the graphic functions of the EA eDIPTFT43-ATP.
 */

#ifndef _tlcd_H
#define _tlcd_H

#include <stdint.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Color;

//! Defines a free touch area. Events that occur in this area are catched
void tlcd_defineTouchArea(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

//! Draws a line on the display with a given start and endpoint
void tlcd_drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

//! Draws a point to a given location on the display
void tlcd_drawPoint(uint16_t x1, uint16_t y1);

//! Changes the draw width
void tlcd_changePenSize(uint8_t size);

//! Changes the draw color to another defined color
void tlcd_changeDrawColor(uint8_t color);

//! Draws a colored box to a given location on the display
void tlcd_drawBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color);

//! Defines a color which can be refered to using the registered colorID
void tlcd_defineColor(uint8_t colorID, Color color);

//! Draws a char to a given location on the display
void tlcd_drawChar(uint16_t x1, uint16_t y1, char c);

//! Enables or disables the cursor on the display
void tlcd_setCursor(uint8_t enabled);

//! Clears the display
void tlcd_clearDisplay();

#endif
