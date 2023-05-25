//-------------------------------------------------
//          TestTask: Termination
//-------------------------------------------------

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_core.h"
#include "os_scheduler.h"
#include "os_memory.h"
#include "os_taskman.h"
#include "os_input.h"

#if VERSUCH < 3
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
#define PHASE_1    1
#define PHASE_2    1
//! Defines how often each strategy is tested
#define RUNS       2
//! Defines after how many spawns the strategy is changed
#define MAX_SPAWNS 200
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

//! Global spawn counter
uint16_t volatile tt_spawns = 2;

//! Starts one process and increases the spawm-counter.
void tt_test(Program prog);
//! Prints an error message on the LCD and halts.
void tt_throwError(char const* str);
//! converts a name of a scheduling strategy to a 4-digit initial
char const* tt_stratToName(SchedulingStrategy strat);
//! Tests if every strategy is able to spawn MAX_SPAWNS processes.
void tt_testSpawns(void);
//! Tests if killing a process will leave foreign critical sections
void tt_testForeignCriticalSections(void);

//! This programs includes the main procedure
REGISTER_AUTOSTART(program1)
void program1(void) {

#if PHASE_1

    if (MAX_NUMBER_OF_PROCESSES < 5) {
        tt_throwError(PSTR("Max.Num.Proc < 5"));
    }

    lcd_clear();
    lcd_writeProgString(PSTR("Phase 1: Foreign Crit. Sec."));
    delayMs(8 * DEFAULT_OUTPUT_DELAY);

    tt_testForeignCriticalSections();

#endif
#if PHASE_2

    lcd_clear();
    lcd_writeProgString(PSTR("Phase 2: Spawns"));
    delayMs(8 * DEFAULT_OUTPUT_DELAY);

    for (uint8_t run = 0; run < RUNS; run++) {
        tt_testSpawns();
    }

    // At this point every scheduling strategy was able to
    // spawn MAX_SPAWNS processes

#endif

	// kill everything else, show end and let idle take over
    os_enterCriticalSection();
	for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++){
		if (i != os_getCurrentProc()){
			os_kill(i);
		}
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

    os_leaveCriticalSection();
}

// We need 3 slots
void program2(void) {
    tt_spawns++;
    while (1) {
        delayMs(DEFAULT_OUTPUT_DELAY);
    }
}


// The following programs execute a new process of themselves and then die
REGISTER_AUTOSTART(program3)
void program3(void) {
    tt_test(program3);
}

#if MAX_NUMBER_OF_PROCESSES > 5
REGISTER_AUTOSTART(program4)
void program4(void) {
    tt_test(program4);
}
#endif

#if MAX_NUMBER_OF_PROCESSES > 6
REGISTER_AUTOSTART(program5)
void program5(void) {
    tt_test(program5);
}
#endif

#if MAX_NUMBER_OF_PROCESSES > 7
REGISTER_AUTOSTART(program6)
void program6(void) {
    tt_test(program6);
}
#endif

/*!
 *  Tests whether the pid passed is invalid.
 *  If so, outputs a message and halts the program.
 *
 */
void tt_verifyPID(ProcessID pid) {
	if (pid < 1 || pid > 7) {
        ATOMIC {
            TEST_FAILED("os_exec failed: ");
            lcd_writeDec(pid);
		    HALT;
        }
	}
}

/*!
 *  Tests if killing another process will leave foreign
 *  critical sections.
 */
void tt_testForeignCriticalSections(void) {
    os_enterCriticalSection();

    // Start a process and immediately kill it
    ProcessID pid = os_exec(program2, DEFAULT_PRIORITY);
    tt_verifyPID(pid);
    os_kill(pid);

    if (TIMSK2 & (1 << OCIE2A)) {
        ATOMIC {
            // Scheduler activated by os_kill()
            TEST_FAILED("Left crit. sec.");
            HALT;
        }
    }

    lcd_goto(2, 14);
    lcd_writeProgString(PSTR("OK"));
    delayMs(8 * DEFAULT_OUTPUT_DELAY);


    os_leaveCriticalSection();
}

/*!
 * Starts one process and increases the spawn-counter.
 * \param  prog Program to start
 */
void tt_test(Program prog) {
    delayMs(DEFAULT_OUTPUT_DELAY);
    /*
     * The critical section makes sure, that we do not start a fork-bomb, e.g.
     * force termination of old process before first runtime for new process.
     * The dispatcher should clean this up.
     */
    os_enterCriticalSection();
    os_exec(prog, DEFAULT_PRIORITY);
    tt_spawns++;
}

/*!
 * Converts a name of a scheduling strategy to a 4-digit initials.
 * \param  strat Strategy of which you want to know the initials
 * \return       4-digit code for that strategy
 */
char const* tt_stratToName(SchedulingStrategy strat) {
    switch (strat) {
        case OS_SS_EVEN:
            return PSTR("EVEN");
        case OS_SS_INACTIVE_AGING:
            return PSTR("INAG");
        case OS_SS_RANDOM:
            return PSTR("RAND");
        case OS_SS_ROUND_ROBIN:
            return PSTR("RORO");
        case OS_SS_RUN_TO_COMPLETION:
            return PSTR(" RTC");
        default:
            return PSTR("NULL");
    }
}

static uint8_t tt_getNumberOfActiveProcs(void) {
    uint8_t num = 0;
    for (ProcessID pid = 0; pid < MAX_NUMBER_OF_PROCESSES; pid++) {
        if (os_getProcessSlot(pid)->state != OS_PS_UNUSED) num++;
    }
    return num;
}

/*!
 * Tests if every strategy is able to spawn MAX_SPAWNS processes.
 */
void tt_testSpawns(void) {
    // In this array you can select which strategies you want to test
    // Remove (or comment out) the strategies you don't want to test
    SchedulingStrategy strategies[] = {
        OS_SS_EVEN,
        OS_SS_RANDOM,
        OS_SS_ROUND_ROBIN,
        OS_SS_INACTIVE_AGING
    };
    uint8_t const numOfStrategies = sizeof(strategies) / sizeof(strategies[0]);
    uint8_t currentStrat = strategies[0];

    // This variable holds the process ID of the currently
    // running process of program 2
    ProcessID prog2ProcID = 0;

    for (uint8_t c = 0; c < numOfStrategies; c++) {
        currentStrat = strategies[c];
        os_setSchedulingStrategy(currentStrat);

        os_enterCriticalSection();
        tt_spawns = 0;
        os_leaveCriticalSection();

        lcd_clear();

        uint8_t finished = 0;

        // This loop runs until MAX_SPAWNS processes were spawned with the
        // selected strategy
        while (1) {
            lcd_line1();
            lcd_writeProgString(PSTR("Act.P.: "));
            lcd_writeDec(tt_getNumberOfActiveProcs());
            lcd_writeChar('/');
            lcd_writeDec(MAX_NUMBER_OF_PROCESSES);
            lcd_writeChar(' ');
            lcd_writeProgString(tt_stratToName(currentStrat));
            lcd_line2();
            lcd_writeProgString(PSTR("Spawns: "));
            lcd_writeDec(tt_spawns);

            /*
             * Testing os_kill: If it works, a process of program 2 will be killed.
             * In the next iteration program 2 will be started again. If it does not
             * work, the process remains active and in the next iteration another
             * process of program 2 will be executed. Since program 2 does not
             * terminate on its own, spawning will cease after a few tries.
             */
            if (prog2ProcID) {
                os_kill(prog2ProcID);
                prog2ProcID = 0;
            } else {
                prog2ProcID = os_exec(program2, 1);
                tt_verifyPID(prog2ProcID);
            }

            // We want to break after a fixed number of spawns. Accessing a shared
            // variable must be safe though
            os_enterCriticalSection();
            finished = tt_spawns >= MAX_SPAWNS ? 1 : 0;
            os_leaveCriticalSection();

            if (finished) {
                // Make sure we kill the current prog2 instance. prog2ProcID will be set
                // to 0 again in the next run so we could end up with multiple prog2 instances.
                if (prog2ProcID) {
                    os_kill(prog2ProcID);
                }
                break; // out of spawn-counting loop
            }
        }
    }
}
