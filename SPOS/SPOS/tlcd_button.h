#ifndef TLCD_BUTTON_H
#define TLCD_BUTTON_H

#include <stdint.h>

#include "tlcd_parser.h"

#define MAX_BUTTONS 10

// Definition des Button-Typs
typedef struct {
	uint16_t x1;        // Position x1
	uint16_t y1;        // Position y1
	uint16_t x2;        // Position x2
	uint16_t y2;        // Position y2
	uint8_t color;      // Farbindex
	uint8_t downCode;   // Button-Code beim Drücken
	char c;             // Aufschrift
} Button;

void tlcd_addButtonWithChar(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, uint8_t downCode, char c);

void tlcd_drawButtons();

uint8_t tlcd_handleButtons(TouchEvent event);
typedef void ButtonCallback(uint8_t code, uint16_t x, uint16_t y);
void tlcd_setButtonCallback(ButtonCallback* callback);

#endif