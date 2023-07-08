/*! \file
 *  \brief Scheduling module for the OS.
 *
 * Contains everything needed to realise the scheduling between multiple processes.
 * Also contains functions to start the execution of programs.
 *
 *  \author   Lehrstuhl Informatik 11 - RWTH Aachen
 *  \date     2013
 *  \version  2.0
 */

#include "os_scheduler.h"
#include "util.h"
#include "os_input.h"
#include "os_scheduling_strategies.h"
#include "os_taskman.h"
#include "os_core.h"
#include "lcd.h"
#include "os_memheap_drivers.h"
#include "os_memory.h"

#include <util/atomic.h>
#include <avr/interrupt.h>
#include <stdbool.h>

//----------------------------------------------------------------------------
// Private Types
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------



ProcessID choosenProc; //Needed for the IRS since it can not have lokal Varibles

//! Array of states for every possible process
//#warning IMPLEMENT STH. HERE
Process os_processes[MAX_NUMBER_OF_PROCESSES]; 

//! Index of process that is currently executed (default: idle)
//#warning IMPLEMENT STH. HERE
ProcessID currentProc = 0;

//----------------------------------------------------------------------------
// Private variables
//----------------------------------------------------------------------------

//! Currently active scheduling strategy
//#warning IMPLEMENT STH. HERE
SchedulingStrategy currentStrategy; 

//! Count of currently nested critical sections
uint8_t criticalSectionCount = 0;

//----------------------------------------------------------------------------
// Private function declarations
//----------------------------------------------------------------------------

//! ISR for timer compare match (scheduler)
ISR(TIMER2_COMPA_vect) __attribute__((naked));

//----------------------------------------------------------------------------
// Function definitions
//----------------------------------------------------------------------------

/*!
 *  Timer interrupt that implements our scheduler. Execution of the running
 *  process is suspended and the context saved to the stack. Then the periphery
 *  is scanned for any input events. If everything is in order, the next process
 *  for execution is derived with an exchangeable strategy. Finally the
 *  scheduler restores the next process for execution and releases control over
 *  the processor to that process.
 */
ISR(TIMER2_COMPA_vect) {
    saveContext();
    if(os_processes[os_getCurrentProc()].state != OS_PS_UNUSED){
	    os_processes[os_getCurrentProc()].sp.as_int = SP;
	    SP = BOTTOM_OF_ISR_STACK;
	    os_processes[os_getCurrentProc()].checksum = os_getStackChecksum(os_getCurrentProc());
	    if (os_processes[os_getCurrentProc()].state != OS_PS_BLOCKED) {os_processes[os_getCurrentProc()].state = OS_PS_READY;}
    }
    if ((os_getInput()&0b00001001) == 0b00001001) {
	    os_waitForNoInput();
	    os_taskManMain();
    }
	switch (os_getSchedulingStrategy()) { //using globel Var, since IRS cannot use lokal Var
		case OS_SS_EVEN: choosenProc = os_Scheduler_Even(os_processes,os_getCurrentProc()); break;
		case OS_SS_INACTIVE_AGING: choosenProc = os_Scheduler_InactiveAging(os_processes,os_getCurrentProc()); break;
		case OS_SS_RANDOM: choosenProc = os_Scheduler_Random(os_processes,os_getCurrentProc()); break;
		case OS_SS_ROUND_ROBIN: choosenProc = os_Scheduler_RoundRobin(os_processes,os_getCurrentProc()); break;
		case OS_SS_RUN_TO_COMPLETION: choosenProc = os_Scheduler_RunToCompletion(os_processes,os_getCurrentProc()); break;
		case OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE: choosenProc = os_Scheduler_MLFQ(os_processes,os_getCurrentProc()); break;
	}
	if (os_processes[choosenProc].checksum != os_getStackChecksum(choosenProc)) {os_error(("Stack Inconsistenz erkannt."));} 
	os_processes[choosenProc].state = OS_PS_RUNNING;
	SP = os_processes[choosenProc].sp.as_int;
	currentProc = choosenProc;
	restoreContext();
	
}
/*
Function that is used by processes to give away their remaining processing time to another process.
Their ProcessState is then being set to OS_PS_BLOCKED so the Scheduler ISR knows not to repick the same process.
Does not change the ProcessState if used to give away the remaining processing time of a terminating process.
*/
void os_yield () {
	os_enterCriticalSection();
	uint8_t prevCriticalSectionCount;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){prevCriticalSectionCount = criticalSectionCount;}
	uint8_t prevGlobalInterrupt = SREG | 0b01111111;
	if (os_processes[currentProc].state != OS_PS_UNUSED) {os_processes[currentProc].state = OS_PS_BLOCKED;}
	while (criticalSectionCount) {
		os_leaveCriticalSection();
	} 
	SREG |= 0b1000000;
	
	TIMER2_COMPA_vect();
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {criticalSectionCount = prevCriticalSectionCount; TIMSK2 &= 0b11111101;}
	SREG &= prevGlobalInterrupt;
	os_leaveCriticalSection();
}

/*!
 *  This is the idle program. The idle process owns all the memory
 *  and processor time no other process wants to have.
 */
void idle(void) {
    while (1) {
		delayMs(DEFAULT_OUTPUT_DELAY);
		lcd_writeErrorProgString(PSTR("...."));
	}
}

/*!
 *  This function is used to execute a program that has been introduced with
 *  os_registerProgram.
 *  A stack will be provided if the process limit has not yet been reached.
 *  This function is multitasking safe. That means that programs can repost
 *  themselves, simulating TinyOS 2 scheduling (just kick off interrupts ;) ).
 *
 *  \param program  The function of the program to start.
 *  \param priority A priority ranging 0..255 for the new process:
 *                   - 0 means least favourable
 *                   - 255 means most favourable
 *                  Note that the priority may be ignored by certain scheduling
 *                  strategies.
 *  \return The index of the new process or INVALID_PROCESS as specified in
 *          defines.h on failure
 */
ProcessID os_exec(Program *program, Priority priority) {
	os_enterCriticalSection();
    //find free processSlot
	// Reserve Slot 0 for idle()
	uint8_t freeSlot = MAX_NUMBER_OF_PROCESSES;
	for (uint8_t i = 0; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if (os_processes[i].state == OS_PS_UNUSED) { freeSlot = i; break;}
	} 
	if (freeSlot == MAX_NUMBER_OF_PROCESSES) {os_leaveCriticalSection(); return INVALID_PROCESS;}
		
	//Verify program validity TODO: Catch program pointers outside of valid range
	//program = &os_dispatcher; //Adresse der Funktion os_dispatcher ¨¹bergeben
	if (program == NULL) {os_leaveCriticalSection(); return INVALID_PROCESS;}
		
	//Save program, program state and program priority
	os_processes[freeSlot].state = OS_PS_READY;
	os_processes[freeSlot].program = program;
	os_processes[freeSlot].priority = priority; 
	
	
	//Initialize Process Stack
	StackPointer pointer = {PROCESS_STACK_BOTTOM(freeSlot)};
	*(pointer.as_ptr) = (uint16_t)os_dispatcher; //Set bottom of Process-Stack to Low Byte of program address   
	*(pointer.as_ptr-1) = (uint16_t)os_dispatcher >> 8; // Set second-Bottom Byte of Process-Stack to High Byte of program address
	for (uint8_t i = 2; i <= 34; i++) {
		*(pointer.as_ptr-i) = (uint8_t)0; //Set the next 33 Bytes of the Process-Stack to NULL. The third-Bottom Byte of represents the SREG Registers, the next 32 Byte represent the CPU-Registers.
	} 
	
	//Set the Stack-Pointer to the first free Var
	os_processes[freeSlot].sp.as_int = PROCESS_STACK_BOTTOM(freeSlot)-35; 
	os_processes[freeSlot].checksum = os_getStackChecksum(freeSlot);
	os_resetProcessSchedulingInformation(freeSlot);
	
	os_leaveCriticalSection();
	return freeSlot;
	
}

/*!
 *  If all processes have been registered for execution, the OS calls this
 *  function to start the idle program and the concurrent execution of the
 *  applications.
 */
void os_startScheduler(void) {
    //#warning IMPLEMENT STH. HERE
	currentProc = 0;
	//os_processes[0].program = &idle;
	//StackPointer pointer = {PROCESS_STACK_BOTTOM(0)};
	//*(pointer.as_ptr) = ((uint16_t)&idle); //Set bottom of Process-Stack to Low Byte of program address
	//*(pointer.as_ptr-1) = ((uint16_t)&idle)>>8; // Set second-Bottom Byte of Process-Stack to High Byte of program address
	//os_processes[0].sp.as_int = PROCESS_STACK_BOTTOM(0)-35;
	os_processes[0].state = OS_PS_RUNNING;
	SP = os_processes[0].sp.as_int;
	restoreContext();
}

/*!
 *  In order for the Scheduler to work properly, it must have the chance to
 *  initialize its internal data-structures and register.
 */
void os_initScheduler(void) {
    //#warning IMPLEMENT STH. HERE
	os_initSchedulingInformation();
	for (uint8_t i = 0; i < MAX_NUMBER_OF_PROCESSES; i++) {
		os_processes[i].state = OS_PS_UNUSED;
	}
	os_exec(*idle,DEFAULT_PRIORITY);
	//TODO: Make Sure that idle is not included in List autostart
	struct program_linked_list_node* iter = autostart_head;
	while (iter != NULL) {
		os_exec(iter->program, DEFAULT_PRIORITY);
		iter = iter->next;
	}
}

/*!
 *  A simple getter for the slot of a specific process.
 *
 *  \param pid The processID of the process to be handled
 *  \return A pointer to the memory of the process at position pid in the os_processes array.
 */
Process* os_getProcessSlot(ProcessID pid) {
    return os_processes + pid;
}

/*!
 *  A simple getter to retrieve the currently active process.
 *
 *  \return The process id of the currently active process.
 */
ProcessID os_getCurrentProc(void) {
    //#warning IMPLEMENT STH. HERE
	return currentProc;
}

/*!
 *  Sets the current scheduling strategy.
 *
 *  \param strategy The strategy that will be used after the function finishes.
 */
void os_setSchedulingStrategy(SchedulingStrategy strategy) {
    currentStrategy = strategy;
	os_resetSchedulingInformation(currentStrategy);
}

/*!
 *  This is a getter for retrieving the current scheduling strategy.
 *
 *  \return The current scheduling strategy.
 */
SchedulingStrategy os_getSchedulingStrategy(void) {
    return currentStrategy;
}

/*!
 *  Enters a critical code section by disabling the scheduler if needed.
 *  This function stores the nesting depth of critical sections of the current
 *  process (e.g. if a function with a critical section is called from another
 *  critical section) to ensure correct behaviour when leaving the section.
 *  This function supports up to 255 nested critical sections.
 */

void os_enterCriticalSection(void) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
	if (criticalSectionCount == 255) {
		os_errorPStr(PSTR("CriticalSection Overflow"));
	}
	else {criticalSectionCount++;}
	TIMSK2 &= 0b11111101;
	}
}

/*
void os_enterCriticalSection(void) {
    uint8_t prevGlobalInterrupt = SREG & 0b10000000;
	SREG &= 0b01111111;
	if (criticalSectionCount == 255) {
		os_errorPStr(PSTR("CriticalSection Overflow"));
	}
	else {criticalSectionCount++;}
	TIMSK2 &= 0b11111101;
	SREG |= prevGlobalInterrupt;
}
*/


/*!
 *  Leaves a critical code section by enabling the scheduler if needed.
 *  This function utilizes the nesting depth of critical sections
 *  stored by os_enterCriticalSection to check if the scheduler
 *  has to be reactivated.
 */


void os_leaveCriticalSection(void) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (criticalSectionCount == 0) {os_errorPStr(PSTR("CriticalSection Underflow"));}
		else {criticalSectionCount--;}
		if (criticalSectionCount == 0) {TIMSK2 |= 0b00000010;}
	}
} 

/*
void os_leaveCriticalSection(void) {
    //#warning IMPLEMENT STH. HERE
	uint8_t prevGlobalInterrupt = SREG & 0b10000000;
	SREG &= 0b01111111;
	if (criticalSectionCount == 0) {os_errorPStr(PSTR("CriticalSection Underflow"));}
	else {criticalSectionCount--;}
	if (criticalSectionCount == 0) {TIMSK2 |= 0b00000010;}
	SREG |= prevGlobalInterrupt;
} */

/*!
 *  Calculates the checksum of the stack for a certain process.
 *
 *  \param pid The ID of the process for which the stack's checksum has to be calculated.
 *  \return The checksum of the pid'th stack.
 */
StackChecksum os_getStackChecksum(ProcessID pid) {
    //#warning IMPLEMENT STH. HERE
	StackPointer iter = {PROCESS_STACK_BOTTOM(pid)};
	uint8_t checksum = 0;
	while (iter.as_int != os_processes[pid].sp.as_int) {
		checksum ^= *iter.as_ptr;
		iter.as_ptr--;
	}
	return checksum;
}

bool os_kill(ProcessID pid){
	//Leerlaufprozess darf nicht gekillt werden
	if (pid == 0){ 
		return false;
	}
	os_enterCriticalSection();
	//Prozess terminiert
	os_processes[pid].state = OS_PS_UNUSED;
	os_freeProcessMemory(intHeap,pid);
	os_freeProcessMemory(extHeap,pid);
	
	//überprüfen, ob es noch geöffnete kritische Bereiche gibt
	if (pid == currentProc){
		os_yield(); //Endlosschleife
	}
	os_leaveCriticalSection();
	return true;
}


void os_dispatcher(void){
	//Prozess-ID des aktuellen Prozesses
	ProcessID i = os_getCurrentProc();
	
	//die Funktionszeiger des Programmes bestimmen und aufrufen
	(os_processes[i].program)();

	//aktiver Prozess terminieren
	os_kill(i);
	os_yield();
}
