#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "tlcd_core.h"
#include "tlcd_parser.h"

#include <stdlib.h>
#include "os_core.h"
#include "util.h"

tlcdBuffer inputBuffer;

/*!
 *  This function configures all relevant ports,
 *  initializes the pin change interrupt, the spi
 *  module and the input buffer
 */
void tlcd_init() {
    // Konfiguration der I/O-Pins für den SPI-Bus
	// B2: Sendbuffer Indicator
	DDRB &= 0b11111011; // Set Pin B2 as input
	PORTB |= 0b00000100;
	// B3: Reset
	DDRB |= 0b00001000; // Set Pin B3 as output
    // B4: \CS
    DDRB |= 0b00010000; // Set Pin B4 as output
    // B5: MOSI
    DDRB |= 0b00100000; // Set Pin B5 as output
    // B6: MISO
    DDRB &= 0b10111111; // Set Pin B6 as input
	PORTB |= 0b01000000;
    // B7: CLK
    DDRB |= 0b10000000; // Set Pin B7 as output
	tlcd_spi_disable();
    // SPI-Konfiguration
    SPCR = 0x7F;	// (1<<SPIE) only SPIE enabled
    // Setzen der SPI-Taktfrequenz (Prescaler) fOSC/128
    SPCR |= 0b00000010; // Setze SPR1 auf 1
    SPCR |= 0b00000001; // Setze SPR0 auf 1

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
    PORTB &= ~(1 << TLCD_RESET_BIT);
    _delay_ms(10);
    PORTB |= (1 << TLCD_RESET_BIT);
}


/*!
 *  This function writes a byte to the lcd via SPI
 *  \param byte The byte to be send to the lcd
 */
uint8_t tlcd_writeByte(uint8_t byte) {
	uint8_t receivedByte;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		tlcd_spi_enable();
		_delay_us(6);

		SPDR = byte;  // Write byte to SPDR

		// Wait for transmission to complete
		while (!(SPSR & (1 << SPIF))) {}
		receivedByte = SPDR;
	
		tlcd_spi_disable();
	}
	return receivedByte;
	
}

/*!
 *  This function waits for a byte to arrive from the lcd and returns it
 *  \returns a byte read from the lcd
 */
uint8_t tlcd_readByte() {
   uint8_t receivedByte = tlcd_writeByte(0xFF); // Sende ein Dummy-Byte, um Daten zu empfangen

   return receivedByte;
}

/*!
 * This function initializes input buffer
 * i.e. allocate memory on the internal
 * heap and initialize head & tail 
 */
void tlcd_initBuffer() {
	inputBuffer.data = os_malloc(intHeap, INPUTBUFFER_SIZE);	
	if(inputBuffer.data == 0) {
		os_error("buffer initialize error");
		return;
	}
	inputBuffer.head = 0;
	inputBuffer.tail = 0;
	inputBuffer.size = INPUTBUFFER_SIZE;
}

/*!
 * This function resets the input buffer by setting head & tail to zero
 */
void tlcd_resetBuffer() {
	inputBuffer.head = 0;
	inputBuffer.tail = 0;
}

/*!
 * Writes a given byte into the input buffer and increments the position of head
 * \param dataByte The byte that is to be stored into the buffer
 */
void tlcd_writeNextBufferElement(uint8_t dataByte) {
	MemAddr tempHead = inputBuffer.head;
	if(tempHead == inputBuffer.size - 1) {
		tempHead = 0;
	} else {
		++tempHead;
	}
	if(tempHead == inputBuffer.tail) {
		os_error("buffer full");
		return;
	} else {
		inputBuffer.head = tempHead;
	}
	intHeap->driver->write(inputBuffer.data + inputBuffer.head, dataByte);
}

/*!
 * This function reads a byte from the input buffer and increments the position of tail
 */
uint8_t tlcd_readNextBufferElement() {
	if(inputBuffer.tail == inputBuffer.head) {
		os_error("buffer empty");
		return 0;
	}
	MemAddr tempTail = inputBuffer.tail;
	if(tempTail == inputBuffer.size - 1) {
		tempTail = 0;
	} else {
		++tempTail;
	}
	inputBuffer.tail = tempTail;
	return intHeap->driver->read(inputBuffer.data + inputBuffer.tail);
}

/*!
 * Checks if there are unprocessed data
 */
uint8_t tlcd_hasNextBufferElement() {
	if (inputBuffer.tail == inputBuffer.head)
	{
		return 0;
	}
	return 1;
}

/*!
 *  This function requests the sending buffer
 *  from the tlcd. Should only be called, if SBUF is low.
 */
void tlcd_requestData() {
	bool acknowledged = false;
	uint8_t bcc = 0x12 + 0x01 + S_BYTE;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		while (!acknowledged)
		{
			tlcd_writeByte(DC2_BYTE);
			tlcd_writeByte(0x01);
			tlcd_writeByte(S_BYTE);
			tlcd_writeByte(bcc);
			uint8_t response = tlcd_readByte();
			if (response == ACK)
			{
				acknowledged = true;
			}
		}
	}
}


/*!
 *  This function reads the complete content of the TLCD sending buffer into the
 *  local input buffer. After a complete frame has been received, the bcc is checked.
 *  In case of a checksum error, the package is ignored by resetting the input buffer
 */
void tlcd_readData() {
    uint8_t frameStart = tlcd_readByte();
	if (frameStart != DC1_BYTE)
	{
		tlcd_resetBuffer();
		return;
	}
	uint8_t frameLen = tlcd_readByte();
	if (frameLen == 0 || frameLen > INPUTBUFFER_SIZE)
	{
		tlcd_resetBuffer();
		return;
	}
	uint8_t data[frameLen];
	uint8_t receivedChecksum = frameStart + frameLen;
	uint8_t calculatedChecksum = 0;
	for (uint8_t i = 0; i < frameLen; i++) {
		data[i] = tlcd_readByte();
		receivedChecksum += data[i];
	}
	calculatedChecksum = tlcd_readByte();
	if (receivedChecksum != calculatedChecksum)
	{
		tlcd_resetBuffer();
		return;
	}
	for (uint8_t i = 0; i < frameLen; i++) {
		tlcd_writeNextBufferElement(data[i]);
	}
}

/*!
 * This function sends a command with a given length
 * \param cmd The cmd to be buffered as a string
 * \param len The length of the command
 *
 */

void tlcd_sendCommand(const unsigned char* cmd, uint8_t len) {
	if(len == 0 ) {
		os_error("send command length 0");
		return;
	}
	uint8_t tlcd_returnValue = 0x15;
	uint16_t bcc;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		while(tlcd_returnValue != ACK) {
			bcc = 0;
			tlcd_writeByte(DC1_BYTE);
			bcc += DC1_BYTE;
			tlcd_writeByte(len);
			bcc += len;
			for(uint8_t i = 0; i < len; ++i) {
				tlcd_writeByte(cmd[i]);
				bcc += cmd[i];
			}
			tlcd_writeByte(bcc);
			tlcd_returnValue = tlcd_readByte();
		}
	}
}