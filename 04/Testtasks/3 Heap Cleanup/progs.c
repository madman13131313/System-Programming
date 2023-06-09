//-------------------------------------------------
//          TestTask: Heap Cleanup
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

#if VERSUCH < 4
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
//! This flag defines if phase 1 of this test is executed
#define PHASE_1                 1
//! This flag defines if phase 2 of this test is executed
#define PHASE_2                 1
//! This flag defines if phase 3 of this test is executed
#define PHASE_3                 1
//! This flag defines if phase 4 of this test is executed
#define PHASE_4                 1
//! Number of heap-drivers that are to be tested
#define DRIVERS                 os_getHeapListLength()
//! Test if all heaps are used. Should be 1 for V3 and 2 in V4
#define HEAP_COUNT              2
//! The default memory allocation strategy for phase 3
#define DEFAULT_ALLOC_STRATEGY  OS_MEM_NEXT
//! This flag decides if the first fit allocation strategy is tested
#define CHECK_FIRST             1
//! This flag decides if the next fit allocation strategy is tested
#define CHECK_NEXT              1
//! This flag decides if the best fit allocation strategy is tested
#define CHECK_BEST              1
//! This flag decides if the worst fit allocation strategy is tested
#define CHECK_WORST             1
//! Number of runs of swarm-allocations that are to be performed. Usually 400
#define RUNS                    400
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
    do { ATOMIC { \
        TEST_FAILED(reason); \
        HALT; \
    } } while (0)

#define TEST_FAILED_HEAP_AND_HALT(reason, heap) \
    do { ATOMIC { \
        TEST_FAILED(reason " "); \
        lcd_writeString((heap)->name); \
        HALT; \
    } } while (0)

//! ProcessID of small allocation process
volatile ProcessID allocProc = 0;
//! This variable is set to 1 when the small allocation process is finished
volatile uint8_t   allocProcReadyToDie = 0;
//! This variable is set to true when the test program finishes
bool terminated = false;

/*!
 * Swarm allocation program. This program tries to allocate all bytes in
 * every heap. This is done by allocating less and less memory in each step.
 * Due to this other processes will be able to allocate memory, too. This
 * yields a very fragmented memory.
 */
void swarm_alloc_program(void) {
    for (uint8_t i = 0; i < DRIVERS; i++) {
        Heap* heap = os_lookupHeap(i);
        // The number 4 is a heuristic. We expect the ram not to be that
        // fragmented.
        uint16_t max = 1024 / (MAX_NUMBER_OF_PROCESSES - 3) / 4;
        while (max) {
            // Allocate less and less memory with every step
            uint16_t const sz = (max + 1) / 2;
            max -= sz;
            os_malloc(heap, sz);
        }
    }
}

/*!
 * Small allocation program that tries to allocate 2 times 4 bytes on every
 * heap. After that it signals that it is finished and it waits in an infinite
 * loop until it is externally killed.
 */
void small_alloc_program(void) {
    allocProc = os_getCurrentProc();
    for (uint8_t i = 0; i < DRIVERS; i++) {
        os_malloc(os_lookupHeap(i), 4);
        os_malloc(os_lookupHeap(i), 4);
    }
    allocProcReadyToDie = 1;
    HALT;
}

/*!
 * Starts the small allocation process and waits until it finished its
 * allocations.
 */
void hc_startSmallAllocator(void) {
    allocProcReadyToDie = 0;
    os_exec(small_alloc_program, DEFAULT_PRIORITY);
    while (!allocProcReadyToDie) {
        //wait until the allocation process allocated its memory
    }
}

/*!
 * Kills the small allocation process.
 */
void hc_killSmallAllocator(void) {
    os_kill(allocProc);
}

/*!
 * Checks if a free process slot exists
 */
static bool hc_hasFreeProcSlot(void) {
    for (ProcessID pid = 0; pid < MAX_NUMBER_OF_PROCESSES; pid++) {
        if (os_getProcessSlot(pid)->state == OS_PS_UNUSED) return true;
    }
    return false;
}

/*!
 * Starts as many processes of the swarm-allocation program as possible.
 */
void hc_startAllocatorSwarm(void) {
    while (hc_hasFreeProcSlot()) {
        os_exec(swarm_alloc_program, DEFAULT_PRIORITY);
    }
}

/*!
 * Waits until all started swarm-processes are dead.
 */
void hc_waitForSwarmKill(void) {
    uint8_t num;
    do {
        num = 0;
        for (ProcessID pid = 1; pid < MAX_NUMBER_OF_PROCESSES; pid++) {
            if (os_getProcessSlot(pid)->state != OS_PS_UNUSED) num++;
        }
    } while (num > 1); // 1 main test-process
}

/*!
 * Waits until a process-slot is free.
 */
void hc_waitForFreeProcSlot(void) {
    while (!hc_hasFreeProcSlot()) continue;
}

//! Main test routine. Executes the single phases.
REGISTER_AUTOSTART(main_program)
void main_program(void) {
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

    // Check if heaps are defined correctly
    if (DRIVERS < HEAP_COUNT) {
        TEST_FAILED_AND_HALT("Missing heap");
    }
    for (uint8_t i = 0; i < DRIVERS; i++) {
        for (uint8_t j = i + 1; j < DRIVERS; j++) {
            if (os_lookupHeap(i) == os_lookupHeap(j)) {
                TEST_FAILED_AND_HALT("Missing heap");
            }
        }
    }

// 1. Phase: Test if you can over-allocate memory
#if PHASE_1
    lcd_clear();
    lcd_writeProgString(PSTR("Phase 1:"));
    lcd_line2();
    lcd_writeProgString(PSTR("Overalloc     "));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

    // First allocate a small amount of memory on each heap. This should make
    // it impossible to allocate the whole use size in the next step
    hc_startSmallAllocator();
    // Then we try to alloc all the memory on one heap. If it works, the
    // student's solution is faulty.
    for (uint8_t i = 0; i < DRIVERS; i++) {
        Heap*     heap  = os_lookupHeap(i);
        uint16_t  total = os_getUseSize(heap);
        if (os_malloc(heap, total)) {
            TEST_FAILED_HEAP_AND_HALT("Overalloc fail", heap);
        }
    }
    hc_killSmallAllocator();

    lcd_writeProgString(PSTR("OK"));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
#endif

// 2. Phase: Settings test: Check if heap is set up correctly
#if PHASE_2
    lcd_clear();
    lcd_writeProgString(PSTR("Phase 2:"));
    lcd_line2();
    lcd_writeProgString(PSTR("Heap settings "));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

    // This address is certainly in the stacks.
    MemValue* startOfStacks = (uint8_t*)((AVR_MEMORY_SRAM / 2) + AVR_SRAM_START);

    /*
     * If we allocate all the memory and write in it, we should not be able to
     * write into that stack memory. If we can, the internal heap is initialized
     * with wrong settings
     */
    os_setAllocationStrategy(intHeap, OS_MEM_FIRST);
    MemAddr  hugeChunk = os_malloc(intHeap, os_getUseSize(intHeap));
    if (!hugeChunk) {
        TEST_FAILED_HEAP_AND_HALT("f:Huge alloc fail", intHeap);
    }

    /*
     * We will now write a pattern into RAM and check if they change the
     * contents of the stack. To check that, we check the top of the stack of
     * process 8. This can be done because there is no process 8
     */
    *startOfStacks = 0x00;
    for (MemAddr currentAddress = hugeChunk; currentAddress < hugeChunk + os_getUseSize(intHeap); currentAddress++) {
        intHeap->driver->write(currentAddress, 0xFF);
    }

    if (*startOfStacks != 0x00) {
        TEST_FAILED_AND_HALT("Internal heap too large");
    }

    // Clean up
    os_free(intHeap, hugeChunk);
    lcd_goto(2, 15);
    lcd_writeProgString(PSTR("OK"));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

#endif

// 3. Phase: Check if we can allocate the whole use-area after killing a RAM-hogger
#if PHASE_3
    lcd_clear();
    lcd_writeProgString(PSTR("Phase 3:"));
    lcd_line2();
    lcd_writeProgString(PSTR("Huge alloc    "));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

    for (uint8_t i = 0; i < DRIVERS; i++) {
        Heap*   heap = os_lookupHeap(i);
        MemAddr hugeChunk;

        /*
         * For each allocation strategy the following two steps are executed
         * 1. Make a RAM-hogger and kill it, such that the heap-cleaner should
         *    have freed its memory
         * 2. Test if this is correct with a certain strategy
         */

        lcd_goto(1, 11);
        lcd_writeDec(i);
        lcd_writeProgString(PSTR(":    "));
        lcd_goto(1, 13);
#if CHECK_BEST
        lcd_writeChar('b');
        hc_startSmallAllocator();
        hc_killSmallAllocator();
        os_setAllocationStrategy(heap, OS_MEM_BEST);
        hugeChunk = os_malloc(heap, os_getUseSize(heap));
        if (!hugeChunk) {
            TEST_FAILED_HEAP_AND_HALT("b:Huge alloc fail", heap);
        } else {
            os_free(heap, hugeChunk);
        }
#endif

#if CHECK_WORST
        lcd_writeChar('w');
        hc_startSmallAllocator();
        hc_killSmallAllocator();
        os_setAllocationStrategy(heap, OS_MEM_WORST);
        hugeChunk = os_malloc(heap, os_getUseSize(heap));
        if (!hugeChunk) {
            TEST_FAILED_HEAP_AND_HALT("w:Huge alloc fail", heap);
        } else {
            os_free(heap, hugeChunk);
        }
#endif

#if CHECK_FIRST == 1
        lcd_writeChar('f');
        hc_startSmallAllocator();
        hc_killSmallAllocator();
        os_setAllocationStrategy(heap, OS_MEM_FIRST);
        hugeChunk = os_malloc(heap, os_getUseSize(heap));
        if (!hugeChunk) {
            TEST_FAILED_HEAP_AND_HALT("f:Huge alloc fail", heap);
        } else {
            os_free(heap, hugeChunk);
        }
#endif

#if CHECK_NEXT == 1
        lcd_writeChar('n');
        hc_startSmallAllocator();
        hc_killSmallAllocator();
        os_setAllocationStrategy(heap, OS_MEM_NEXT);
        hugeChunk = os_malloc(heap, os_getUseSize(heap));
        if (!hugeChunk) {
            TEST_FAILED_HEAP_AND_HALT("n:Huge alloc fail", heap);
        } else {
            os_free(heap, hugeChunk);
        }
#endif
    }

    lcd_goto(2, 15);
    lcd_writeProgString(PSTR("OK"));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
#endif

// 4. Phase: Main test: Check if heap is cleaned up
#if PHASE_4
    lcd_clear();
    lcd_writeProgString(PSTR("Phase 4:"));
    lcd_line2();
    lcd_writeProgString(PSTR("Heap Cleanup  "));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

    // We want to use the default strategy on every heap. After phase 2 we
    // cannot know which strategy was last used.
    for (uint8_t i = 0; i < DRIVERS; i++) {
        os_setAllocationStrategy(os_lookupHeap(i), DEFAULT_ALLOC_STRATEGY);
    }

    /*
     * In this phase a swarm of allocation-process is started. These processes
     * terminate. Thus heap-cleanup should be performed. Whenever a process
     * terminates, a new one is started in his stead. Every 50 steps we will
     * check, if the cleanup was successful.
     */
    hc_startAllocatorSwarm();

    uint16_t const totalRuns = RUNS;

    for (uint16_t runs = 0; runs < totalRuns; runs++) {
        // Every run we start a new allocator and give it some time to allocate
        // memory
        hc_waitForFreeProcSlot();

        os_exec(swarm_alloc_program, DEFAULT_PRIORITY);
        delayMs(DEFAULT_OUTPUT_DELAY);


        lcd_drawBar((100 * runs) / totalRuns);
        lcd_goto(2, 6);
        lcd_writeDec(runs + 1);
        lcd_writeChar('/');
        lcd_writeDec(totalRuns);

        /*
         * Every 50 steps we test if the memory get freed correctly after a
         * kill. For that, we wait for every big allocator to get killed and
         * then check if the whole memory is free again
         */
        if ((runs + 1) % 50 == 0) {
            hc_waitForSwarmKill();
            for (uint8_t i = 0; i < DRIVERS; i++) {
                Heap*    heap      = os_lookupHeap(i);
                MemAddr  hugeChunk = os_malloc(heap, os_getUseSize(heap));
                if (!hugeChunk) {
                    // We could not allocate all the memory on that heap, so there was
                    // a leak
                    TEST_FAILED_HEAP_AND_HALT("Heap not clean:", heap);
                } else {
                    os_free(heap, hugeChunk);
                }
            }
            // Since we killed the whole swarm, we have to revive it again before
            // we jump back to our loop
            hc_startAllocatorSwarm();
        }
    }
    hc_waitForSwarmKill();

    lcd_clear();
    lcd_writeProgString(PSTR("Phase 4:"));
    lcd_line2();
    lcd_writeProgString(PSTR("Heap Cleanup  "));
    lcd_writeProgString(PSTR("OK"));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);

#endif
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
 *	Timeout ISR
 */
ISR(TIMER1_COMPA_vect) {
    // counts seconds
    static int16_t timeout_counter = 0;
    if (++timeout_counter >= 600 && !terminated) {
        TEST_FAILED_AND_HALT("Test timed out after 10 minutes!");
    }
}
