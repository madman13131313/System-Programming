#ifndef TLCD_BUTTON_H
#define TLCD_BUTTON_H

#include <stdint.h>

#include "tlcd_parser.h"

#define MAX_BUTTONS 10

void tlcd_addButtonWithChar(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t color, uint8_t downCode, char c);

void tlcd_drawButtons();

uint8_t tlcd_handleButtons(TouchEvent event);

#endif