//-------------------------------------------------
//          TestTask: Shared Access
//-------------------------------------------------

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

//---- Adjust here what to test -------------------
#define NUM_MAL 5
#define SIZE    10
#define DRIVER  intHeap
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

#define TEST_FAILED_AND_HALT(reason) \
    do ATOMIC { \
        TEST_FAILED(reason); \
        HALT; \
    } while (0)


#if (NUM_MAL * SIZE > 255)
    #error Reduce SIZE or NUM_MAL
#endif
#if (SIZE < 4)
    #error Chunks must at least be 4 Bytes
#endif


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
        delayMs(DEFAULT_OUTPUT_DELAY * 10); \
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

MemAddr shBlock;
MemAddr shBlockYield;
volatile uint8_t state = 0;
volatile uint8_t mode = 0;


void check_rw_barriers(void) {
    state = 1;

    if (mode == 1) {
        os_sh_writeOpen(DRIVER, &shBlock);
        TEST_FAILED_AND_HALT("Read before write");
    } else if (mode == 2) {
        os_sh_readOpen(DRIVER, &shBlock);
        TEST_FAILED_AND_HALT("Write before read");
    } else if (mode == 3) {
        os_sh_writeOpen(DRIVER, &shBlock);
        TEST_FAILED_AND_HALT("Write before write");
    }
}

void check_readOpen_yield(void) {
    os_sh_readOpen(DRIVER, &shBlockYield);
    os_sh_readOpen(DRIVER, &shBlockYield);
    os_sh_readOpen(DRIVER, &shBlockYield);
    os_sh_readOpen(DRIVER, &shBlockYield);
    os_sh_readOpen(DRIVER, &shBlockYield);
    os_sh_readOpen(DRIVER, &shBlockYield);
    TEST_FAILED_AND_HALT("No yield when read opened");
}

void check_readwriteOpen_yield(void) {
    if (mode == 0) {
        os_sh_writeOpen(DRIVER, &shBlockYield);
        os_sh_readOpen(DRIVER, &shBlockYield);
        os_sh_readOpen(DRIVER, &shBlockYield);
        TEST_FAILED_AND_HALT("No yield when w/r opened");
    } else if (mode == 1) {
        os_sh_readOpen(DRIVER, &shBlockYield);
        os_sh_readOpen(DRIVER, &shBlockYield);
        os_sh_writeOpen(DRIVER, &shBlockYield);
        TEST_FAILED_AND_HALT("No yield when r/w opened");
    }
}

REGISTER_AUTOSTART(program1)
void program1(void) {
    MemAddr allocs[NUM_MAL];
    MemAddr opBlock1, opBlock2;

    stderr = wrappedStderr;

    // Allocate some shared memory
    lcd_writeProgString(PSTR("1: Allocating..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    for (uint8_t i = 0; i < NUM_MAL; i++) {
        EXPECT_NO_ERROR;
        lcd_clear();
        lcd_writeDec(i);
        allocs[i] = os_sh_malloc(DRIVER, 10);

        // Assure that the allocated memory chunks are pairwise distinct
        bool different = true;
        for (uint8_t j = 0; different && j < i; j++) {
            different = allocs[j] != allocs[i];
        }
        lcd_writeChar(' ');
        if (!different) {
            TEST_FAILED_AND_HALT("          1: Allocating");
        }
        lcd_writeProgString(PSTR("OK"));
        ASSERT_NO_ERROR;
        delayMs(DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Test if locks can be opened and closed.
    lcd_writeProgString(PSTR("2a: Accessing (open/close)..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    for (uint8_t i = 0; i < NUM_MAL; i++) {
        EXPECT_NO_ERROR;
        lcd_clear();
        lcd_writeDec(i);
        lcd_writeChar(' ');
        for (uint8_t j = 0; j < SIZE; j++) {
            MemAddr const p = allocs[i] + j;
            os_sh_writeOpen(DRIVER, &p);
            os_sh_close(DRIVER, p);
            os_sh_readOpen(DRIVER, &p);
            os_sh_close(DRIVER, p);
        }
        ASSERT_NO_ERROR;
        lcd_writeProgString(PSTR("OK"));
        delayMs(DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Test if locks can read on private memory
    lcd_writeProgString(PSTR("2b: Read on private..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        EXPECT_ERROR("read on private");
        MemAddr privBlock = os_malloc(DRIVER, 10);
        os_sh_readOpen(DRIVER, &privBlock);
        os_free(DRIVER, privBlock);
        ASSERT_ERROR("read on private");
        delayMs(5 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Test if locks can write on private memory
    lcd_writeProgString(PSTR("2c: Write on private..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        EXPECT_ERROR("write on priv.");
        MemAddr privBlock = os_malloc(DRIVER, 10);
        os_sh_writeOpen(DRIVER, &privBlock);
        os_free(DRIVER, privBlock);
        ASSERT_ERROR("write on priv.");
        delayMs(5 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Do some simple write operations
    lcd_writeProgString(PSTR("3: Writing..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    for (uint8_t i = 0; i < NUM_MAL; i++) {
        EXPECT_NO_ERROR;
        lcd_clear();
        lcd_writeDec(i);
        lcd_writeChar(' ');
        for (uint8_t j = 0; j < SIZE; j++) {
            MemValue const a = i * SIZE + j;
            MemAddr const p = allocs[i];
            os_sh_write(DRIVER, &p, j, &a, 1);
        }
        ASSERT_NO_ERROR;
        lcd_writeProgString(PSTR("OK"));
        delayMs(DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Read the written pattern and test if it's still the same
    lcd_writeProgString(PSTR("4: Reading..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    for (uint8_t i = 0; i < NUM_MAL; i++) {
        EXPECT_NO_ERROR;
        lcd_clear();
        lcd_writeDec(i);
        lcd_writeChar(' ');
        MemValue a;
        for (uint8_t j = 0; j < SIZE; j++) {
            MemAddr const p = allocs[i];
            a = (MemValue) - 1;
            os_sh_read(DRIVER, &p, j, &a, 1);
            if (a != i * SIZE + j) {
                cli();
                lcd_clear();
                lcd_writeDec(i);
                lcd_writeChar(' ');
                lcd_writeProgString(PSTR("FAILURE @ "));
                lcd_writeDec(j);
                lcd_writeChar('/');
                lcd_writeDec(SIZE);
                delayMs(DEFAULT_OUTPUT_DELAY * 10);
                TEST_FAILED_AND_HALT("          4: Reading");
                sei();
            }
        }
        ASSERT_NO_ERROR;
        lcd_writeProgString(PSTR("OK"));
        delayMs(DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Now read past the end of the allocated memory to provoke an error
    lcd_writeProgString(PSTR("5: Provoking viol. (read)..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        EXPECT_ERROR("read violation");
        MemAddr const p = allocs[1] + 1;
        uint32_t a;
        os_sh_read(DRIVER, &p, SIZE - 3, (MemValue*)&a, 4);
        ASSERT_ERROR("read violation");
        delayMs(5 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // As above just with writing
    lcd_writeProgString(PSTR("6: Provoking viol. (write)..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        EXPECT_ERROR("write violation");
        MemAddr const p = allocs[1] + 2;
        MemValue const a = 0xFF;
        os_sh_write(DRIVER, &p, SIZE, &a, 1);
        ASSERT_ERROR("write violation");
        delayMs(5 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Now check if the write operation was executed despite the access violation
    lcd_writeProgString(PSTR("7: Checking..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        MemAddr const p = allocs[2];
        MemValue a = 0xBB;
        os_sh_read(DRIVER, &p, 0, &a, 1);
        delayMs(3 * DEFAULT_OUTPUT_DELAY);
        if (a != 2 * SIZE + 0) {
            TEST_FAILED_AND_HALT("          7: Checking");
        }
        lcd_clear();
        lcd_writeProgString(PSTR("OK"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Check parallel reading
    lcd_writeProgString(PSTR("8: Multiple access..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        shBlock = os_sh_malloc(DRIVER, 10);
        if (shBlock == 0) {
            TEST_FAILED_AND_HALT("Not enough memory");
        }
        shBlockYield = os_sh_malloc(DRIVER, 10);
        if (shBlockYield == 0) {
            TEST_FAILED_AND_HALT("Not enough memory");
        }

        lcd_clear();
        lcd_writeProgString(PSTR("readOpen Yield"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        ProcessID readOpenProc = os_exec(check_readOpen_yield, DEFAULT_PRIORITY);
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        os_kill(readOpenProc);

        lcd_clear();
        lcd_writeProgString(PSTR("write/readOpen  Yield"));
        mode = 0;
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        ProcessID writeReadOpenProc = os_exec(check_readwriteOpen_yield, DEFAULT_PRIORITY);
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        os_kill(writeReadOpenProc);

        lcd_clear();
        lcd_writeProgString(PSTR("read/writeOpen  Yield"));
        mode = 1;
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        ProcessID readWriteOpenProc = os_exec(check_readwriteOpen_yield, DEFAULT_PRIORITY);
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        os_kill(readWriteOpenProc);

        // Open the block twice (-> test parallel reading)
        opBlock1 = os_sh_readOpen(DRIVER, &shBlock);
        if (opBlock1 != shBlock) {
            TEST_FAILED_AND_HALT("Address has changed (1)");
        }

        lcd_clear();
        lcd_writeProgString(PSTR("1x readOpen"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        opBlock2 = os_sh_readOpen(DRIVER, &shBlock);
        if (opBlock2 != shBlock) {
            TEST_FAILED_AND_HALT("Address has changed (2)");
        }

        lcd_clear();
        lcd_writeProgString(PSTR("2x readOpen"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        EXPECT_NO_ERROR;
        os_sh_close(DRIVER, opBlock2);
        ASSERT_NO_ERROR;

        lcd_clear();
        lcd_writeProgString(PSTR("OK"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

    // Check read/write barriers
    lcd_writeProgString(PSTR("9: Checking RW barriers..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        // Read before write (opBlock1 is still read-open here!):
        lcd_clear();
        lcd_writeProgString(PSTR("Read before\nwrite"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        // Set behavior of program 2: readOpen
        mode = 1;
        os_exec(check_rw_barriers, DEFAULT_PRIORITY);
        // Grant process at least one time slot
        while (state == 0);
        delayMs(5 * DEFAULT_OUTPUT_DELAY);
        os_kill(2);
        state = 0;
        os_sh_close(DRIVER, opBlock1);

        lcd_clear();
        lcd_writeProgString(PSTR("OK"));
        delayMs(5 * DEFAULT_OUTPUT_DELAY);

        // Write before read:
        lcd_clear();
        lcd_writeProgString(PSTR("Write before\nread"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        opBlock1 = os_sh_writeOpen(DRIVER, &shBlock);
        if (opBlock1 != shBlock) {
            TEST_FAILED_AND_HALT("Address has changed (3)");
        }

        // Set behavior of program 2: writeOpen
        mode = 2;
        os_exec(check_rw_barriers, DEFAULT_PRIORITY);
        // Grant process at least one time slot
        while (state == 0);
        delayMs(5 * DEFAULT_OUTPUT_DELAY);
        os_kill(2);
        state = 0;

        lcd_clear();
        lcd_writeProgString(PSTR("OK"));
        delayMs(5 * DEFAULT_OUTPUT_DELAY);

        // Write before write:
        lcd_clear();
        lcd_writeProgString(PSTR("Write before\nwrite"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        // Set behavior of program 2: readOpen
        mode = 3;
        os_exec(check_rw_barriers, DEFAULT_PRIORITY);
        // Grant process at least one time slot
        while (state == 0);
        delayMs(5 * DEFAULT_OUTPUT_DELAY);
        os_kill(2);
        state = 0;
        os_sh_close(DRIVER, opBlock1);

        lcd_clear();
        lcd_writeProgString(PSTR("OK"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();


    // Check offset
    lcd_writeProgString(PSTR("10: Checking offset..."));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    {
        MemValue value = 0;
        MemValue pat[10];

        // Write pattern (opBlock1 is still write-open here!):
        for (uint8_t i = 0; i < 10; i++) {
            MemValue const a = i + 1;
            os_sh_write(DRIVER, &shBlock, i, &a, 1);
        }

        // Read pattern without offset
        // The read pattern should always be the first one
        for (uint8_t i = 0; i < 10; i++) {
            opBlock1 = shBlock + i;
            os_sh_read(DRIVER, &opBlock1, 0, &value, 1);
            if (value != 1) {
                TEST_FAILED_AND_HALT("Pattern mismatch (1)");
            }
        }

        // Read pattern one by one with offset
        for (uint8_t i = 0; i < 10; i++) {
            os_sh_read(DRIVER, &shBlock, i, &value, 1);
            if (value != i + 1) {
                TEST_FAILED_AND_HALT("Pattern mismatch (2)");
            }
        }

        // Read pattern all at once without offset
        os_sh_read(DRIVER, &shBlock, 0, (MemValue*)&pat, 10);
        for (uint8_t i = 0; i < 10; i++) {
            if (pat[i] != i + 1) {
                TEST_FAILED_AND_HALT("Pattern mismatch (3)");
            }
        }

        // Read pattern-blocks with false and correct offset
        for (uint8_t i = 0; i < 10; i++) {
            opBlock1 = shBlock + i;
            os_sh_read(DRIVER, &opBlock1, i, (MemValue*)&pat, 10 - i);

            for (uint8_t j = 0; j < 10 - i; j++) {
                if (pat[j] != i + j + 1) {
                    TEST_FAILED_AND_HALT("Pattern mismatch (4)");
                }
            }
        }

        lcd_clear();
        lcd_writeProgString(PSTR("OK"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
    }
    lcd_clear();

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
