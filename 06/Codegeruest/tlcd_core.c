#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "tlcd_core.h"
#include "tlcd_parser.h"
#include "util.h"

#warning IMPLEMENT STH. HERE

/*!
 *  This function configures all relevant ports,
 *  initializes the pin change interrupt, the spi
 *  module and the input buffer
 */
void tlcd_init() {
    #warning IMPLEMENT STH. HERE

    // Pin change interrupt on PORTB
    PCICR = (1 << TLCD_SEND_BUFFER_IND_INT_MSK_PORT);
    PCMSK1 = (1 << TLCD_SEND_BUFFER_IND_INT_MSK_PIN);

    // Initial reset
    tlcd_reset();

    // Initialize all buffers
    tlcd_initBuffer();
}

/*!
 *  This function resets the lcd by toggling the reset pin.
 */
void tlcd_reset() {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function writes a byte to the lcd via SPI
 *  \param byte The byte to be send to the lcd
 */
uint8_t tlcd_writeByte(uint8_t byte) {
    #warning IMPLEMENT STH. HERE
    return 0;
}

/*!
 *  This function waits for a byte to arrive from the lcd and returns it
 *  \returns a byte read from the lcd
 */
uint8_t tlcd_readByte() {
    #warning IMPLEMENT STH. HERE
    return 0;
}

/*!
 *  This function initializes input buffer
 *  i.e. allocate memory on the internal
 *  heap and initialize head & tail
 */
void tlcd_initBuffer() {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function resets the input buffer by setting head & tail to zero
 */
void tlcd_resetBuffer() {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  Writes a given byte into the input buffer and increments the position of head
 *  \param dataByte The byte that is to be stored into the buffer
 */
void tlcd_writeNextBufferElement(uint8_t dataByte) {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function reads a byte from the input buffer and increments the position of tail
 */
uint8_t tlcd_readNextBufferElement() {
    #warning IMPLEMENT STH. HERE
    return 0;
}

/*!
 *  Checks if there are unprocessed data
 */
uint8_t tlcd_hasNextBufferElement() {
    #warning IMPLEMENT STH. HERE
    return 0;
}

/*!
 *  This function requests the sending buffer
 *  from the tlcd. Should only be called, if SBUF is low.
 */
void tlcd_requestData() {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function reads the complete content of the TLCD sending buffer into the
 *  local input buffer. After a complete frame has been received, the bcc is checked.
 *  In case of a checksum error, the package is ignored by resetting the input buffer
 */
void tlcd_readData() {
    #warning IMPLEMENT STH. HERE
}

/*!
 *  This function sends a command with a given length
 *  \param cmd The cmd to be buffered as a string
 *  \param len The length of the command
 */
void tlcd_sendCommand(const unsigned char* cmd, uint8_t len) {
    #warning IMPLEMENT STH. HERE
}