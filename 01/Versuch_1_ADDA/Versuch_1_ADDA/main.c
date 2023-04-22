#include <avr/io.h>
#include <util/delay.h>
#include "os_input.h"

void manuell(void);
void sar(void);
void tracking(void);

int main(void) {
	os_initInput();
    uint8_t choice = 1;
    while (1) {
        switch(choice){
			case 1:
				manuell();
				break;
			case 2:
				sar();
				break;
			case 3:
				tracking();
				break;
			default:
				//Error message
				break;
		}
    }
	return 0;
}

void manuell(void){
	DDRD = 0b00000000; //Port D as input
	PORTD |= 0b11111111; // Pullup-Widerstand an Pin D aktivieren
	DDRA = 0b11111111; //Port A and B as output
	DDRB = 0b11111111;
	
	while(1){
		uint8_t dip = PIND;
		PORTA = ~dip; //dip gives active low signal
		PORTB = ~dip;
	}
}

void sar(void){
	while(1){
		os_waitForInput(); // Button down
		os_waitForNoInput(); // Button up
		DDRA = 0b11111111; // Port A and B as output
		DDRB = 0b11111111;
		DDRC &= 0b11111110; // Pin C0 as input
		PORTC |= 0b00000001;
		uint8_t i;
		uint8_t dip = 0b00000000;
		uint8_t x = 0b10000000;
		PORTA = 0b11111111;
		for (i = 0; i < 8; ++i)
		{
			PORTB = dip + x; // Zur Erzeugung der Referenzspannung
			_delay_ms(50); // Wartezeit
			if ((PINC & 0b00000001) == 0) // Komparator Verhalten
			{
				dip += x; // die Eins beibehalten
			}
			PORTA = ~dip; // die aktuelle Referenzspannung auf LED ausgeben
			x = (x >> 1); // mit dem nächstniedrigerwertigem Bit verfahren
		}
	}
}

void tracking(void){
	while(1){
		os_waitForInput(); // Button down
		os_waitForNoInput(); // Button up
		DDRA = 0b11111111; // Port A and B as output
		DDRB = 0b11111111;
		DDRC &= 0b11111110; // Pin C0 as input
		PORTC |= 0b00000001;
		uint8_t dip = 0b00000001;
		uint8_t ubc = 0b00000001; // Up-Down-Counter
		uint8_t flag = 0b00000000; // sicherzustellen ,ob die Referenzspannung die Messspannung über- oder unterschritten hat
		PORTA = 0b11111111;
		while(flag == 0)
		{
			PORTB = dip ; // Zur Erzeugung der Referenzspannung
			_delay_ms(50); // Wartezeit
			if ((PINC & 0b00000001) == 0) // Komparator Verhalten
			{
				dip += ubc; // inkrementieren
				}else{
				dip -= ubc; // dekrementieren
				flag += 0b00000001; // die Wandlung stoppen
			}
			PORTA = ~dip; // die aktuelle Referenzspannung auf LED ausgeben
		}
	}
}
