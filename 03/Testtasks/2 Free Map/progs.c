//-------------------------------------------------
//          TestTask: Free Map
//-------------------------------------------------

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_scheduler.h"
#include "os_memory.h"
#include "os_memheap_drivers.h"
#include "os_input.h"

#if VERSUCH < 3
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
//! The heap-driver we expect to find at position 0 of the heap-list
#define DRIVER intHeap
//! Number of bytes we allocate by hand. Must be even and >2.
#define SIZE 10
//! The minimal size of the map we accept
#define MAPSIZE 100
//! Defines if the first phase is executed
#define PHASE_1 1
//! Defines if the second phase is executed
#define PHASE_2 1
//! Defines if the third phase is executed
#define PHASE_3 1
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

//! Prints phase information to the LCD.
void tt_phase(uint8_t i, char const* name) {
    lcd_clear();
    lcd_writeProgString(PSTR("Phase "));
    lcd_writeDec(i);
    lcd_writeChar(':');
    lcd_line2();
    lcd_writeProgString(name);
    delayMs(DEFAULT_OUTPUT_DELAY * 8);
}
//! Prints OK for the phase message.
void tt_phaseOK(void) {
    lcd_goto(2, 15);
    lcd_writeProgString(PSTR("OK"));
    delayMs(DEFAULT_OUTPUT_DELAY * 8);
    lcd_clear();
}

//! Checks if all bytes in a certain memory-area are the same.
uint8_t tt_isAreaUniform(MemAddr start, uint16_t size, MemValue byte);


REGISTER_AUTOSTART(program1)
void program1(void) {
    // If there are any problems, we set a certain bit in the bitmask
    // "errorCode", indicating what exactly went wrong.
    uint8_t errorCode = 0;

// Phase 1: Some sanity checks
#if PHASE_1
    tt_phase(1, PSTR("Sanity check"));


    // Check if the driver returned by lookupHeap is the correct one.
    // If not, set bit 0
    errorCode |= (os_lookupHeap(0) != DRIVER) << 0;
    // Check if the mapsize of the heap is correct.
    // If not, set bit 1
    errorCode |= (os_getMapSize(DRIVER) < MAPSIZE) << 1;
    // Check if the length of the heaplist is correct.
    // If not, set bit 2
    errorCode |= (os_getHeapListLength() != (VERSUCH <= 3 ? 1 : 2)) << 2;
    // Check if use- and map-area have a size relationship of 2:1.
    // If not, set bit 3
    errorCode |= (os_getMapSize(DRIVER) != os_getUseSize(DRIVER) - os_getMapSize(DRIVER)) << 3;
    // Check if the ends before the stack area
    errorCode |= (os_getUseStart(DRIVER) + os_getUseSize(DRIVER) - 1 > PROCESS_STACK_BOTTOM(MAX_NUMBER_OF_PROCESSES)) << 4;

    // Output of test-results, if there was an error
    if (errorCode) {
        TEST_FAILED("");
        delayMs(DEFAULT_OUTPUT_DELAY * 8);
        lcd_clear();
        // The additional | at the end is intentional, it gets wrapped into
        // the next line as the left table column separator
        lcd_writeProgString(PSTR("|DR|MS|LS|12|ST||"));
        for (uint8_t errorIndex = 0; errorIndex < 5; errorIndex++) {
            if (errorCode & (1 << errorIndex)) {
                lcd_writeProgString(PSTR("XX|"));
            } else {
                lcd_writeProgString(PSTR("OK|"));
            }
        }
        HALT;
    }

    tt_phaseOK();
#endif

// Phase 2: Malloc
#if PHASE_2
    tt_phase(2, PSTR("malloc"));

    // We use error-codes again
    errorCode = 0;

    // We allocate the whole use-area with os_malloc
    MemAddr hugeChunk = os_malloc(DRIVER, os_getUseSize(DRIVER));
    if (hugeChunk == 0) {
        // We could not allocate the whole use-area
        errorCode |= 1 << 0;
    } else {
        // Allocation was successful, but what does the map look like?
        uint8_t mapEntry = (os_getCurrentProc() << 4) | 0x0F;
        if (DRIVER->driver->read(os_getMapStart(DRIVER)) != mapEntry) {
            // The first map-entry is wrong
            errorCode |= 1 << 1;
        }
        if (!tt_isAreaUniform(os_getMapStart(DRIVER) + 1, os_getMapSize(DRIVER) - 1, 0xFF)) {
            // The rest of the map is not filled with 0xFF
            errorCode |= 1 << 2;
        }

        // Now we free and check if the map is correct afterwards
        os_free(DRIVER, hugeChunk);
        if (!tt_isAreaUniform(os_getMapStart(DRIVER), os_getMapSize(DRIVER), 0x00)) {
            // there are some bytes in the map that are not 0
            errorCode |= 1 << 3;
        }
    }

    // Output of test-results, if there was an error
    if (errorCode) {
        TEST_FAILED("");
        delayMs(DEFAULT_OUTPUT_DELAY * 8);
        lcd_clear();
        lcd_writeProgString(PSTR("MAL|OWN|FIL|FRE|"));
        lcd_line2();
        for (uint8_t errorIndex = 0; errorIndex < 4; errorIndex++) {
            if (errorCode & (1 << errorIndex)) {
                lcd_writeProgString(PSTR("ERR|"));
            } else {
                lcd_writeProgString(PSTR("OK |"));
            }
        }
        HALT;
    }

    tt_phaseOK();
#endif

// Phase 3: Free
#if PHASE_3
    tt_phase(3, PSTR("free"));

    /*  Calculating the map-address for our hand-allocated block of memory.
     *  The block will be at the very end of the use-area because we write
     *  into the very end of the map-area
	 */
    uint16_t addr = os_getUseStart(DRIVER);
    addr -= SIZE / 2;

    // We build the first two entries of the map-area
    ProcessID process = os_getCurrentProc();
    process = (process << 4) | 0xF;

    /* Now we allocate memory by hand. This is done in two steps. First we
     * write the process-byte which contains the owner and an 0xF, then we
     * write the remaining 0xF until we allocated the memory we need.
	 */
    DRIVER->driver->write(addr, process);
    for (uint8_t i = 1; i < SIZE / 2; i++) {
        DRIVER->driver->write(addr + i, 0xFF);
    }
    // Fill the first half of the block with 0xFF
    for (uint8_t i = 0; i < SIZE / 2; i++) {
        DRIVER->driver->write(addr + (SIZE / 2) + i, 0xFF);
    }

    // Determine use address by calculating offset of map-entry to mapstart
    MemAddr  useaddr = ((addr - os_getMapStart(DRIVER)) * 2) + os_getUseStart(DRIVER);

    /* Free map
     * Expected state: free map + #SIZE/2 byte (0xFF) at the beginning of
     * use-area. If free is implemented wrongly, it will also zero out the
     * 0xFF at the beginning of the use-area
	 */
    os_free(DRIVER, useaddr);

    // Just like in phase 1 we use a bitmap to store errors
    errorCode = 0;

    // Check map data
    for (uint8_t i = 0; i < SIZE / 2; i++) {
        if (DRIVER->driver->read(addr + i)) {
            // The map was not freed
            errorCode |= 1 << 0;
        }
    }

    // Check map data
    for (uint8_t i = SIZE / 2; i < SIZE; i++) {
        if (!(DRIVER->driver->read(addr + i) == 0xFF)) {
            // The use area was touched
            errorCode |= 1 << 1;
        }
    }

    // Output of test-results, if there was an error
    if (errorCode) {
        TEST_FAILED("");
        delayMs(DEFAULT_OUTPUT_DELAY * 8);
        lcd_clear();
        lcd_writeProgString(PSTR("MAP|USE|"));
        lcd_line2();
        for (uint8_t errorIndex = 0; errorIndex < 2; errorIndex++) {
            if (errorCode & (1 << errorIndex)) {
                lcd_writeProgString(PSTR("ERR|"));
            } else {
                lcd_writeProgString(PSTR("OK |"));
            }
        }
        HALT;
    }


    tt_phaseOK();
#endif

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

/*!
 * Checks if all bytes in a certain memory-area are the same.
 * \param  start First address of area
 * \param  size  Length of area in bytes
 * \param  byte  Byte that is supposed to appear in this area
 */
uint8_t tt_isAreaUniform(MemAddr start, uint16_t size, MemValue byte) {
    for (uint16_t i = 0; i < size; i++) {
        if (DRIVER->driver->read(start + i) != byte) {
            return 0;
        }
    }
    return 1;
}
