//-------------------------------------------------
//          TestTask: Yield
//-------------------------------------------------

#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "lcd.h"
#include "util.h"
#include "os_core.h"
#include "os_scheduler.h"
#include "os_input.h"
#include <avr/interrupt.h>

#if VERSUCH < 5
    #warning "Please fix the VERSUCH-define"
#endif

//---- Adjust here what to test -------------------
#define TEST_EVEN  1       // OS_SS_EVEN
#define TEST_MLFQ  1       // OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE
#define TEST_RAND  1       // OS_SS_RANDOM
#define TEST_INAC  1       // OS_SS_INACTIVE_AGING
#define TEST_RR    1       // OS_SS_ROUND_ROBIN
#define TEST_RCOMP 1       // OS_SS_RUN_TO_COMPLETION
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

#define MAX_TEST_ITERATIONS_PER_STRATEGY (9)
#define ITERATIONCOUNT_DIVISION_BASE (2)
#define TEST_TIMEOUT_MINUTES (5)
#define WAITTIME_MULTIPLICATOR_PHASE_4 (10)
#define GLOBAL_INTERRUPT_ENABLE_BIT (7)

#define STRATEGYCOUNT (sizeof(strategies) / sizeof(struct Strategy))

static char PROGMEM const  evenStr[] = "EVEN";
static char PROGMEM const  mlfqStr[] = "MLFQ";
static char PROGMEM const  randStr[] = "RAND";
static char PROGMEM const  inacStr[] = "INACTIVE AGING";
static char PROGMEM const    rrStr[] = "ROUND ROBIN";
static char PROGMEM const  runcomp[] = "RUN TO COMP.";

typedef unsigned long volatile Iterations;

volatile uint8_t giebSetInOtherProcess = 0;
Time yieldSpeed = 0;
Iterations nonYieldings = 0;
Iterations yieldings = 0;
Iterations nonYieldingsInlog = 0;
Iterations yieldingsInlog = 0;
SchedulingStrategy backupSchedulingStrategy = OS_SS_EVEN;

struct Strategy {
    char const* const strategyName;
    uint8_t activated;
    uint16_t lowestAcceptableQuotient;
    uint16_t highestAcceptableQuotient;
} strategies[] = {
    [OS_SS_EVEN]                       = {evenStr, TEST_EVEN , 2, 255},
    [OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE] = {mlfqStr, TEST_MLFQ , 2, 255},
    [OS_SS_RANDOM]                     = {randStr, TEST_RAND , 2, 255},
    [OS_SS_INACTIVE_AGING]             = {inacStr, TEST_INAC , 2, 255},
    [OS_SS_ROUND_ROBIN]                = {rrStr  , TEST_RR   , 2, 255},
    [OS_SS_RUN_TO_COMPLETION]          = {runcomp, TEST_RCOMP, 1,   1},
};

Program program_sreg_restore_helper;
Program program_yield_speed_main;
Program program_quotient_main;
Program program_edgecase;
Program program_run_to_completion_main;
Program program_run_to_completion_helper;

void program_yield_speed_helper(void) {
    while (1);
}

unsigned long benchmarkYield(uint16_t samplecount) {
    uint8_t dummy = os_exec(program_yield_speed_helper, DEFAULT_PRIORITY);
    Time time1 = os_systemTime_precise();
    for (long it = 0; it < samplecount; it++) {
        os_yield();
    }
    Time time2 = os_systemTime_precise();
    os_kill(dummy);
    return (time2 - time1) * 1000ul / samplecount;
}

unsigned logBase(unsigned long base, unsigned long value) {
    if (base <= 1) {
        return 0;
    }
    unsigned result = 0;
    for (; value; value /= base) {
        result++;
    }
    return result;
}

unsigned logBaseStandard(unsigned long value) {
    return logBase(ITERATIONCOUNT_DIVISION_BASE, value);
}

void updateGlobalIterationsLogValues() {
    yieldingsInlog = logBaseStandard(yieldings);
    nonYieldingsInlog = logBaseStandard(nonYieldings);
}

unsigned long roundClosestUp(unsigned long dividend, unsigned long divisor) {
    return (dividend + (divisor - 1)) / divisor;
}

void dialogTestPassed() {
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

void displayInversionSafeQuotient() {
    if (nonYieldings >= yieldings) {
        lcd_writeDec(roundClosestUp(nonYieldings, yieldings));
    } else {
        lcd_writeChar('1');
        lcd_writeChar('/');
        lcd_writeDec(roundClosestUp(yieldings, nonYieldings));
    }
}

void displayQuotient(char const* strategyName) {
    lcd_line1();
    lcd_writeProgString(strategyName);
    lcd_line2();
    lcd_writeDec(ITERATIONCOUNT_DIVISION_BASE);
    lcd_writeChar('^');
    lcd_writeDec(nonYieldingsInlog);
    lcd_writeChar('/');
    lcd_writeDec(ITERATIONCOUNT_DIVISION_BASE);
    lcd_writeChar('^');
    lcd_writeDec(yieldingsInlog);
    lcd_goto(2, 10);
    lcd_writeChar('=');
    lcd_goto(2, 12);
    lcd_writeProgString(PSTR("     "));
    lcd_goto(2, 12);
    displayInversionSafeQuotient();
}

void displayProgBanner(char const* phasetext) {
    lcd_clear();
    lcd_writeProgString(phasetext);
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
}

void displayProgFooter(char const* footertext) {
    lcd_line2();
    lcd_writeProgString(footertext);
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
}

void displayFailureFooter(char const* footertext) {
    displayProgFooter(footertext);
    HALT;
}

void displayOKFooter() {
    displayProgFooter(PSTR("OK"));
}

void enterCriticalSections(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        os_enterCriticalSection();
    }
}

void leaveCriticalSections(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        os_leaveCriticalSection();
    }
}

void yieldLoop(bool shouldYield, Iterations* iterations) {
    while (1) {
        if (shouldYield) {
            enterCriticalSections(128);
            os_yield();
            leaveCriticalSections(128);
        } else {
            enterCriticalSections(128);
            delayMs(yieldSpeed);
            leaveCriticalSections(128);
        }
        ++*iterations;
    }
}

void enforceTimeLimit() {
    if (os_systemTime_coarse() >= TIME_M_TO_MS(TEST_TIMEOUT_MINUTES)) {
        TEST_FAILED("TIMEOUT");
        HALT;
    }
}

void enforceQuotientLimit(struct Strategy* strategy) {
    if (nonYieldings < yieldings) {
        TEST_FAILED("INVERSION");
        displayFailureFooter(strategy->strategyName);
    }
    uint16_t quotient = roundClosestUp(nonYieldings, yieldings);
    if (strategy->lowestAcceptableQuotient > quotient) {
        TEST_FAILED("Q-UNDER");
        displayFailureFooter(strategy->strategyName);
    }
    if (strategy->highestAcceptableQuotient < quotient) {
        TEST_FAILED("Q-OVER");
        displayFailureFooter(strategy->strategyName);
    }
}

void program_quotient_non_yielding(void) {
    yieldLoop(false, &nonYieldings);
}

void program_quotient_yielding(void) {
    yieldLoop(true, &yieldings);
}

void quotientTest(struct Strategy* strategyToTest) {
    nonYieldings = 0;
    yieldings = 0;
    uint8_t PID_yielding = os_exec(program_quotient_non_yielding, DEFAULT_PRIORITY);
    uint8_t PID_nonYielding = os_exec(program_quotient_yielding, DEFAULT_PRIORITY);
    lcd_clear();
    do {
        enforceTimeLimit();
        updateGlobalIterationsLogValues();
        displayQuotient(strategyToTest->strategyName);
    } while (yieldingsInlog < MAX_TEST_ITERATIONS_PER_STRATEGY);
    enforceQuotientLimit(strategyToTest);
    os_kill(PID_yielding);
    os_kill(PID_nonYielding);
}

void replacementIdleProcessFunction(void){
    ATOMIC {
        TEST_FAILED("IDLE HIT");
        displayFailureFooter(strategies[os_getSchedulingStrategy()].strategyName);
    }
}

void backupCurrentSchedulingStrategy() {
    backupSchedulingStrategy = os_getSchedulingStrategy();
}

void restorePreviousSchedulingStrategy() {
    os_setSchedulingStrategy(backupSchedulingStrategy);
}

void finishTesttask() {
    enforceTimeLimit();
    dialogTestPassed();
}

REGISTER_AUTOSTART(program_sreg_restore_main)
void program_sreg_restore_main(void) {
    displayProgBanner(PSTR("1:CHECK SREG    "));

    cli();
    os_exec(program_sreg_restore_helper, DEFAULT_PRIORITY);
    while (!giebSetInOtherProcess) {
        os_yield();
    }

    if (gbi(SREG, GLOBAL_INTERRUPT_ENABLE_BIT) != 0) {
        TEST_FAILED("SREG");
        HALT;
    }
    displayOKFooter();

    os_exec(program_yield_speed_main, DEFAULT_PRIORITY);
}

void program_sreg_restore_helper(void) {
    sei();
    giebSetInOtherProcess = 1;
}

void program_yield_speed_main(void) {
    displayProgBanner(PSTR("2:CHECK Speed"));

    backupCurrentSchedulingStrategy();
    os_setSchedulingStrategy(OS_SS_EVEN);
    yieldSpeed = benchmarkYield(200);

    lcd_line2();
    lcd_writeDec(yieldSpeed);
    lcd_writeProgString(PSTR(" us"));
    delayMs(10 * DEFAULT_OUTPUT_DELAY);
    yieldSpeed = yieldSpeed / 1000;

    restorePreviousSchedulingStrategy();

    os_exec(program_quotient_main, DEFAULT_PRIORITY);
}

void program_quotient_main(void) {
    displayProgBanner(PSTR("3:CHECK Quotient"));

    backupCurrentSchedulingStrategy();
    for (uint8_t i = 0; i < STRATEGYCOUNT; i++) {
        //Run to Completion will be skipped as
        //the non yielding helper process will be stuck forever
        //therefor not "yielding" any meaningful results in this phase
        if (strategies[i].activated && i != OS_SS_RUN_TO_COMPLETION) {
            os_setSchedulingStrategy(i);
            quotientTest(&strategies[i]);
        }
    }
    restorePreviousSchedulingStrategy();

    displayProgBanner(PSTR("3:CHECK Quotient"));
    displayOKFooter();
    os_exec(program_edgecase, DEFAULT_PRIORITY);
}

void program_edgecase(void) {
    displayProgBanner(PSTR("4:CHECK Edgecase"));

    // get the idle process and replace its program pointer
    // this works because the process has not yet been dispatched
    Process *idle = os_getProcessSlot(0);
    Program *idleProgram = idle->program;
    idle->program = replacementIdleProcessFunction;
    backupCurrentSchedulingStrategy();
    for (uint8_t i = 0; i < STRATEGYCOUNT; i++) {
        if (strategies[i].activated) {
            os_setSchedulingStrategy(i);
            os_yield();
        }
    }
    idle->program = idleProgram;
    restorePreviousSchedulingStrategy();

    displayOKFooter();

    if (TEST_RCOMP) {
        os_exec(program_run_to_completion_main, DEFAULT_PRIORITY);
    } else {
        finishTesttask();  
    }
}

void program_run_to_completion_main(void) {
    displayProgBanner(PSTR("5:CHECK RCOMP"));

    os_setSchedulingStrategy(OS_SS_RUN_TO_COMPLETION);
    Time helperStartime = os_systemTime_precise();
    os_exec(program_run_to_completion_helper, DEFAULT_PRIORITY);
    os_yield();
    Time returnToTestTime = os_systemTime_precise();

    Time timeOutsideTestcode = (returnToTestTime-helperStartime);
    if (timeOutsideTestcode < (WAITTIME_MULTIPLICATOR_PHASE_4 * yieldSpeed)) {
        TEST_FAILED("RCOMP-TIME");
        HALT;
    }

    displayOKFooter();
    finishTesttask();
}

void program_run_to_completion_helper(void) {
    delayMs(WAITTIME_MULTIPLICATOR_PHASE_4 * yieldSpeed);
}
