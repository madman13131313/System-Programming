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
	unsigned char command[] = {
		ESC_BYTE,
		A_BYTE,
		H_BYTE,
		LOW(x1),
		HIGH(x1),
		LOW(y1),
		HIGH(y1),
		LOW(x2),
		HIGH(x2),
		LOW(y2),
		HIGH(y2)
	};
	// Befehl an das TLCD senden
	tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  This function draws a point (x1,y1) on the display.
 *  \param x1 The position on the x-axis.
 *  \param y1 The position on the y-axis.
 */
void tlcd_drawPoint(uint16_t x1, uint16_t y1) {
    unsigned char command[] = {
		ESC_BYTE,
	    G_BYTE,
	    P_BYTE,
	    LOW(x1),
	    HIGH(x1),
	    LOW(y1),
	    HIGH(y1)
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  This function draws a line in graphic mode from the point (x1,y1) to (x2,y2)
 *  \param x1 X-coordinate of the starting point
 *  \param y1 Y-coordinate of the starting point
 *  \param x2 X-coordinate of the ending point
 *  \param y2 Y-coordinate of the ending point
 */
void tlcd_drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    unsigned char command[] = {
		ESC_BYTE,
	    G_BYTE,
	    D_BYTE,
	    LOW(x1),
	    HIGH(x1),
	    LOW(y1),
	    HIGH(y1),
	    LOW(x2),
	    HIGH(x2),
	    LOW(y2),
	    HIGH(y2)
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  Draw a colored box at given coordinates
 *  \param x1 X-coordinate of the starting point
 *  \param y1 Y-coordinate of the starting point
 *  \param x2 X-coordinate of the ending point
 *  \param y2 Y-coordinate of the ending point
 */
void tlcd_drawBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color) {
    unsigned char command[] = {
		ESC_BYTE,
	    R_BYTE,
	    F_BYTE,
	    LOW(x1),
	    HIGH(x1),
	    LOW(y1),
	    HIGH(y1),
	    LOW(x2),
	    HIGH(x2),
	    LOW(y2),
	    HIGH(y2),
		color
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  Change the size of lines being drawn
 *  \param size The new size for the lines
 */
void tlcd_changePenSize(uint8_t size) {
    unsigned char command[] = {
	    ESC_BYTE,
	    G_BYTE,
	    Z_BYTE,
	    size,
		size
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  Define the color at a given index. Not all bits are used, refer to datasheet.
 *  \param colorID The color index to change.
 *  \param The color to set.
 */
void tlcd_defineColor(uint8_t colorID, Color color) {
    unsigned char command[] = {
	    ESC_BYTE,
	    F_BYTE,
	    P_BYTE,
	    colorID,
	    color.red,
		color.green,
		color.blue
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  Change the color lines are drawn in
 *  \param color Index of the color lines should be drawn in
 */
void tlcd_changeDrawColor(uint8_t color) {
    unsigned char command[] = {
	    ESC_BYTE,
	    F_BYTE,
	    G_BYTE,
	    color,
	    255
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  Draw a character c at position (x1,y1).
 *  \param c Character to draw
 *  \param x1 X coordiate
 *  \param x1 Y coordinate
 */
void tlcd_drawChar(uint16_t x1, uint16_t y1, char c) {
    unsigned char command[] = {
	    ESC_BYTE,
	    Z_BYTE,
	    C_BYTE,
	    LOW(x1),
	    HIGH(x1),
	    LOW(y1),
	    HIGH(y1),
		c,
		0
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*! 
 *  Enable or disable the Cursor.
 *  \param enable Enable(1) or disable(0) the cursor.
 */
void tlcd_setCursor(uint8_t enabled) {
    unsigned char command[] = {
	    ESC_BYTE,
	    T_BYTE,
	    C_BYTE,
	    enabled
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}

/*!
 *  This function clears the display (fills hole display with background color).
 */
void tlcd_clearDisplay() {
    unsigned char command[] = {
	    ESC_BYTE,
	    D_BYTE,
	    L_BYTE
    };
    // Befehl an das TLCD senden
    tlcd_sendCommand(command, sizeof(command));
}
