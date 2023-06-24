//-------------------------------------------------
//          TestTask: Multilevel-Feedback-Queue
//-------------------------------------------------

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>
#include "lcd.h"
#include "os_core.h"
#include "util.h"
#include "os_scheduler.h"
#include "os_memory.h"
#include "os_scheduling_strategies.h"
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

// Handy error message define
#define TEST_FAILED_AND_HALT(reason) \
    do ATOMIC { \
        TEST_FAILED(reason); \
        HALT; \
    } while (0)


Program program2;
Program program3;
Program program4;
Program program5;
Program program6;
Program program7;

#define MAX_STEPS 64
#define NUMBER_OF_QUEUES 4
uint8_t capture[MAX_STEPS];
uint8_t i = 0;

ISR(TIMER2_COMPA_vect);

// Array containing the correct output values for all four scheduling strategies.
const uint8_t scheduling[MAX_STEPS] PROGMEM  =  {
    1, 2, 3, 4, 3, 20, 4, 4, 2, 3, 3, 4, 5, 5, 70, 4, 7, 4, 40, 2, 2, 2, 2, 6, 5, 1, 1, 1, 1, 1, 1, 1,
    1, 4, 4, 4, 4,  4, 4, 4, 4, 1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

void printAndCheck(uint8_t page) {
    // Print captured schedule
    lcd_clear();
    for (i = (page * 32); i < (page * 32) + 32; i++) {
        // Print corresponding alphabetic character if yielded
        if (capture[i] >= 10) {
            lcd_writeChar('a' + (capture[i] / 10) - 1);
        } else {
            lcd_writeDec(capture[i]);
        }
    }

    // Check captured schedule
    for (i = (page * 32); i < (page * 32) + 32; i++) {
        if (capture[i] != pgm_read_byte(&scheduling[i])) {
            // Move cursor
            lcd_goto((i >= 16 + page * 32) + 1, (i % 16) + 1);
            // Show cursor without underlining the position
            lcd_command((LCD_SHOW_CURSOR & ~(1 << 1)) | LCD_DISPLAY_ON);
            HALT;
        }
        if (i == (page * 32) + 31) {
            _delay_ms(20 * DEFAULT_OUTPUT_DELAY);
            lcd_clear();
            lcd_writeProgString(PSTR("OK"));
        }
    }

    _delay_ms(20 * DEFAULT_OUTPUT_DELAY);
}


void performTest() {
    lcd_writeProgString(PSTR("Testing MLFQ"));
    os_setSchedulingStrategy(OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE);
    _delay_ms(20 * DEFAULT_OUTPUT_DELAY);

    // Perform scheduling test.
    // Save the id of the running process and call the scheduler.
    i = 0;
    uint8_t runtime = 0;
    while (i < MAX_STEPS) {
        runtime++;

        if (runtime == 10) {
            // Check if process 5 is really program 4
            if (os_getProcessSlot(4)->program != program4) {
                TEST_FAILED_AND_HALT("Program 4 not in slot 4");
            }
            os_kill(4);


            // Process with program id 1 is still running, so we should be able to spawn at least 6 more (wrt. idle process)
            uint8_t procs_num = MAX_NUMBER_OF_PROCESSES - 2;
            uint8_t procs[procs_num];
            Program *procs_program = program2;

            // Check if all processes were removed or are going to be (if that's implemented in os_exec)
            uint8_t pid = INVALID_PROCESS;
            for (uint8_t i = 0; i < procs_num; i++) {
                pid = os_exec(procs_program, DEFAULT_PRIORITY);
                if (pid == INVALID_PROCESS) {
                    TEST_FAILED_AND_HALT("Could not exec process");
                }
                procs[i] = pid;
            }

            // Check program id, number of processes in queues and process state
			uint8_t count_all = 0;
			uint8_t count_valid = 0;

			for (uint8_t i = 0; i < NUMBER_OF_QUEUES; i++){

				ProcessQueue* queue = MLFQ_getQueue(i);

				if (!pqueue_hasNext(queue))
					continue;

				ProcessID first = pqueue_getFirst(queue);


				do {
					ProcessID pid = pqueue_getFirst(queue);
					Process* proc = os_getProcessSlot(pid);

					if (
                        (proc->program == procs_program && proc->state == OS_PS_READY)
                        || (proc->program == os_getProcessSlot(os_getCurrentProc())->program)
                        || (proc->program == NULL)
                    ) {
						count_valid++;
					}

					count_all++;
					pqueue_dropFirst(queue);
					pqueue_append(queue, pid);
				} while (pqueue_getFirst(queue) != first);

			}

            if (count_all != count_valid || count_valid < procs_num + 1) {
                TEST_FAILED_AND_HALT("Queue incorrect");
            }

            bool killed = false;
            for (uint8_t i = 0; i < procs_num; i++) {
                killed = os_kill(procs[i]);
                if (!killed) {
                    TEST_FAILED_AND_HALT("Could not kill process");
                }
            }

        }

        capture[i++] = 1;
        TIMER2_COMPA_vect();
    }

    printAndCheck(0);
    printAndCheck(1);

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


REGISTER_AUTOSTART(program1)
void program1(void) {
    // Disable scheduler-timer
    cbi(TCCR2B, CS22);
    cbi(TCCR2B, CS21);
    cbi(TCCR2B, CS20);

    os_getProcessSlot(os_getCurrentProc())->priority = 2;

    os_exec(program2, 0b11000000);
    os_exec(program3, 0b10000000);

    // Start test cycle
    performTest();
}

void program2(void) {
    uint8_t runtime = 0;

    // Perform scheduling test
    while (i < MAX_STEPS) {
        runtime++;
        //exec
        // -

        //yield
        if (runtime == 2) {
            capture[i++] = 20;
            os_yield();
            runtime++;
        }

        capture[i++] = 2;

        //termination
        if (runtime == 7) {
            break;
        }
        TIMER2_COMPA_vect();
    }
}

void program3(void) {
    uint8_t runtime = 0;

    // Perform scheduling test
    while (i < MAX_STEPS) {
        runtime++;
        //exec
        if (runtime == 1) {
            os_exec(program4, 0b11000000);
        }

        //yield
        // -

        capture[i++] = 3;

        //termination
        if (runtime == 4) {
            break;
        }
        TIMER2_COMPA_vect();
    }
}



void program4(void) {
    uint8_t runtime = 0;

    // Perform scheduling test
    while (i < MAX_STEPS) {
        runtime++;
        //exec
        if (runtime == 4) {
            os_exec(program5, 0b10000000);
            os_exec(program6, 0b01000000);
        }

        //yield
        if (runtime == 7) {
            capture[i++] = 40;
            os_yield();
            runtime++;
        }

        capture[i++] = 4;

        //termination
        // -

        TIMER2_COMPA_vect();
    }
}

void program5(void) {
    uint8_t runtime = 0;

    // Perform scheduling test
    while (i < MAX_STEPS) {
        runtime++;
        //exec
        if (runtime == 1) {
            os_exec(program7, 0b10000000);
        }

        //yield
        // -

        capture[i++] = 5;

        //termination
        if (runtime == 3) {
            break;
        }
        TIMER2_COMPA_vect();
    }
}

void program6(void) {
    uint8_t runtime = 0;

    // Perform scheduling test
    while (i < MAX_STEPS) {
        runtime++;
        //exec
        // -

        //yield
        // -

        capture[i++] = 6;

        //termination
        if (runtime == 1) {
            break;
        }
        TIMER2_COMPA_vect();
    }
}

void program7(void) {
    uint8_t runtime = 0;

    // Perform scheduling test
    while (i < MAX_STEPS) {
        runtime++;
        //exec
        // -

        //yield
        if (runtime == 1) {
            capture[i++] = 70;
            os_yield();
            runtime++;
        }

        capture[i++] = 7;

        //termination
        if (runtime == 2) {
            break;
        }
        TIMER2_COMPA_vect();
    }
}
