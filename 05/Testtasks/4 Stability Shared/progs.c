//-------------------------------------------------
//          TestTask: Stability Shared
//-------------------------------------------------

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdbool.h>
#include <string.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_core.h"
#include "os_scheduler.h"
#include "os_memory.h"

#if VERSUCH < 5
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
#define DRIVER intHeap
#define PROCESS_NUMBER 3
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

// Handy error message define
#define TEST_FAILED_AND_HALT(reason) \
    do ATOMIC { \
        TEST_FAILED(reason); \
        HALT; \
    } while (0)

#if PROCESS_NUMBER + 2 > 9
    #warning "Please lower PROCESS_NUMBER"
#endif

unsigned char pos = 16;
volatile uint16_t writeItrs = 0;
bool expectProcesses = false;
uint16_t seenProcesses = 0;
static char PROGMEM const spaces16[] = "                ";
MemAddr sh;
uint16_t sz;

//! This program prints the current time in the first line
REGISTER_AUTOSTART(program4)
void program4(void) {
    uint16_t writeItrsAtEnd = 0;
    while (1) {
        os_enterCriticalSection();
        lcd_line1();
        lcd_writeProgString(PSTR("Time: ")); // +6
        Time     const msecs = os_systemTime_coarse();
        uint16_t const secs  = TIME_MS_TO_S(msecs);
        uint16_t const mins  = TIME_MS_TO_M(msecs);
        if (mins < 100) {
            lcd_writeDec(mins); // +2
            lcd_writeChar('m'); // +1
            lcd_writeChar(' '); // +1
            lcd_writeDec(secs % 60); // +2
            lcd_writeChar('.'); // +1
            lcd_writeDec(msecs / 100 % 10); // +2
            lcd_writeChar('s'); // +1
            lcd_writeChar(' '); // +1
        } else {
            lcd_writeProgString(PSTR("> 100 min"));
        }
        if (mins >= 3) {
            // Save how many iterations have been made
            if (writeItrsAtEnd == 0) {
                writeItrsAtEnd = writeItrs;
            }
            // We expect to still see all the processes
            expectProcesses = true;
            for (uint8_t i = 0; i <= PROCESS_NUMBER; i++) {
                if (i == PROCESS_NUMBER) { // All processes have been seen
                    // Check if iterations are still counting
                    if (writeItrs - writeItrsAtEnd >= 10) {
                        ATOMIC {
                            TEST_PASSED;
                            HALT;
                        }
                    }
                }
                // Check if process has been seen
                if (!gbi(seenProcesses, i)) {
                    // Process has not been seen yet
                    break;
                }
            }
        }
        if (mins >= 4) {
            // Timeout reached
            TEST_FAILED_AND_HALT("          Missing process");
        }
        lcd_goto(2, pos + 1);
        os_leaveCriticalSection();
        delayMs(DEFAULT_OUTPUT_DELAY);
    }
}

//! Write a character to the correct position in the second line after the pipe
void writeChar(char c) {
    os_enterCriticalSection();
    if (++pos > 16) {
        pos = 5;
        lcd_line2();
        lcd_goto(2, pos);
        lcd_writeChar('|');
        pos++;
        lcd_writeProgString(spaces16 + 5);
    }
    lcd_goto(2, pos);
    lcd_writeChar(c);
    os_leaveCriticalSection();
}

//! Main test function
void makeCheck(uint16_t size, MemValue pat) {
    // Allocate private memory and read in the data of the shared memory
    MemAddr const priv = os_malloc(intHeap, size);
    os_sh_read(DRIVER, &sh, 0, (uint8_t*)priv, size);

    // Test if only one pattern is present throughout the memory chunk
    MemValue const check = intHeap->driver->read(priv);
    for (MemAddr p = priv; p < priv + size; p++) {
        unsigned char tmp = intHeap->driver->read(p);
        if (tmp != check) {
            TEST_FAILED_AND_HALT("Write was interleaved");
        }
        intHeap->driver->write(p, pat);
    }

    // Write private memory (now with new pattern) back to shared and free the private one
    os_sh_write(DRIVER, &sh, 0, (uint8_t*)priv, size);
    os_free(intHeap, priv);
}

//! Function for reallocation of shared memory
void myRelocate(uint16_t newsz) {

    // Allocate new shared memory and write zeros to it
    MemAddr newsh = os_sh_malloc(DRIVER, newsz);
    // Important because our programs expect all cells to be the same
    for (MemAddr i = 0; i < sz; i++) {
        DRIVER->driver->write(newsh + i, 0);
    }

    // Swap newly allocated shared memory with public shared memory
    os_enterCriticalSection();
    sh ^= newsh;
    newsh ^= sh;
    sh ^= newsh;
    sz = newsz;
    os_leaveCriticalSection();

    // Now free the old shared memory
    os_sh_free(DRIVER, &newsh);
}

/*!
 * Program that writes a pattern to the shared memory region and reallocates it
 * using a newly introduced function. Furthermore, it checks whether the pattern
 * in shared memory is consistent.
 */
void pattern_program(void) {
    while (1) {
        uint8_t k = (TCNT0 % 40);
        while (k--) {
            makeCheck(sz, os_getCurrentProc());
        }
        myRelocate(sz);
        os_enterCriticalSection();
        writeChar(os_getCurrentProc() + '0');
        if (expectProcesses) {
            sbi(seenProcesses, os_getCurrentProc() - 3);
        }
        os_leaveCriticalSection();
    }
}

/*!
 * Entry program that creates several instances of the test program
 * and checks for successful mutual exclusions
 */
REGISTER_AUTOSTART(program1)
void program1(void) {
    uint8_t counter = PROCESS_NUMBER;

    // Allocate shared memory and write to it
    sz = os_getUseSize(intHeap) / (counter + 1);
    sh = os_sh_malloc(DRIVER, sz);
    for (MemAddr i = 0; i < sz; i++) {
        DRIVER->driver->write(sh + i, 0xFF);
    }

    // Start checking processes
    while (counter--) {
        os_exec(pattern_program, 10);
    }

    // Do iteration printout
    MemValue start;
    MemValue end;
    while (1) {
        // Read from shared memory to test if a writing operation is in progress by another process
        // If writing to shared memory uses critical section as locks this will never be the case
        cli();
        start = DRIVER->driver->read(sh);
        end = DRIVER->driver->read(sh + sz - 1);
        sei();
        if (start != end) {
            writeItrs++;
            cli();
            lcd_goto(2, 1);
            lcd_writeProgString(spaces16 + (16 - 4));
            lcd_goto(2, 1);
            lcd_writeDec(writeItrs);
            sei();
            Time const now = os_systemTime_coarse(); // Granularity: 0.1/8 ms
            while (now == os_systemTime_coarse()) {
                os_yield();
            }
        }
        os_yield();
    }
}
