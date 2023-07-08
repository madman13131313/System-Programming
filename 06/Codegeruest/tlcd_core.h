/*! \file
 *  \brief Core library for the touchpanel EA EDIPTFT43-ATP
 */

#ifndef _TLCD_CORE_H
#define _TLCD_CORE_H

#include <stdint.h>

#include "os_memory.h"

//! Physical size of the display
#define TLCD_WIDTH 480
#define TLCD_HEIGHT 272

// Hardware mappings
#define TLCD_PORT PORTB
#define TLCD_DDR DDRB
#define TLCD_PIN PINB

#define TLCD_SPI_CS_BIT PB4
#define TLCD_SPI_MOSI_BIT PB5
#define TLCD_SPI_MISO_BIT PB6
#define TLCD_SPI_CLK_BIT PB7

#define TLCD_SEND_BUFFER_IND_INT_MSK_PIN PCINT10
#define TLCD_SEND_BUFFER_IND_INT_MSK_PORT PCIE1
#define TLCD_SEND_BUFFER_IND_BIT PB2

#define TLCD_RESET_BIT PB3

#define ESC_BYTE 0x1B
#define A_BYTE 0x41
#define C_BYTE 0x43
#define D_BYTE 0x44
#define E_BYTE 0x45
#define F_BYTE 0x46
#define G_BYTE 0x47
#define H_BYTE 0x48
#define L_BYTE 0x4C
#define P_BYTE 0x50
#define R_BYTE 0x52
#define S_BYTE 0x53
#define T_BYTE 0x54
#define Z_BYTE 0x5A
#define ACK 0x06

//! Set the chip select of the display to high
#define tlcd_spi_disable() TLCD_PORT |= (1<<TLCD_SPI_CS_BIT);

//! Set the chip select of the display to low
#define tlcd_spi_enable() TLCD_PORT &= ~(1<<TLCD_SPI_CS_BIT);

//! Extracts the low byte from a 2 byte word
#define LOW(x) (x&0xFF)

//! Extracts the high byte from a 2 byte word
#define HIGH(x) ((x&0xFF00)>>8)

#define INPUTBUFFER_SIZE 256

//! Organizes bytes during the communication process
typedef struct {
    MemAddr data;
    uint16_t head;
    uint16_t tail;
    uint16_t size;
} tlcdBuffer;

//! Initializes the display
void tlcd_init();

//! Resets the display
void tlcd_reset();

//! Tries to read a byte from the display
uint8_t tlcd_readByte(void);

//! Initializes the command buffer
void tlcd_initBuffer();

//! Resets a given buffer
void tlcd_resetBuffer();

//! Reads the next buffer element from a given buffer
uint8_t tlcd_readNextBufferElement();

//! Writes the given byte into the specified buffer
void tlcd_writeNextBufferElement(uint8_t dataByte);

//! Checks if there are unprocessed data
uint8_t tlcd_hasNextBufferElement();

//! Reads the complete contenet of the TLCD sending buffer into the local input buffer.
void tlcd_readData();

//! Buffers a given command within the command buffer
void tlcd_sendCommand(const unsigned char* cmd, uint8_t len);

//! Requests the sending buffer from the display
void tlcd_requestData();

#endif