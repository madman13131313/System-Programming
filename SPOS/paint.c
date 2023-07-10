#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <string.h>
#include "tlcd_core.h"
#include "lcd.h"
#include "util.h"
#include "os_input.h"
#include "os_core.h"
#include "tlcd_graphic.h"
#include "tlcd_parser.h"
#include "tlcd_button.h"
#include "defines.h"

#define BACKGROUND_COLOR 16
void initializePaintApp(void);
void handleButtonPress(uint8_t code, uint16_t x, uint16_t y);
void handleTouchEvent(TouchEvent event);
void defineColors(void);
void drawColorGradient();
uint8_t getColorFromGradientPosition(uint16_t x);


uint8_t penSize = 5;
uint8_t penColor = 14;
uint8_t isEraserMode = 0;
TouchEvent prevEvent;

void initializePaintApp() {
	// Initialize TLCD
	tlcd_init();
	
	// Define touch area
	tlcd_defineTouchArea(0, 0, TLCD_WIDTH, TLCD_HEIGHT);
	
	// Clear TLCD
	tlcd_clearDisplay();
	
	defineColors();
	
	// Create buttons
	tlcd_addButtonWithChar(420, 0, 470, 40, 15, PENSIZE_INCREASE, '+');
	tlcd_addButtonWithChar(420, 50, 470, 90, 15, PENSIZE_DECREASE, '-');
	tlcd_addButtonWithChar(420, 100, 470, 140, 15, ERASER, 'E');
	tlcd_addButtonWithChar(0, 220, 470, 270, 16, CHANGE_COLOR, ' ');
	
	// Set button callbacks
	tlcd_setButtonCallback(handleButtonPress);
	tlcd_setEventCallback(handleTouchEvent);
	
	// Draw buttons
	tlcd_drawButtons();
	drawColorGradient();
	tlcd_changePenSize(penSize);
	tlcd_changeDrawColor(penColor);
}

void handleButtonPress(uint8_t code, uint16_t x, uint16_t y) {
	if (code == PENSIZE_INCREASE) {
		// Increase pen size
		penSize += 1;
		if (penSize > 15) {
			penSize = 15;
		}
		tlcd_changePenSize(penSize);
	} else if (code == PENSIZE_DECREASE) {
		// Decrease pen size
		penSize -= 1;
		if (penSize < 1) {
			penSize = 1;
		}
		tlcd_changePenSize(penSize);
	} else if (code == ERASER) {
		if (!isEraserMode)
		{
			isEraserMode = 1;
			tlcd_changeDrawColor(BACKGROUND_COLOR);
			tlcd_changePenSize(15);
		}
		else
		{
			isEraserMode = 0;
			tlcd_changeDrawColor(penColor);
			tlcd_changePenSize(penSize);
		}
	} else if (code == CHANGE_COLOR) {
		// Get color from gradient position
		uint8_t colorID = getColorFromGradientPosition(x);
		penColor = colorID;
		tlcd_changeDrawColor(colorID);
	}
}

void handleTouchEvent(TouchEvent event) {
	if (event.x > 410 || event.y > 210)
	{
		return;
	}
	if (event.type == TOUCHPANEL_DRAG){
		if (prevEvent.type == TOUCHPANEL_DRAG || prevEvent.type == TOUCHPANEL_DOWN)
		{
			tlcd_drawLine(prevEvent.x, prevEvent.y, event.x, event.y);
		}
		prevEvent = event;
	}
	else
	{
		prevEvent = event;
		if (event.type == TOUCHPANEL_DOWN) {
			uint16_t x = event.x;
			uint16_t y = event.y;
			tlcd_drawPoint(x, y);
		}
	}
}

void defineColors() {
	unsigned char white[] = {0x1B, 0x46, 0x50, 14, 255, 255, 255};
	tlcd_sendCommand(white, 7);
	
	unsigned char grey[] = {0x1B, 0x46, 0x50, 15, 175, 175, 175};
	tlcd_sendCommand(grey, 7);
	
	unsigned char black[] = {0x1B, 0x46, 0x50, 16, 0, 0, 0};
	tlcd_sendCommand(black, 7);
}

void drawColorGradient() {
	uint16_t startY = 220;
	uint16_t endY = 270;

	for (uint16_t i = 0; i < 470; i++) {
		if (i < 78)
		{
			uint8_t red = 255;
			uint8_t green = i * (255/78);
			uint8_t blue = 0;
			unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
			tlcd_sendCommand(color, 7);
		}
		if (i < 156 && i >= 78)
		{
			uint8_t red = (156- i) * (255/78);
			uint8_t green = 255;
			uint8_t blue = 0;
			unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
			tlcd_sendCommand(color, 7);
		}
		if (i < 234 && i >= 156)
		{
			uint8_t red = 0;
			uint8_t green = 255;
			uint8_t blue = (i - 156) * (255/78);
			unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
			tlcd_sendCommand(color, 7);
		}
		if (i < 312 && i >= 234)
		{
			uint8_t red = 0;
			uint8_t green = (312- i) * (255/78);
			uint8_t blue = 255;
			unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
			tlcd_sendCommand(color, 7);
		}
		if (i < 390 && i >= 312)
		{
			uint8_t red = (i - 312) * (255/78);
			uint8_t green = 0;
			uint8_t blue = 255;
			unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
			tlcd_sendCommand(color, 7);
		}
		if (i < 470 && i >= 390)
		{
			uint8_t red = 255;
			uint8_t green = 0;
			uint8_t blue = (470- i) * (255/78);
			unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
			tlcd_sendCommand(color, 7);
		}
		tlcd_changeDrawColor(1);
		tlcd_drawLine(i, startY, i, endY);
	}
}

uint8_t getColorFromGradientPosition(uint16_t x) {
	// Calculate color ID based on the x position on the color gradient
	// Divide the x position by the width of the TLCD and multiply by the total number of available color IDs
	if (x < 78)
	{
		uint8_t red = 255;
		uint8_t green = x * (255/78);
		uint8_t blue = 0;
		unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
		tlcd_sendCommand(color, 7);
	}
	if (x < 156 && x >= 78)
	{
		uint8_t red = (156- x) * (255/78);
		uint8_t green = 255;
		uint8_t blue = 0;
		unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
		tlcd_sendCommand(color, 7);
	}
	if (x < 234 && x >= 156)
	{
		uint8_t red = 0;
		uint8_t green = 255;
		uint8_t blue = (x - 156) * (255/78);
		unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
		tlcd_sendCommand(color, 7);
	}
	if (x < 312 && x >= 234)
	{
		uint8_t red = 0;
		uint8_t green = (312- x) * (255/78);
		uint8_t blue = 255;
		unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
		tlcd_sendCommand(color, 7);
	}
	if (x < 390 && x >= 312)
	{
		uint8_t red = (x - 312) * (255/78);
		uint8_t green = 0;
		uint8_t blue = 255;
		unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
		tlcd_sendCommand(color, 7);
	}
	if (x < 470 && x >= 390)
	{
		uint8_t red = 255;
		uint8_t green = 0;
		uint8_t blue = (470- x) * (255/78);
		unsigned char color[] = {0x1B, 0x46, 0x50, 1, red, green, blue};
		tlcd_sendCommand(color, 7);
	}
	
	return 1;
}

REGISTER_AUTOSTART(program1)
void program1(void) {
	// Initialize paint app
	initializePaintApp();
	
	while (1) {
		
		if (!(PORTB & 0b00001000))
		{
			tlcd_clearDisplay();
		}
	}
}

