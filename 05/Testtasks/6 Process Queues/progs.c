//-------------------------------------------------
//          TestTask: Process Queues
//-------------------------------------------------

#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "lcd.h"
#include "os_scheduling_strategies.h"
#include "os_scheduler.h"
#include "string.h"
#include "os_input.h"
#include "os_process.h"
#include "os_core.h"

#if VERSUCH < 5
#warning "Please fix the VERSUCH-define"
#endif



//---- Adjust here what to test -------------------
#define PHASE_1_INIT		1
#define PHASE_2_SIMPLE		1
#define PHASE_3_CONSISTENCY 1
#define PHASE_4_REMOVE		1
#define PHASE_5_MLFQ		1

// Draw the process queues after each step
// This is useful for debugging, but the task will take
// longer to complete due to the added delay (which is necessary
// to actually see the output
#define DRAW_PROCESS_QUEUES		1
//-------------------------------------------------

#ifndef WRITE
#define WRITE(str) lcd_writeProgString(PSTR(str))
#endif
#define TEST_PASSED \
do { ATOMIC { \
	lcd_clear(); \
	WRITE("  TEST PASSED   "); \
} } while (0)

// Keep queue on screen with failure message
// Note: this restricts messages to 16 chars
#define TEST_FAILED(reason) \
do { ATOMIC { \
	lcd_clear(); \
	WRITE("FAIL  "); \
	WRITE(reason); \
} } while (0)


#ifndef CONFIRM_REQUIRED
	#define CONFIRM_REQUIRED 1
#endif

// turn off process queue drawing by defining SHOW_QUEUE as empty
#if !DRAW_PROCESS_QUEUES
	#define SHOW_QUEUE
#endif

//! call the queue display method with the address of a locally defined ProcessQueue queue, and a delay depending on queue_display_delay_factor
// only define this macro if it has not been already to allow overriding by external tools
#ifndef SHOW_QUEUE
	#define SHOW_QUEUE test_displayProcessQueue(&queue, queue_display_delay_factor * DEFAULT_OUTPUT_DELAY)
#endif

//! Show a failure message with the option to switch to a queue view
#define TEST_FAILED_QUEUE_VIEWER(message) \
	do { \
		TEST_FAILED(message); \
		os_waitForInput(); \
		os_waitForNoInput(); \
		test_drawProcessQueue(&queue); \
		os_waitForInput(); \
		os_waitForNoInput(); \
	} while(1)


// Simple macro to define an array (using compound initializers) in-place
// This macro uses variadic arguments, so it can take any number of arguments
// __VA_ARGS__ just resembles exactly the input arguments as a text string with commas
// so all input values will just be pasted into an array initializer
#define Q(...) ((ProcessID []){__VA_ARGS__})

// Test if the queue resembles pattern after removing pid
// expects a local ProcessQueue queue
#define assert_pattern_after_remove(pid, pattern, message) \
	do { \
		pqueue_removePID(&queue, pid); \
		SHOW_QUEUE; \
		if (!test_processQueueMatches(&queue, pattern, sizeof(pattern) / sizeof(pattern[0]))) \
			TEST_FAILED_QUEUE_VIEWER("Remove error: "message); \
	} while(0)


//! draws a visual representation of the queue array, head and tail
static void test_drawProcessQueue(const ProcessQueue *queue) {
	// calculate the length of the queue defined by the students
	// using sizeof() should work here since data is required to be 
	// an array inside of the struct with a fixed size
	uint8_t array_length = sizeof(queue->data) / sizeof(queue->data[0]);
	lcd_clear();
	lcd_writeChar('[');
	lcd_goto(1,array_length + 2);
	lcd_writeChar(']');
	
	
	// tail is marked with a ^, head with . . LCD_CC_IXI is a custom char
	// that displays both a . and a ^ in the same cell
	if (queue->head == queue->tail) {
		lcd_goto(2, queue->tail + 2);
		lcd_writeChar(LCD_CC_IXI);
		} else {
		lcd_goto(2, queue->tail + 2);
		lcd_writeChar('.');
		lcd_goto(2, queue->head + 2);
		lcd_writeChar('^');
	}

	// go through the queue and print the elements
	// there are valid queue implementations where the whole queue
	// can be filled with user data, so head == tail can mean either a completely
	// full or completely empty queue. Therefore we have to do a case distinction
	// with pqueue_hasNext
	if (pqueue_hasNext(queue)) {
		uint8_t i = queue->tail;
		lcd_goto(1,queue->tail + 2);
		do {
			// for invalid data inside the queue we write an X
			if (queue->data[i] < MAX_NUMBER_OF_PROCESSES) {
				lcd_writeDec(queue->data[i]);
			} else {
				lcd_writeChar('X');
			}
			i++;
			// wrap-around both counter and lcd pos
			if (i == array_length) {
				i = 0;
				lcd_goto(1,2);
			}
		} while (i != queue->head);
	}
}

/*! display the queue using \p pqueue_draw for the given delay \p delay.
 * Waits for all buttons to be released before returning, so the program can be frozen by holding down a button
 * to look at the queue
 */
static void test_displayProcessQueue(const ProcessQueue *queue, uint16_t delay) {
	test_drawProcessQueue(queue);
	delayMs(delay);
	// allow students to pause execution to look
	// at current queue state
	os_waitForNoInput();
}

//! non-destructively check whether a queue contains the given list of processes in that order
static bool test_processQueueMatches(const ProcessQueue *queue, const ProcessID pat[], size_t length) {
	ProcessQueue copy;
	memcpy(&copy, queue, sizeof(ProcessQueue));
	for(size_t i = 0; i < length; i++) {
		if (!pqueue_hasNext(&copy)) {
			return false;
		}
		if (pqueue_getFirst(&copy) != pat[i]) {
			return false;
		}
		pqueue_dropFirst(&copy);
	}
	return !pqueue_hasNext(&copy);
}

void endless_loop(void) { 
	HALT; 
}

REGISTER_AUTOSTART(test_program);
void test_program(void) {
	// register a custom char that looks like a ^ and a . in the same LCD cell
	// to elegantly represent the head == tail situation
	// There is no particular reason for using the DEGREE slot
	// but it is likely not used in default messages of the OS
	lcd_registerCustomChar(LCD_CC_IXI, CUSTOM_CHAR(
		0b00100,
		0b01010,
		0b10001,
		0b00000,
		0b00000,
		0b01100,
		0b01100,
		0b00000
	));
	
	os_setSchedulingStrategy(OS_SS_EVEN);
	
#if PHASE_1_INIT || PHASE_2_SIMPLE || PHASE_3_CONSISTENCY || PHASE_4_REMOVE
	ProcessQueue queue;
	pqueue_init(&queue);
	uint8_t queue_display_delay_factor = 10;
#endif
	
// Phase 1: Test Queue initialization
#if PHASE_1_INIT
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 1: Init"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
	
	SHOW_QUEUE;
	
	// head / tail must be 0
	if (queue.head != 0 || queue.tail != 0) {
		TEST_FAILED_QUEUE_VIEWER("Head/Tail not 0");
	}

	// For MLFQ, it is sufficient to fit all processes except
	// the idle process (which is only scheduled as a last-resort)
	if (queue.size < MAX_NUMBER_OF_PROCESSES - 1) {
		TEST_FAILED_QUEUE_VIEWER("size too small for MLFQ");
	}

	// size must not be greater than the number of elements in the array
	if (queue.size > sizeof(queue.data) / sizeof(queue.data[0])) {
		TEST_FAILED_QUEUE_VIEWER("array too small for size");
	}

	lcd_clear();
	lcd_writeProgString(PSTR("Phase 1: Init"));
	lcd_goto(2, 15);
	lcd_writeProgString(PSTR("OK"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
#endif
	
// Phase 2: Simple Queue Operations
// Enqueue a single element, get it with getFirst and drop it again
#if PHASE_2_SIMPLE
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 2: Simple Operations"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
	SHOW_QUEUE;
	pqueue_append(&queue, 1);
	SHOW_QUEUE;
	
	// Create a copy of the queue to test that getFirst does not change state
	ProcessQueue queue_copy = queue;
	
	if (!pqueue_hasNext(&queue)) {
		TEST_FAILED_QUEUE_VIEWER("queue should have next");
	}
	if (pqueue_getFirst(&queue) != 1) {
		TEST_FAILED_QUEUE_VIEWER("unexpected element");
	}
	if (memcmp(&queue, &queue_copy, sizeof(ProcessQueue))) {
		TEST_FAILED_QUEUE_VIEWER("getFirst changed queue");
	}
	pqueue_dropFirst(&queue);
	SHOW_QUEUE;
	
	if (pqueue_hasNext(&queue)) {
		TEST_FAILED_QUEUE_VIEWER("queue should be empty");
	}
		
	pqueue_reset(&queue);
	SHOW_QUEUE;
	
	if (queue.head != 0 || queue.tail != 0) {
		TEST_FAILED_QUEUE_VIEWER("head/tail != 0 after reset");
	}
	
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 2: Simple Operations"));
	lcd_goto(2, 15);
	lcd_writeProgString(PSTR("OK"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
#endif
	
// Phase 3: Consistency
#if PHASE_3_CONSISTENCY
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 3: Consistency"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);

	// we are doing a lot of operations so we reduce the time for each queue display
	queue_display_delay_factor = 1;
		
	// We want to test for possible edge cases in the enqueue / dequeue logic
	// that can occur when wrapping from the end of the array to the start
	// Therefore, for every possible start position of the tail, we fill a queue
	// completely and then clear it out again, while checking the values
	// Since we add the IDs of all processes, we also check whether the
	// queue has suitable space to implement MLFQ as a side effect
	uint8_t array_length = sizeof(queue.data) / sizeof(queue.data[0]);
	
	for (uint8_t start = 0; start < array_length; start++) {
		queue.head = queue.tail = start;
		for (ProcessID pid = 1; pid < MAX_NUMBER_OF_PROCESSES; pid++) {
			pqueue_append(&queue, pid);
			SHOW_QUEUE;
		}
		
		for (ProcessID pid = 1; pid < MAX_NUMBER_OF_PROCESSES; pid++) {
			if (!pqueue_hasNext(&queue)) {
				TEST_FAILED_QUEUE_VIEWER("queue should have next");
			}
			if (pqueue_getFirst(&queue) != pid) {
				TEST_FAILED_QUEUE_VIEWER("unexpected element");
			}
			pqueue_dropFirst(&queue);
			SHOW_QUEUE;
		}
		if (pqueue_hasNext(&queue)) {
			TEST_FAILED_QUEUE_VIEWER("queue should be empty");
		}
	}
	
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 3: Consistency"));
	lcd_goto(2, 15);
	lcd_writeProgString(PSTR("OK"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
#endif
	
#if PHASE_4_REMOVE
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 4: Remove"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
	queue_display_delay_factor = 10;

	

	ProcessQueue template;
	pqueue_init(&template);
	template.head = template.tail = template.size / 2;


	for (ProcessID pid = 1; pid < 8; pid++) {
		pqueue_append(&template, pid);
	}


	// Simple checks for removing an element at the front, back or in the middle
	// We restore the queue every time to make sure that it wraps around
	// this makes it more likely to provoke errors
	queue = template;
	SHOW_QUEUE;
	assert_pattern_after_remove(1, Q(2,3,4,5,6,7), "front");
	
	
	queue = template;
	SHOW_QUEUE;
	assert_pattern_after_remove(7, Q(1,2,3,4,5,6), "back");
	queue = template;
	SHOW_QUEUE;
	assert_pattern_after_remove(4, Q(1,2,3,5,6,7), "mid");


	queue = template;
	SHOW_QUEUE;
	assert_pattern_after_remove(4, Q(1,2,3,5,6,7), "successive");
	assert_pattern_after_remove(1, Q(2,3,5,6,7), "successive");
	assert_pattern_after_remove(7, Q(2,3,5,6), "successive");
	assert_pattern_after_remove(3, Q(2,5,6), "successive");
	assert_pattern_after_remove(2, Q(5,6), "successive");
	assert_pattern_after_remove(6, Q(5), "successive");
	assert_pattern_after_remove(5, Q(), "successive");
	
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 4: Remove"));
	lcd_goto(2, 15);
	lcd_writeProgString(PSTR("OK"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
#endif
	
#if PHASE_5_MLFQ
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 5: MLFQ Integration"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);

	// we do not want to run our dummy processes or the MLFQ strat
	os_enterCriticalSection();

	/*
	 * we want four processes with PID 1-4, with a respective priority of
	 * (pid - 1) << 6 to cover all process queues.
	 * The test process should have PID 1, so we can use it if we manually change its prio
	 * In addition, we need to exec three new processes
	 */
	if (os_getCurrentProc() != 1) {
		TEST_FAILED("Incorrect PID");
	}

	os_getProcessSlot(1)->priority = 0b00000000;
	
	if (os_exec(endless_loop, 0b01000000) != 2) {
		TEST_FAILED("Incorrect PID");
		HALT;
	}
		
	if (os_exec(endless_loop, 0b10000000) != 3) {
		TEST_FAILED("Incorrect PID");
		HALT;
	}
	
	if (os_exec(endless_loop, 0b11000000) != 4) {
		TEST_FAILED("Incorrect PID");
		HALT;
	}
	
	// this call should reset the scheduling information
	// and insert the processes into the queues
	os_setSchedulingStrategy(OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE);
	

	// The process queues should be ordered by priority, but it is not specified
	// whether that order is ascending or descending, so both should pass the testtask

	// we will use some flags that are both true initially and are set to false
	// whenever we detect that the queues do not match the pattern
	bool asc = true;
	bool desc = true;
	

	// Check if process queues are sorted by prio ascending
	for (uint8_t qid = 0; qid < 4 && asc; qid++) {
		// process in queue 0 should be 1, ...
		// only single process per queue
		uint8_t pattern[] = {qid + 1};
		ProcessQueue const* q = MLFQ_getQueue(qid);
		if (!test_processQueueMatches(q, pattern, sizeof(pattern))) {
			asc = false;
		}
	}

	// if not ascending, order could still be descending or false
	if (!asc) {

		// check if process queues are sorted by prio descending
		for (uint8_t qid = 0; qid < 4 && desc; qid++) {
			// process in queue 0 should be 4 since it has highest prio
			uint8_t pattern[] = {4 - qid};
			const ProcessQueue *q = MLFQ_getQueue(qid);
			if (!test_processQueueMatches(q, pattern, sizeof(pattern))) {
				desc = false;
			}
		}
		// if neither of these patterns match, display an error
		// and offer to view the queues
		if (!desc) {
			// page 0 : error message, pages 1 - 4: queues 0 - 3
			uint8_t page = 0;
			while(true) {
				lcd_clear();
				if (page == 0) {
					TEST_FAILED("Incorrect queue (press UP)");
				} else {
					test_drawProcessQueue(MLFQ_getQueue(page - 1));
					lcd_goto(2, 16);
					lcd_writeDec(page - 1);
				}

				// wait until either up or down is pressed and record it
				uint8_t in;
				while(!(in = os_getInput() & 0b0110));

				// increment or decrement page accordingly
				if (in & 0b0100 && page < 4) {
					page++;
				}
				if (in & 0b0010 && page > 0) {
					page--;
				}
				os_waitForNoInput();
			}
		}
	}
	
	// we passed the test. kill all remaining process
	// so that we end with the idle process running as usual
	for (ProcessID pid = 2; pid < 5; pid++) {
		if (!os_kill(pid)) {
			TEST_FAILED("Kill failure");
			HALT;
		}
	}
	
	// ensure that MLFQ will not be called and break something
	os_setSchedulingStrategy(OS_SS_EVEN);
	os_leaveCriticalSection();
	
	lcd_clear();
	lcd_writeProgString(PSTR("Phase 5: MLFQ Integration"));
	lcd_goto(2, 15);
	lcd_writeProgString(PSTR("OK"));
	delayMs(DEFAULT_OUTPUT_DELAY * 10);
	
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
