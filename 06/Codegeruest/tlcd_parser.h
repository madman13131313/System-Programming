/*! \file
 *  \brief Interaction library for the touchpanel EA eDIPTFT43-ATP
 *
 *  This module implements the response of the communication with the EA eDIPTFT43-ATP. Some commands send to
 *  the display force it to send a response (directly or when an event occurs, e.g., pressing a button on the display).
 *  This response is evaluated within this module. To react to this kind of responses, handlers can be registered, which will be called
 *  when a certain message is captured.
 */

#ifndef _TLCD_PARSER_H
#define _TLCD_PARSER_H

#include <stdint.h>

//! Protocol specific constants
#define DC1_BYTE 0x11
#define DC2_BYTE 0x12
#define ESC_BYTE 0x1B

//! Identifier for a TOUCHPANEL_EVENT
#define TOUCHPANEL_EVENT 0x48

//! Identifier for specific buttons
#define PENSIZE_INCREASE 33
#define PENSIZE_DECREASE 34
#define ERASER 35

//! Enum giving information if the event announces pressing or letting go of the touch panel
typedef enum {
    TOUCHPANEL_UP = 0,
    TOUCHPANEL_DOWN = 1,
    TOUCHPANEL_DRAG = 2,
} TouchEventType;

//! Struct holding the information that are transmitted when a TOUCHPANEL_EVENT occurs
typedef struct {
    TouchEventType type;
    uint16_t x;
    uint16_t y;
} TouchEvent;

#endif