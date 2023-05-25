//-------------------------------------------------
//          TestTask: Heap Start
//-------------------------------------------------

#include <avr/pgmspace.h>
#include <stdio.h>
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


//! Large global memory
uint8_t dummy[512];

//! Use a wrapper to detect errors
static int stderrWrapper(const char c, FILE* stream) {
    TEST_PASSED;
    HALT;
    return 0;
}
static FILE *wrappedStderr = &(FILE)FDEV_SETUP_STREAM(stderrWrapper, NULL, _FDEV_SETUP_WRITE);

//! Function declared with "__attribute__ ((constructor))" that is executed before the main function.
void __attribute__((constructor)) tt_heapStart() {
    lcdout = wrappedStderr;
}

REGISTER_AUTOSTART(program1)
void program1(void) {
    // Write something into the memory block to prevent omission at compile time.
    dummy[0] = 'A';
    dummy[sizeof(dummy) - 1] = 'B';

    TEST_FAILED("          Error expected");
    HALT;
}
