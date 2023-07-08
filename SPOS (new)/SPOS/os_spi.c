#include "os_spi.h"

void os_spi_init() {
	/*Configure PORT B since 23LC1024 should be connected to Pin B4-B7
	Pin B4: \CS (Chip Select: Should be Output; Auto configures the Controller as Slave if configured as Input and connected to GDN)
	Pin B5: MOSI (Master-Out-Slave-In; Should be Output)
	Pin B6: MISO (Master-In-Slave-Out; Should be Input)
	Pin B7: CLK (Clock; Should be Output since the Controller is condfigured as the Master)
	*/
	
	DDRB |= 0b10110000;
	DDRB &= 0b10111111;
	// Enable Pull-Up-resistance for the Input Pin
	PORTB |= 0b010000000;
	// Raise Pin 4 high, disabling the Slave by default
	PORTB |= 0b00010000;  
	
	/* Set SPCR SPI-Controll-Register
	Bit 7: SPIE (SPI Interrupt Enable)
	Bit 6: SPE  (Spi-enable)
	Bit 5: DORD (Dataorder: 0 := MSB first)
	Bit 4: MSTR (Master)
	Bit 3: CPOL (Polarity: 0 := Idle low, Active High)
	Bit 2: CPHA (Phasing:  0 := Leading Edge)
	
	Bit 1/0: SPR1/SPR0 (Clock-Rate-Select: Max. Frequency 23LC1024: 20MHz || CPU-Frequency: 20 MHZ ) 
	00 := Cpu-Frequency/4 = 5 MHz
	*/
	
	SPCR = 0b01010000;
	
	/* Set SPSR Spi-Status-Register
	Bit 7: SPIF (SPI-Interrupt-Flag)
	Bit 0: SPI2X (Double Speed; SPI2X := 1 and SPR1:= 0 and SPR0 := 0 :: Cpu-Frequency/2 = 10 MHz  
	*/
	
	SPSR &= 0b01111111;
	SPSR |= 0b00000001;
}



uint8_t os_spi_send(uint8_t data) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		SPSR &= 0b01111111;
		//Writing to the SPI-Data-Register should trigger the Transfer of one Byte.
		SPDR = data;
		//Active Waiting for the SPIF (SPI-Interrupt-Flag) to be set at the End of the Transfer
		while ((SPSR & 0b10000000) != 0b10000000 ) {}
	}
	return SPDR;
	
}

uint8_t os_spi_recive() {
	return os_spi_send(0xFF);
}