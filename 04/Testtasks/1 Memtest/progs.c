//-------------------------------------------------
//          TestTask: Memtest
//-------------------------------------------------

#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <avr/interrupt.h>
#include <string.h>

#include "lcd.h"
#include "util.h"
#include "os_core.h"
#include "os_scheduler.h"
#include "os_memory.h"
#include "os_memheap_drivers.h"
#include "os_input.h"

#if VERSUCH < 4
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
//! Set this to 1 in order to execute the test phase, 0 to skip the phase
#define PHASE_1 1
#define PHASE_2 1
#define PHASE_3 1
#define PHASE_4 1
#define PHASE_5 1
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

//! Number of bytes the processes for the concurrency test write
#define CONCURRENCY_BYTES   (1000)
//! Number of processes that try to write in the last phase
#define CONCURRENCY_PROCS   (4)
//! Number of address bits on the external memory board minus 1
#define ADDRESS_BITS        (sizeof(MemAddr) * 8)
//! Highest address as size reference value
#define MAX_ADDRESS         (~(MemAddr) 0)

//! The heap-driver we expect to find at position 1 of the heap-list
#define DRIVER              extHeap
//! Number of bytes we allocate by hand. Must be even and >2.
#define MALLOC_SIZE         (10)

/************************************************************************/
/* Typedef                                                              */
/************************************************************************/

//! Factor for math-magic
typedef uint8_t Factor;

/*!
 * Definition for MEMTEST-functions.
 * \param f       Magic factor that is supposed to assure that most of the
 *                use-cases are covered.
 * \param round   The current round the test is executed in. For more
 *                information about rounds see performTest.
 * \param cell    Cell the test is to be executed on
 * \param errors  Place to store the errors that occurred.
 */
#define MEMTEST(NAME) void NAME(Factor f, uint8_t round, uint16_t cell, uint32_t* errors)

/************************************************************************/
/* Forward declarations                                                 */
/************************************************************************/

//! Prints occurring memtest-errors to the LCD.
void tt_renderError(uint32_t errors, bool errorsAsHex, bool show0);
//! Performs one of the MEMTEST-functions in order to check if external memory
char tt_performTest(uint8_t testId, uint8_t iterations, uint16_t max, bool errorsAsHex, char const* testName, MEMTEST((*test)));
//! Checks if all bytes in a certain memory-area are the same.
uint8_t tt_isAreaUniform(MemAddr start, uint16_t size, MemValue byte);

//! Prints phase information to the LCD.
void tt_showPhase(uint8_t i, char const* name);
//! Prints OK for the phase message.
void tt_phaseSuccess(void);

// Memtests

//! Test basic read/write functionality.
MEMTEST(tt_doubleAccess);
//! Tests if address is applied correctly to external memory.
MEMTEST(tt_addressBits);
//! Test if memory can remember a pattern
MEMTEST(tt_integrity);
//! Tests if everything of the above works.
MEMTEST(tt_inversions);

/************************************************************************/
/* Global variables                                                     */
/************************************************************************/


//! Number of started processes in the concurrency phase
volatile uint8_t startedProcs = 0;
//! Number of finished processes in the concurrency phase
volatile uint8_t finishedProcs = 0;
//! Pattern to write in the concurrency phase
uint8_t pattern[] = {0b11001100, 0b10101010, 0b01010101, 0b11110000, 0b11000011};
//! Remembers, if there was a pattern mismatch or not
volatile uint8_t patternMismatch = 0;

/************************************************************************/
/* Programs                                                             */
/************************************************************************/

Program program2;

/*!
 * The test consists of several sub-tests checking different aspects of the driver.
 */
REGISTER_AUTOSTART(program1)
void program1(void) {
    // 1. Phase: Sanity checks
#if PHASE_1
    {
        tt_showPhase(1, PSTR("Sanity check"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        // If there are any problems, we set a certain bit in the bitmask
        // "errorCode", indicating what exactly went wrong.
        uint8_t errorCode = 0;

        // Check if the length of the heaplist is correct
        // If not, set bit 0
        errorCode |= !(os_getHeapListLength() == 2) << 0;
        // Check if the driver returned by lookupHeap is the correct one
        // If not, set bit 1
        errorCode |= !(os_lookupHeap(0) == intHeap) << 1;
        errorCode |= !(os_lookupHeap(1) == extHeap) << 1;
        // Check if the external heap uses all of the available space.
        // If not, set bit 2
        // Some unusual calculations have to be performed for this check to avoid an overflow
        errorCode |= !((MAX_ADDRESS - 2) / 3 + 1 == os_getMapSize(extHeap)) << 2;
        // Check if map- and use-area have a size relationship of 1:2
        // If not, set bit 3
        errorCode |= !(os_getMapSize(intHeap) == os_getUseSize(intHeap) - os_getMapSize(intHeap)) << 3;
        errorCode |= !(os_getMapSize(extHeap) == os_getUseSize(extHeap) - os_getMapSize(extHeap)) << 3;

        // Check if there is an error
        if (errorCode) {
            TEST_FAILED("");
            delayMs(DEFAULT_OUTPUT_DELAY * 8);
            lcd_clear();
            lcd_writeProgString(PSTR("LST|DRV|SIZ|1:2|"));
            for (uint8_t errorIndex = 0; errorIndex < 4; errorIndex++) {
                if (errorCode & (1 << errorIndex)) {
                    lcd_writeProgString(PSTR("ERR|"));
                } else {
                    lcd_writeProgString(PSTR("OK |"));
                }
            }
            HALT;
        }

        tt_phaseSuccess();
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        lcd_clear();
    }
#endif

    // 2. Phase: Memory-Driver
#if PHASE_2
    {
        tt_showPhase(2, PSTR("Mem-Driver"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        uint8_t pass = 0;
        uint8_t testId = 0;

        // As a reminder, here the signature of performTest:
        // performTest(uint8_t testId, uint8_t iterations, uint16_t max, bool errorsAsHex, char const* testName, MEMTEST((*test)))


        // As you can see, this test performs exactly one round, but in this round
        // it tests all possible addresses for tt_doubleAccess
        pass += !tt_performTest(++testId, 1, MAX_ADDRESS, false, PSTR("double access"), tt_doubleAccess);

        /*
         * AddressBits already performs two rounds. In the first round the address
         * lines are tested from the LSB to the MSB and in the second round this
         * order is inverted.
         */
        pass += !tt_performTest(++testId, 2, ADDRESS_BITS - 1, false, PSTR("address bits"), tt_addressBits);

        // Integrity is tested for all addresses. In the first round values are
        // written to the cell. In the second round, values are read from the cells.
        pass += !tt_performTest(++testId, 2, MAX_ADDRESS, false, PSTR("memory integrity"), tt_integrity);

        // Inversions is the longest test - it has 6 rounds. This means there are
        // 6 write/read cycles.
        pass += !tt_performTest(++testId, 6, MAX_ADDRESS, false, PSTR("inversions"), tt_inversions);

        // Check if one test failed
        if (pass == testId) {
            tt_showPhase(2, PSTR("Mem-Driver"));
            tt_phaseSuccess();
            delayMs(10 * DEFAULT_OUTPUT_DELAY);
            lcd_clear();
        } else {
            tt_showPhase(2, PSTR("Mem-Driver"));
            TEST_FAILED("Phase 2");
            lcd_line2();
            lcd_writeProgString(PSTR("Failed tests: "));
            lcd_writeDec(testId - pass);
            HALT;
        }
    }
#endif

    // 3. Phase: Malloc
#if PHASE_3
    {
        tt_showPhase(3, PSTR("malloc"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        // If there are any problems, we set a certain bit in the bitmask
        // "errorCode", indicating what exactly went wrong.
        uint8_t errorCode = 0;

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
                // There are some bytes in the map that are not 0
                errorCode |= 1 << 3;
            }
        }

        // Check if there is an error
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

        tt_phaseSuccess();
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        lcd_clear();
    }
#endif

    // 4. Phase: Free
#if PHASE_4
    {
        tt_showPhase(4, PSTR("free"));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);

        /*
         * Calculating the map-address for our hand-allocated block of memory.
         * The block will be at the very end of the use-area because we write
         * into the very end of the map-area.
         */
        uint16_t addr = os_getUseStart(DRIVER);
        addr -= MALLOC_SIZE / 2;

        // We build the first two entries of the map-area
        ProcessID process = os_getCurrentProc();
        process = (process << 4) | 0xF;

        /*
         * Now we allocate memory by hand. This is done in two steps. First we
         * write the process-byte which contains the owner and an 0xF, then we
         * write the remaining 0xF until we allocated the memory we need.
         */
        DRIVER->driver->write(addr, process);
        for (uint8_t i = 1; i < MALLOC_SIZE / 2; i++) {
            DRIVER->driver->write(addr + i, 0xFF);
        }
        // Fill the first half of the block with 0xFF
        for (uint8_t i = 0; i < MALLOC_SIZE / 2; i++) {
            DRIVER->driver->write(addr + (MALLOC_SIZE / 2) + i, 0xFF);
        }

        // Determine use address by calculating offset of map-entry to map-start
        MemAddr  useaddr = ((addr - os_getMapStart(DRIVER)) * 2) + os_getUseStart(DRIVER);

        /*
         * Free map
         * Expected state: free map + #SIZE/2 byte (0xFF) at the beginning of
         * use-area. If free is implemented wrongly, it will also zero out the
         * 0xFF at the beginning of the use-area
         */
        os_free(DRIVER, useaddr);

        // If there are any problems, we set a certain bit in the bitmask
        // "errorCode", indicating what exactly went wrong.
        uint8_t errorCode = 0;

        // Check map data
        for (uint8_t i = 0; i < MALLOC_SIZE / 2; i++) {
            if (DRIVER->driver->read(addr + i)) {
                // The map was not freed
                errorCode |= 1 << 0;
            }
        }

        // Check map data
        for (uint8_t i = MALLOC_SIZE / 2; i < MALLOC_SIZE; i++) {
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

        tt_phaseSuccess();
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        lcd_clear();
    }
#endif

    // 5. Phase: Critical Sections
#if PHASE_5
    {
        // Test on critical sections in read/write driver functions.
        // Doing lots of concurrent read/write operations on ext. memory.
        tt_showPhase(5, PSTR("Concurrency..."));
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
        os_setSchedulingStrategy(OS_SS_RANDOM);

        /*
         * We start 4 processes that try to access the external memory a lot.
         * If critical sections were used wrongly, they will interrupt each
         * others writing or reading processes and destroy that pattern that
         * was written
         */
        for (uint8_t i = 0; i < CONCURRENCY_PROCS; i++) {
            os_exec(program2, DEFAULT_PRIORITY);
        }

        // Wait for them to finish
        while (finishedProcs != CONCURRENCY_PROCS) {
            delayMs(10 * DEFAULT_OUTPUT_DELAY);
        }

        // Check if successful
        lcd_clear();
        tt_showPhase(5, PSTR("Concurrency"));
        if (patternMismatch) {
            TEST_FAILED("Pattern mismatch");
            HALT;
        } else {
            tt_phaseSuccess();
        }
        delayMs(10 * DEFAULT_OUTPUT_DELAY);
    }
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
 * This program is used in order check if the student implemented critical
 * sections correctly or not. It does so by choosing a unique memory area for
 * a process of this program. This area is then filled several times with a
 * pattern, which is then read. If the read pattern does not match the written
 * pattern, we must have been interrupted during memory-access by another
 * process.
 */
void program2(void) {
    /*
     * It is important to get a unique number between 0 and 3. That way we can
     * be sure that the memory block we use is not used by anyone else. If it
     * were this test would not be as efficient.
     */
    os_enterCriticalSection();
    uint8_t me = startedProcs;
    startedProcs++;
    os_leaveCriticalSection();

    // The first address of the memory area that we chose
    MemAddr addr = CONCURRENCY_BYTES * me;

    // Wait for all processes to be started.
    while (startedProcs != CONCURRENCY_PROCS);

    // Write and read and hope to not be interrupted.
    // For each address, we write the pattern 15 times. This is done in order to
    // achieve a huge amount of memory accesses in the beginning of this task.
    for (uint16_t offset = 0; offset < CONCURRENCY_BYTES; offset++) {
        for (uint8_t iteration = 0; iteration < 15; iteration++) {
            extSRAM->write(addr + offset, pattern[me] ^ (1 << (offset % 8)));
        }
    }

    // For each address we read the pattern again 15 times
    MemValue val;
    for (uint16_t offset = 0; offset < CONCURRENCY_BYTES; offset++) {
        for (uint8_t iteration = 0; iteration < 15; iteration++) {
            val = extSRAM->read(addr + offset);
            if (val != (pattern[me] ^ (1 << (offset % 8)))) {
                // It happened. We were interrupted by another process when we were
                // reading or writing to the external memory
                os_enterCriticalSection();
                if (!patternMismatch) {
                    // we only show this message once
                    lcd_clear();
                    lcd_writeProgString(PSTR("Pattern mismatch"));
                    delayMs(20 * DEFAULT_OUTPUT_DELAY);
                    patternMismatch = 1;
                }
                lcd_clear();
                lcd_writeProgString(PSTR("Proc "));
                lcd_writeDec(me + 1);
                lcd_writeProgString(PSTR(" @ "));
                lcd_writeHex(addr + offset);
                lcd_writeChar(':');
                lcd_line2();
                lcd_writeHex(val);
                lcd_writeProgString(PSTR("!="));
                lcd_writeHex(pattern[me] ^ (1 << (offset % 8)));
                os_waitForInput();
                /*
                 * We want to give the user the chance to see where he went wrong, so
                 * we show all errors. That means, we do not halt here, instead we let
                 * them scroll through them
                 */
                os_waitForNoInput();
                os_leaveCriticalSection();
                break; // We do not need to see the same error 15 times, do we?
            }
        }
    }

    finishedProcs++;

    // We already have a heap-cleanup, so if a process dies, the map-area will
    // be altered. That would corrupt the memory
    while (finishedProcs != CONCURRENCY_PROCS) {
        // Nop
    }
}


/************************************************************************/
/* Function implementations                                             */
/************************************************************************/


// Basic test functionality

/*!
 * Prints phase information to the LCD.
 * \param  i    Index of phase
 * \param *name Name of phase
 */
void tt_showPhase(uint8_t i, char const* name) {
    lcd_clear();
    lcd_writeProgString(PSTR("Phase "));
    lcd_writeDec(i);
    lcd_writeChar(':');
    lcd_line2();
    lcd_writeProgString(name);
}

/*!
 * Prints OK for the phase message.
 */
void tt_phaseSuccess(void) {
    lcd_goto(2, 15);
    lcd_writeProgString(PSTR("OK"));
}


// Specialized test functionality

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

/*!
 * Prints occurring memtest-errors to the LCD.
 * \param  errors      Number of errors that occurred
 * \param  errorsAsHex Set this to true if you want error-numbers to be
 *                     printed in hexadecimal
 * \param  show0       This parameter decides if a zero is to be printed if
 *                     there are no errors.
 */
void tt_renderError(uint32_t errors, bool errorsAsHex, bool show0) {
    if (show0 || errors) {
        if (errorsAsHex) {
            lcd_writeHexWord(errors);
        } else {
            // The following distinction is made because the lcd-header can only
            // print decimals up to 0xFFFF
            if (errors >= (1ul << 16)) {
                lcd_writeChar('>');
                lcd_writeDec(-1);
            } else {
                lcd_writeDec(errors);
            }
        }
    }
}

/*!
 * Performs one of the MEMTEST-functions in order to check if external memory.
 * The test-function is executed in 6 steps. In each step several rounds are
 * performed. In each round the function is finally executed for 'max' cells.
 * \param testId      ID of test that is displayed
 * \param iterations  Number of iterations the MEMTEST-function is executed
 *                    for each cell (specified by max)
 * \param max         Number of cells the MEMTEST-function is executed for in
 *                    each round.
 * \param errorsAsHex Set this to true, if you want hexadecimal output of errors
 * \param *testName   Name of test that is shown on the LCD
 * \param *test       MEMTEST-function that is to be executed
 * \return            1 if an error occurred, 0 if not
 */
char tt_performTest(uint8_t testId, uint8_t iterations, uint16_t max, bool errorsAsHex, char const* testName, MEMTEST((*test))) {
    // Test setup and some print outs.
    lcd_clear();
    lcd_writeProgString(PSTR("Testing "));
    lcd_writeProgString(testName);
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

    // Note: only use primes (including 1) (numeric black magic for optimal
    // diversity of tests
    Factor const factors[] = {1, 7, 13, 29, 101, 127};
    uint8_t const factorSize = sizeof(factors) / sizeof(factors[0]);
    uint32_t errors = 0;

    // Using different factors (step) in several rounds, perform the test once
    // for each memory cell (cell) up to a maximum.
    for (uint8_t step = 0; step < factorSize; step++) {
        lcd_clear();
        // Print progress
        lcd_writeDec(step + 1);
        lcd_writeChar('/');
        lcd_writeDec(factorSize);
        // Print number of errors
        lcd_writeChar(' ');
        tt_renderError(errors, errorsAsHex, false);

        for (uint8_t round = 0; round < iterations; round++) {
            MemAddr cell = 0;
            do {
                test(factors[step], round, cell, &errors);
            } while (cell++ != max);
        }
    }

    // Check on success and print out.
    lcd_clear();
    lcd_writeProgString(PSTR("Test #"));
    lcd_writeDec(testId);
    lcd_writeChar(':');
    lcd_line2();
    if (errors) {
        lcd_writeProgString(PSTR("FEHLER: "));
        tt_renderError(errors, errorsAsHex, true);
        delayMs(20 * DEFAULT_OUTPUT_DELAY);
    } else {
        lcd_writeProgString(PSTR("Successful!"));
    }
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    lcd_clear();

    // Clean up.
    MemAddr i = 0;
    do {
        extHeap->driver->write(i, 0);
    } while (i++ != MAX_ADDRESS);

    return !!errors; // The double negation will result in a boolean value of either 0 or 1
}


// Memtests

/*!
 * Test basic read/write functionality. It writes the byte i*f to the address i
 * and then checks if it was properly stored to the address by reading it again.
 */
MEMTEST(tt_doubleAccess) {
    extHeap->driver->write(cell, cell * f);
    *errors += extHeap->driver->read(cell) != (0xFF & (cell * f)); // Only compare saved bits of i*f
}

/*!
 * Tests if address is applied correctly to external memory. Here the
 * parameter cell is not indicating a certain address, but one address-
 * line that is to be checked.
 */
MEMTEST(tt_addressBits) {
    // For odd rounds we take the i-th last address-bit. For even rounds we take
    // the i-th. (cell is small enugh)
    uint8_t const addressBit = (round & 1) ? (ADDRESS_BITS - cell - 1) : cell;
    uint8_t const pattern = addressBit * f ? addressBit * f : 0xFF;
    // Static phase: This phase does not depend on the function parameters
    extHeap->driver->write(0, 0xFF);
    extHeap->driver->write(0xFF, 0);
    *errors += (extHeap->driver->read(0) != 0xFF);
    extHeap->driver->read(0xFF);
    extHeap->driver->write(0, 0);
    extHeap->driver->write(0xFF, 0xFF);
    *errors += (extHeap->driver->read(0) != 0x00);
    // Dynamic phase
    extHeap->driver->write(1ul << addressBit, pattern);
    *errors += (extHeap->driver->read(0) != 0);
}

/*!
 * In the first round (0) it writes a value to the cell. In the second round
 * (1) it reads the value of the same cell and checks if it is correct.
 * Checks if every memory cell is written exactly once.
 */
MEMTEST(tt_integrity) {
    MemValue val = cell * f;
    if (round == 0) {
        extHeap->driver->write(cell, val);
    } else if (round == 1) {
        if (extHeap->driver->read(cell) != (0xFF & val)) {
            (*errors)++;
        }
    }
}

/*!
 * Tests if everything of the above works. This is done by writing and reading
 * patterns from the cells.
 */
MEMTEST(tt_inversions) {
    MemValue const pat = cell * f ^ (uint8_t) - (round & 1);
    // Note f and 2 have no factor in common, so (i->f*i) is a permutation on Z/(2**13)Z
    MemAddr const p = cell * f;
    if (round) {
        // In the first (0) round nothing is read, for obvious reasons
        *errors += extHeap->driver->read(p) != pat;
    }
    extHeap->driver->write(p, pat ^ 0xFF);
}
