//-----------------------------------------------------
//          TestTask: Free Private
//-----------------------------------------------------

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_core.h"
#include "os_scheduler.h"
#include "os_memory.h"
#include "os_input.h"

#if VERSUCH < 5
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
#ifndef CONFIRM_REQUIRED
    #define CONFIRM_REQUIRED 1
#endif

#define TEST_FAILED_AND_HALT(reason) \
    do { \
        TEST_FAILED(reason); \
        HALT; \
    } while (0)


static bool errflag = false;

static int stderrWrapper(const char c, FILE* stream) {
    errflag = true;
    putchar(c);
    return 0;
}
static FILE *wrappedStderr = &(FILE)FDEV_SETUP_STREAM(stderrWrapper, NULL, _FDEV_SETUP_WRITE);

#define EXPECT_ERROR(label) \
    do { \
        lcd_clear(); \
        lcd_writeProgString(PSTR("Please confirm  ")); \
        lcd_writeProgString(PSTR(label ":")); \
        delayMs(DEFAULT_OUTPUT_DELAY * 16); \
        errflag = false; \
    } while (0)

#define EXPECT_NO_ERROR \
    do { \
        errflag = false; \
    } while (0)

#define ASSERT_ERROR(label) \
    do { \
        if (!errflag) { \
            TEST_FAILED_AND_HALT("No error (" label ")"); \
        } \
    } while (0)

#define ASSERT_NO_ERROR \
    do { \
        if (errflag) { \
            TEST_FAILED_AND_HALT("Unexpected error"); \
        } \
    } while (0)


REGISTER_AUTOSTART(program1)
void program1(void) {
    stderr = wrappedStderr;

    lcd_writeProgString(PSTR("Allocating "));
    lcd_line2();
    MemAddr sh, p;
    EXPECT_NO_ERROR;
    {
        os_malloc(intHeap, 1);
        lcd_writeProgString(PSTR("▮"));

        sh = os_sh_malloc(intHeap, 1);
        lcd_writeProgString(PSTR("▮"));

        os_malloc(intHeap, 1);
        lcd_writeProgString(PSTR("▮"));

        os_sh_malloc(intHeap, 1);
        lcd_writeProgString(PSTR("▮"));

        p = os_malloc(intHeap, 1);
        lcd_writeProgString(PSTR("▮"));

        os_sh_malloc(intHeap, 1);
        lcd_writeProgString(PSTR("▮"));
    }
    ASSERT_NO_ERROR;
    lcd_goto(2, 12);
    lcd_writeProgString(PSTR(" Done"));
    delayMs(DEFAULT_OUTPUT_DELAY * 10);

    EXPECT_ERROR("free priv as sh");
    os_sh_free(intHeap, &p);
    ASSERT_ERROR("free priv as sh");
    delayMs(DEFAULT_OUTPUT_DELAY * 4);

    EXPECT_ERROR("free sh as priv");
    os_free(intHeap, sh);
    ASSERT_ERROR("free sh as priv");
    delayMs(DEFAULT_OUTPUT_DELAY * 4);

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
