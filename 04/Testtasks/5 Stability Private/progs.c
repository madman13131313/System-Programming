//-------------------------------------------------
//          TestTask: Stability Private
//-------------------------------------------------

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_core.h"
#include "os_scheduler.h"
#include "os_memory.h"

#if VERSUCH < 4
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
#define HEAP_COUNT 2
#define PROCESS_NUMBER 5
#define TEST_OS_MEM_FIRST 1
#define TEST_OS_MEM_NEXT  1
#define TEST_OS_MEM_BEST  1
#define TEST_OS_MEM_WORST 1
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



static uint8_t processCount = 0;

//! This program prints the current time in the first line.
REGISTER_AUTOSTART(program4)
void program4(void) {
    while (1) {
        os_enterCriticalSection();
        lcd_line1();
        lcd_writeProgString(PSTR("Time: "));
        Time     const msecs = os_systemTime_coarse();
        uint16_t const secs  = TIME_MS_TO_S(msecs);
        uint16_t const mins  = TIME_MS_TO_M(msecs);
        if (mins < 100) {
            lcd_writeDec(mins);
            lcd_writeChar('m');
            lcd_writeChar(' ');
            lcd_writeDec(secs % 60);
            lcd_writeChar('.');
            lcd_writeDec(msecs / 100 % 10);
            lcd_writeChar('s');
            lcd_writeChar(' ');
        } else {
            lcd_writeProgString(PSTR("> 100 min"));
        }
        if (mins >= 3) {
            if (processCount != PROCESS_NUMBER) {
                TEST_FAILED_AND_HALT("Invalid process count");
            }
            ATOMIC {
                TEST_PASSED;
                HALT;
            }
        }
        os_leaveCriticalSection();
        delayMs(DEFAULT_OUTPUT_DELAY);
    }
}

//! Writes characters consecutively into the second line of the LCD
void writeChar(char c) {
    static unsigned char pos = 16;
    os_enterCriticalSection();
    // Clear line if full
    if (++pos > 16) {
        pos = 1;
        lcd_line2();
        lcd_writeProgString(PSTR("                "));
    }
    lcd_goto(2, pos);
    lcd_writeChar(c);
    os_leaveCriticalSection();
}

//! Prints the status of the process
void printPhase(uint8_t id, char phase) {
    os_enterCriticalSection();
    writeChar(id + '0');
    writeChar(phase);
    writeChar(' ');
    os_leaveCriticalSection();
}

#define SZ 6

void makeCheck(uint8_t id, uint16_t mod, MemValue pat[SZ]) {
    uint16_t sizes[SZ * HEAP_COUNT];
    MemAddr sizedChunks[SZ * HEAP_COUNT];

    // Alloc chunks and test if they are within the valid range.
    // Then write the corresponding number from the pattern into them.
    printPhase(id, 'a');
    for (uint8_t i = 0; i < SZ; i++) { // Chunk
        for (uint8_t j = 0; j < HEAP_COUNT; j++) { // Heap
            Heap* heap = os_lookupHeap(j);
            sizes[i + SZ * j] = (TCNT0 % (mod * (j + 1))) + 1; // Get sizes from timer counter
            sizedChunks[i + SZ * j] = os_malloc(heap, sizes[i + SZ * j]);
            if (sizedChunks[i + SZ * j] < os_getUseStart(heap)) {
                TEST_FAILED_AND_HALT("Address too small");
            }
            if (sizedChunks[i + SZ * j] + sizes[i + SZ * j] > (os_getUseStart(heap) + os_getUseSize(heap))) { // This is NO off-by-one!
                TEST_FAILED_AND_HALT("Address too large");
            }
            for (uint16_t k = 0; k < sizes[i + SZ * j]; k++) {
                heap->driver->write(sizedChunks[i + SZ * j] + k, pat[i]);
            }
        }
    }

    // Just...wait.
    printPhase(id, 'b');
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

    // Are the correct numbers still contained in the right chunks?
    printPhase(id, 'c');
    for (uint8_t i = 0; i < SZ; i++) {
        for (uint8_t j = 0; j < HEAP_COUNT; j++) {
            Heap* heap = os_lookupHeap(j);
            for (uint16_t k = 0; k < sizes[i + SZ * j]; k++) {
                if (heap->driver->read(sizedChunks[i + SZ * j] + k) != pat[i]) {
                    if (j == 0) {
                        TEST_FAILED_AND_HALT("Pattern mismatch internal");
                    } else {
                        TEST_FAILED_AND_HALT("Pattern mismatch external");
                    }
                }
            }
        }
    }
    delayMs(DEFAULT_OUTPUT_DELAY);

    // Free all chunks.
    printPhase(id, 'd');
    for (uint8_t i = 0; i < SZ; i++) {
        for (uint8_t j = 0; j < HEAP_COUNT; j++) {
            Heap* heap = os_lookupHeap(j);
            os_free(heap, sizedChunks[i + SZ * j]);
        }
    }
}

//! Main test program.
REGISTER_AUTOSTART(program1)
void program1(void) {

    // First spawn consecutively numbered instances of this program.
    // Having increasing priority.
    uint8_t me = ++processCount;
    if (me < PROCESS_NUMBER) {
        os_exec(program1, me * 20);
    }

    // Create a characteristic pattern (number sequence) for each instance.
    MemValue pat[SZ];
    for (uint8_t i = 0; i < SZ; i++) {
        pat[i] = me * SZ + i;
    }

    // Prepare AllocStrats and cycle through them endlessly while
    // doing stability checks.
    uint8_t checkCount = 0, strategyId = 0;
    struct {
        AllocStrategy strat;
        char name;
    } cycle[] = {
#if TEST_OS_MEM_FIRST
        {.strat = OS_MEM_FIRST, .name = 'F'},
#endif
#if TEST_OS_MEM_NEXT
        {.strat = OS_MEM_NEXT , .name = 'N'},
#endif
#if TEST_OS_MEM_BEST
        {.strat = OS_MEM_BEST , .name = 'B'},
#endif
#if TEST_OS_MEM_WORST
        {.strat = OS_MEM_WORST, .name = 'W'},
#endif
    };
    uint8_t const cycleSize = sizeof(cycle) / sizeof(*cycle);
    while (1) {
        // One process changes the allocation strategy periodically
        if (me == 1 && !(checkCount++ % 8)) {
            os_enterCriticalSection();
            lcd_clear();
            delayMs(3 * DEFAULT_OUTPUT_DELAY);
            lcd_writeProgString(PSTR("Change to "));
            lcd_writeChar(cycle[strategyId].name);
            delayMs(5 * DEFAULT_OUTPUT_DELAY);
            for (uint8_t j = 0; j < HEAP_COUNT; j++) {
                Heap* heap = os_lookupHeap(j);
                os_setAllocationStrategy(heap, cycle[strategyId].strat);
            }
            delayMs(3 * DEFAULT_OUTPUT_DELAY);
            lcd_clear();
            strategyId = (strategyId + 1) % cycleSize;
            os_leaveCriticalSection();
        }

        makeCheck(me, me * 5, pat);
    }
}
