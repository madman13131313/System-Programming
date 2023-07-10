#include "os_spi.h"

// Configures relevant I/O registers/pins and initializes the SPI module.
void os_spi_init() {
	// Konfiguration der I/O-Pins für den SPI-Bus
	// B4: \CS
	DDRB |= 0b00010000; // Set Pin B4 as output
	// B5: MOSI
	DDRB |= 0b00100000; // Set Pin B5 as output
	// B6: MISO
	DDRB &= 0b10111111; // Set Pin B6 as input
	PORTB |= 0b01000000;
	// B7: CLK
	DDRB |= 0b10000000; // Set Pin B7 as output
	// SPI-Konfiguration
	SPCR |= (1 << SPE) | (1 << MSTR); // Aktivieren des SPI-Moduls, Master-Modus
	// Setzen der SPI-Taktfrequenz (Prescaler) fCPU/2
	SPCR &= 0b11111101; // Setze SPR1 auf 0
	SPCR &= 0b11111110; // Setze SPR0 auf 0
	SPSR |= 0b00000001; // Setze SPI2X auf 1
}

// Performs a SPI send This method blocks until the data exchange is completed.
// Additionally, this method returns the byte which is received in exchange during the communication.
uint8_t os_spi_send(uint8_t data) {
	
	// Setze das zu sendende Datenbyte
	SPDR = data;

	// Warte erneut, bis die Übertragung abgeschlossen ist
	while (!(SPSR & (1 << SPIF))) {}

	uint8_t receivedByte = SPDR;
	return receivedByte;
}

// Performs a SPI read This method blocks until the exchange is completed.
uint8_t os_spi_receive(void) {
	
	uint8_t receivedByte = os_spi_send(0xFF); // Sende ein Dummy-Byte, um Daten zu empfangen

	return receivedByte;
}