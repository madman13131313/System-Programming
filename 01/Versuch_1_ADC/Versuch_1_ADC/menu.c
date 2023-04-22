#include "menu.h"
#include "os_input.h"
#include "bin_clock.h"
#include "lcd.h"
#include "led.h"
#include "adc.h"
#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

/*!
 *  Hello world program.
 *  Shows the string 'Hello World!' on the display.
 */
void helloWorld(void) {
    // Repeat until ESC gets pressed
    while(1){
		if (os_getInput() == 0b00001000)
		{
			os_waitForNoInput();
			showMenu();
			break;
		}
		lcd_writeString("Hello World!");
		_delay_ms(500);
		lcd_clear();
		_delay_ms(500);
	}
}

/*!
 *  Shows the clock on the display and a binary clock on the led bar.
 */
void displayClock(void) {
	lcd_clear();
	uint16_t clockVal;
	while (1){
		clockVal = ((getTimeHours() & 0xFF) << 12) |
		((getTimeMinutes() & 0xFF) << 6) |
		((getTimeSeconds() & 0xFF));
		setLedBar(clockVal);
		lcd_erase(1);
		lcd_line1();
		if (getTimeHours() < 10)
		{
			lcd_writeChar('0');
			lcd_writeDec(getTimeHours());
		}else
		{
			lcd_writeDec(getTimeHours());
		}
		lcd_writeChar(':');
		if (getTimeMinutes() < 10)
		{
			lcd_writeChar('0');
			lcd_writeDec(getTimeMinutes());
		}else
		{
			lcd_writeDec(getTimeMinutes());
		}
		lcd_writeChar(':');
		if (getTimeSeconds() < 10)
		{
			lcd_writeChar('0');
			lcd_writeDec(getTimeSeconds());
		}else
		{
			lcd_writeDec(getTimeSeconds());
		}
		lcd_writeChar(':');
		if (getTimeMilliseconds()  < 100)
		{
			lcd_writeChar('0');
			lcd_writeDec(getTimeMilliseconds());
		}
		else
		{
			lcd_writeDec(getTimeMilliseconds());
		}
		_delay_ms(100);
		if (os_getInput() == 0b00001000)
		{
			os_waitForNoInput();
			showMenu();
			break;
		}
	}
    
			   
}

/*!
 *  Shows the stored voltage values in the second line of the display.
 */
void displayVoltageBuffer(uint8_t displayIndex) {
	if (displayIndex < 9)
	{
		lcd_writeString("0");
	}
    lcd_writeString("0");
	lcd_writeDec(displayIndex + 1);
	lcd_writeString("/100: ");
	lcd_writeVoltage(getStoredVoltage(displayIndex), 1023, 5);
}

/*!
 *  Shows the ADC value on the display and on the led bar.
 */
void displayAdc(void) {
	uint8_t displayIndex = 0; // Index des anzuzeigenden Spannungswertes
    while (1){
		bool saveData = false; // define save data flag
		uint16_t ledValue = 0;
		uint16_t adcResult = getAdcValue();
		if ((os_getInput() == 0b00000001) && (!saveData)) // Messwerte sollen abgespeichert werden, wenn der Enter-Button gedrückt wird.
		{
			saveData = true;
		}
		lcd_clear();
		lcd_writeString("Voltage: ");
		lcd_writeVoltage(adcResult, 1023, 5);
		lcd_line2();
		displayVoltageBuffer(displayIndex);
		if (saveData)
		{
			storeVoltage();
		}
		
		if (os_getInput() == 0b00000100) // check if Up button is pressed
		{
			os_waitForNoInput();
			if (displayIndex > 0)
			{	
				displayIndex--;
			}else
			{
				displayIndex = getBufferIndex() - 1;
			}
			lcd_erase(2);
			lcd_line2();
			displayVoltageBuffer(displayIndex);
		}
		
		if (os_getInput() == 0b00000010) // check if Down button is pressed
		{
			os_waitForNoInput();
			if (displayIndex < getBufferIndex() - 1)
			{
				displayIndex++;
			}else
			{
				displayIndex = 0;
			}
			lcd_erase(2);
			lcd_line2();
			displayVoltageBuffer(displayIndex);
		}
		
		// set Led Bar
		while (adcResult >= 68)
		{
			ledValue = ledValue << 1; // um Eins nach links shiften
			ledValue += 2; // Addition von Zwei
			adcResult -= 68;
		}
		setLedBar(ledValue);
		
		if (os_getInput() == 0b00001000) // check if ESC is pressed
		{
			os_waitForNoInput();
			break;
		}
		
		_delay_ms(100);
    }
}

/*! \brief Starts the passed program
 *
 * \param programIndex Index of the program to start.
 */
void start(uint8_t programIndex) {
    // Initialize and start the passed 'program'
    switch (programIndex) {
        case 0:
            lcd_clear();
            helloWorld();
            break;
        case 1:
            activateLedMask = 0xFFFF; // Use all LEDs
            initLedBar();
            initClock();
            displayClock();
            break;
        case 2:
            activateLedMask = 0xFFFE; // Don't use LED 0
            initLedBar();
            initAdc();
            displayAdc();
            break;
        default:
            break;
    }

    // Do not resume to the menu until all buttons are released
    os_waitForNoInput();
}

/*!
 *  Shows a user menu on the display which allows to start subprograms.
 */
void showMenu(void) {
    uint8_t pageIndex = 0;

    while (1) {
        lcd_clear();
        lcd_writeProgString(PSTR("Select:"));
        lcd_line2();

        switch (pageIndex) {
            case 0:
                lcd_writeProgString(PSTR("1: Hello world"));
                break;
            case 1:
                lcd_writeProgString(PSTR("2: Binary clock"));
                break;
            case 2:
                lcd_writeProgString(PSTR("3: Internal ADC"));
                break;
            default:
                lcd_writeProgString(PSTR("----------------"));
                break;
        }

        os_waitForInput();
        if (os_getInput() == 0b00000001) { // Enter
            os_waitForNoInput();
            start(pageIndex);
        } else if (os_getInput() == 0b00000100) { // Up
            os_waitForNoInput();
            pageIndex = (pageIndex + 1) % 3;
        } else if (os_getInput() == 0b00000010) { // Down
            os_waitForNoInput();
            if (pageIndex == 0) {
                pageIndex = 2;
            } else {
                pageIndex--;
            }
        }
    }
}
