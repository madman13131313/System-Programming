//------------------------------------------------------------
//          TestTask: Allocation Strategies
//------------------------------------------------------------

#include <avr/pgmspace.h>
#include <stdbool.h>
#include <stdlib.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_core.h"
#include "os_memory.h"
#include "os_scheduler.h"
#include "os_input.h"

#if VERSUCH < 3
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
#define FIRST  1
#define NEXT   0 // Optional in Versuch 3
#define BEST   0 // Optional in Versuch 3
#define WORST  0 // Optional in Versuch 3
#define DRIVER intHeap
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


REGISTER_AUTOSTART(program1)
void program1(void) {

    // Struct-Array with Alloc.Strats
    struct {
        AllocStrategy strat;
        char name;
        bool check;
    } cycle[] = {
        {
            .strat = OS_MEM_FIRST,
            .name = 'f',
            .check = FIRST
        }, {
            .strat = OS_MEM_NEXT,
            .name = 'n',
            .check = NEXT
        }, {
            .strat = OS_MEM_BEST,
            .name = 'b',
            .check = BEST
        }, {
            .strat = OS_MEM_WORST,
            .name = 'w',
            .check = WORST
        }
    };

    /*
     * Create following pattern in memory: first big, second small, rest huge
     * X = allocated
     * __________________________________
     * |   |   |   | X |   |   | X |   | ...
     * 0   5   10  15  20  25  30  35
     *
     * malloc and free must be working correctly
     */
    MemAddr p[7];
    uint8_t s[] = {15, 5, 10, 5, 1};
    uint8_t error = 0;

    // Precheck heap size
    if (os_getUseSize(DRIVER) < 50) {
        TEST_FAILED("Heap too  small");
        HALT;
    }

    os_enterCriticalSection();
    lcd_clear();
    lcd_writeProgString(PSTR("Check strategy.."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    lcd_clear();
    os_leaveCriticalSection();

    uint16_t start = os_getMapStart(DRIVER);

    // Check if map is clean
    for (size_t i = 0; i < os_getMapSize(DRIVER); i++) {
        if (DRIVER->driver->read(start + i)) {
            TEST_FAILED("Map not free");
            HALT;
        }
    }

    // Check overalloc for all strategies
    for (uint8_t strategy = 0; strategy < 4; strategy++) {
        if (!cycle[strategy].check) {
            continue;
        }
        os_setAllocationStrategy(DRIVER, cycle[strategy].strat);
        if (os_malloc(DRIVER, os_getUseSize(DRIVER) + 1) != 0) {
            TEST_FAILED("Overalloc");
            HALT;
        }
    }

    // The test for next fit depends on not creating the memory pattern with OS_MEM_NEXT
    os_setAllocationStrategy(DRIVER, OS_MEM_FIRST);
    // Create pattern in memory
    for (uint8_t i = 0; i < 5; i++) {
        p[i] = os_malloc(DRIVER, s[i]);
    }
    for (uint8_t i = 0; i <= 4; i += 2) {
        os_free(DRIVER, p[i]);
    }

    // Check strategies
    for (uint8_t lastStrategy = 0; lastStrategy < 4; lastStrategy++) {

        if (!cycle[lastStrategy].check) {
            continue;
        }

        lcd_clear();
        lcd_writeProgString(PSTR("Checking strat.\n"));
        lcd_writeChar(cycle[lastStrategy].name);
        os_setAllocationStrategy(DRIVER, cycle[lastStrategy].strat);
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        lcd_clear();

        /*
         * We allocate, and directly free two times (except for BestFit)
         * for NextFit we should get different addresses
         * for FirstFit, both addresses are equal to first segment
         * for BestFit, first address equals second segment an second address equals first segment
         * for WorstFit we get the first byte of the rest memory
         * otherwise we found an error
         */
        for (uint8_t i = 5; i < 7; i++) {
            p[i] = os_malloc(DRIVER, s[2]);
            if (cycle[lastStrategy].strat != OS_MEM_BEST) {
                os_free(DRIVER, p[i]);
            }
        }


        // NextFit
        if (cycle[lastStrategy].strat == OS_MEM_NEXT) {
            if (p[5] != p[0] || p[6] != p[2]) {
                lcd_writeProgString(PSTR("Error NextFit"));
                delayMs(10 * DEFAULT_OUTPUT_DELAY);
                error = 1;
            } else {
                lcd_writeProgString(PSTR("NextFit Ok"));
            }
            delayMs(10 * DEFAULT_OUTPUT_DELAY);
        }

        // FirstFit
        else if (cycle[lastStrategy].strat == OS_MEM_FIRST) {
            if (p[5] != p[6] || p[5] != p[0]) {
                lcd_writeProgString(PSTR("Error FirstFit"));
                delayMs(10 * DEFAULT_OUTPUT_DELAY);
                error = 1;
            } else {
                lcd_writeProgString(PSTR("FirstFit Ok"));
            }
            delayMs(10 * DEFAULT_OUTPUT_DELAY);
        }
        // BestFit
        else if (cycle[lastStrategy].strat == OS_MEM_BEST) {
            if (p[5] != p[2] || p[6] != p[0]) {
                lcd_writeProgString(PSTR("Error BestFit"));
                delayMs(10 * DEFAULT_OUTPUT_DELAY);
                error = 1;
            } else {
                lcd_writeProgString(PSTR("BestFit Ok"));
            }
            delayMs(10 * DEFAULT_OUTPUT_DELAY);

            // Free manually as it wasn't done before
            if (p[5]) {
                os_free(DRIVER, p[5]);
            }
            if (p[6]) {
                os_free(DRIVER, p[6]);
            }
        }
        // WorstFit
        else if (cycle[lastStrategy].strat == OS_MEM_WORST) {
            if (p[4] != p[5] || p[5] != p[6]) {
                lcd_writeProgString(PSTR("Error WorstFit"));
                delayMs(10 * DEFAULT_OUTPUT_DELAY);
                error = 1;
            } else {
                lcd_writeProgString(PSTR("WorstFit Ok"));
            }
            delayMs(10 * DEFAULT_OUTPUT_DELAY);
        } else {
            lcd_writeChar('e');
        }
    }

    // Remove pattern
    for (uint8_t i = 1; i <= 3; i += 2) {
        os_free(DRIVER, p[i]);
    }

    // Check if map is clean
    for (size_t i = 0; i < os_getMapSize(DRIVER); i++) {
        if (DRIVER->driver->read(start + i)) {
            TEST_FAILED("Map not free afterwards");
            HALT;
        }
    }

    // Special NextFit test
    if (NEXT) {
        lcd_clear();
        lcd_writeProgString(PSTR("Special NextFit test"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        lcd_clear();
        os_setAllocationStrategy(DRIVER, OS_MEM_NEXT);
        size_t rema = os_getUseSize(DRIVER);
        for (uint8_t i = 0; i < 5; i++) {
            rema -= s[i];
        }
        p[0] = os_malloc(DRIVER, rema);
        os_free(DRIVER, p[0]);
        for (uint8_t i = 0; i < 3; i++) {
            p[i] = os_malloc(DRIVER, s[i]);
        }
        rema = os_getUseSize(DRIVER) - (s[0] + s[1] + s[2]);
        os_malloc(DRIVER, rema);
        for (uint8_t i = 1; i < 3; i++) {
            os_free(DRIVER, p[i]);
        }
        p[1] = os_malloc(DRIVER, s[1]);
        os_free(DRIVER, p[1]);
        if (!os_malloc(DRIVER, s[1] + s[2])) {
            os_enterCriticalSection();
            lcd_clear();
            lcd_writeProgString(PSTR("Error Next Fit (special)"));
            delayMs(10 * DEFAULT_OUTPUT_DELAY);
            error = 1;
        } else {
            lcd_writeProgString(PSTR("Ok"));
            delayMs(10 * DEFAULT_OUTPUT_DELAY);
        }
    }

    lcd_clear();
    if (error) {
        TEST_FAILED("Check failed");
        HALT;
    }

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
