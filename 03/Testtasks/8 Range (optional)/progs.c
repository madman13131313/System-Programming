//-------------------------------------------------
//          TestSuite: Range
//-------------------------------------------------

//  NOTE:
//  The heap cleanup must work for this test to run correctly!
//  The run will hang, if you have a bug in the process termination.

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_input.h"
#include "os_core.h"
#include "os_scheduler.h"
#include "os_memory.h"

#if VERSUCH < 3
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
#define DRIVER intHeap
#define DSIZE (os_getUseSize(DRIVER)/4)
//-------------------------------------------------

#ifndef WRITE
    #define WRITE(str) lcd_writeProgString(PSTR(str))
#endif
#define TEST_PASSED \
    do { ATOMIC { \
        lcd_clear(); \
        WRITE("  TEST PASSED   "); \
    } } while (0)
#define TEST_FAILED(reason) \
    do { ATOMIC { \
        lcd_clear(); \
        WRITE("FAIL  "); \
        WRITE(reason); \
    } } while (0)
#ifndef CONFIRM_REQUIRED
    #define CONFIRM_REQUIRED 1
#endif

uint16_t size = 1;
volatile uint16_t check_i = 0;
//! This variable is set to true when the test program finishes
bool terminated = false;

Program program2;
Program program3;

/*!
 * Display memory size and start test execution.
 */
REGISTER_AUTOSTART(program1)
void program1(void) {
    // Init Timeout Timer 1
    // CTC mode
    TCCR1A &= ~(1 << WGM10);
    TCCR1A &= ~(1 << WGM11);
    TCCR1B |= (1 << WGM12);
    TCCR1B &= ~(1 << WGM13);

    // set prescaler to 1024
    TCCR1B |=  (1 << CS12) | (1 << CS10);
    TCCR1B &= ~(1 << CS11);

    // set compare register -> match every 1s
    OCR1A = 19531;

    // enable timer
    TIMSK1 |= (1 << OCIE1A);

    os_exec(program2, DEFAULT_PRIORITY);
}


/*!
 * This program tests allocation of memory of arbitrary position and size within the heap.
 * To that end it allocates a quarter of the available memory
 * in chunks of increasing size (from 1 to DSIZE).
 */
void program2(void) {
    Program *cont = program2;
    lcd_clear();
    lcd_writeProgString(PSTR("[Testing range]"));
    if (size <= DSIZE) {
        lcd_line2();
        lcd_writeProgString(PSTR("Mass alloc: "));
        lcd_writeDec(size);
        // Allocate chunks
        for (uint16_t i = 0; i < DSIZE / size; i++) {
            if (!os_malloc(DRIVER, size)) {
                TEST_FAILED("Could not alloc (Phase 1)");
                HALT;
            }

            // Print progress
            lcd_drawBar((uint16_t)((100ul * i) / (DSIZE / size)));
            lcd_line2();
            lcd_writeProgString(PSTR("Mass alloc: "));
            lcd_writeDec(size);
            check_i = i;

        }
        // Increase the chunk size for next program run
        size++;
    } else {
        // All chunk sizes done. Switch to second phase (run prog 2).
        cont = program3;
    }
    lcd_line2();
    lcd_writeProgString(PSTR("OK, next..."));

    // Force termination and freeing of allocated memory by heap cleanup
    // before first execution of new program instance.
    os_enterCriticalSection();
    os_exec(cont, DEFAULT_PRIORITY);
}

/*!
 * This program asserts if a chunk of memory can be freed
 * by using a pointer to a arbitrary position within the chunk.
 */
void program3(void) {
    uint8_t accuracy = 10;
    lcd_clear();
    lcd_writeProgString(PSTR("[Testing range]"));

    // Iterate chunk sizes
    for (size = 1; size <= DSIZE; size++) {
        lcd_line2();
        lcd_writeProgString(PSTR("Random free: "));
        lcd_writeDec(size);

        // Try different addresses to free memory
        for (uint16_t i = 0; i < (size / accuracy); i++) {
            uint16_t startAddress = os_malloc(DRIVER, size);
            if (!startAddress) {
                TEST_FAILED("Could not alloc (Phase 2)");
                HALT;
            }
            // Use the timer counter value as "random" number
            os_free(DRIVER, startAddress + (TCNT0 % size));
            lcd_drawBar((100 * i) / (size / accuracy));
            lcd_line2();
            lcd_writeProgString(PSTR("Random free: "));
            lcd_writeDec(size);
        }
    }
    terminated = true;

    // SUCCESS
    #if CONFIRM_REQUIRED
    lcd_clear();
	lcd_writeProgString(PSTR("  PRESS ENTER!  "));
	os_waitForInput();
	os_waitForNoInput();
    #endif
    TEST_PASSED;
	lcd_line2();
	lcd_writeProgString(PSTR(" WAIT FOR IDLE  "));
    delayMs(DEFAULT_OUTPUT_DELAY * 6);
}

/*
 * Timeout ISR
 */
ISR(TIMER1_COMPA_vect) {
    // counts seconds
    static int16_t timeout_counter = 0;
    if (++timeout_counter >= 900 && !terminated){
        TEST_FAILED("Test timed out after 15 minutes!");
        HALT;
    }
}
