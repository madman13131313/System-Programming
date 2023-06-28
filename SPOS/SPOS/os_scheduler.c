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
#include <avr/interrupt.h>
#include <stdbool.h>

//----------------------------------------------------------------------------
// Private Types
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------

//! Array of states for every possible process
Process os_processes[MAX_NUMBER_OF_PROCESSES];

//! Index of process that is currently executed (default: idle)
ProcessID currentProc = 0;

//----------------------------------------------------------------------------
// Private variables
//----------------------------------------------------------------------------

//! Currently active scheduling strategy
SchedulingStrategy currentStrategy;

//! Count of currently nested critical sections
uint8_t criticalSectionCount;

//----------------------------------------------------------------------------
// Private function declarations
//----------------------------------------------------------------------------

//! ISR for timer compare match (scheduler)
ISR(TIMER2_COMPA_vect) __attribute__((naked));

//----------------------------------------------------------------------------
// Function definitions
//----------------------------------------------------------------------------

void setCurrentProc(void){
	if (currentStrategy == OS_SS_EVEN)
	{
		currentProc = os_Scheduler_Even(os_processes, currentProc);
	}if (currentStrategy == OS_SS_RANDOM)
	{
		currentProc = os_Scheduler_Random(os_processes, currentProc);
	}if (currentStrategy == OS_SS_INACTIVE_AGING)
	{
		currentProc = os_Scheduler_InactiveAging(os_processes, currentProc);
	}if (currentStrategy == OS_SS_ROUND_ROBIN)
	{
		currentProc = os_Scheduler_RoundRobin(os_processes, currentProc);
	}if (currentStrategy == OS_SS_RUN_TO_COMPLETION)
	{
		currentProc = os_Scheduler_RunToCompletion(os_processes, currentProc);
	}if (currentStrategy == OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE)
	{
		currentProc = os_Scheduler_MLFQ(os_processes, currentProc);
	}
}

/*!
 *  Timer interrupt that implements our scheduler. Execution of the running
 *  process is suspended and the context saved to the stack. Then the periphery
 *  is scanned for any input events. If everything is in order, the next process
 *  for execution is derived with an exchangeable strategy. Finally the
 *  scheduler restores the next process for execution and releases control over
 *  the processor to that process.
 */
ISR(TIMER2_COMPA_vect) {
	//1. Speichern des Programmzählers als Rücksprungadresse (implizit)
    //2. Sichern des Laufzeitkontextes des aktuellen Prozesses auf dessen Prozessstack.
	saveContext();
	
	//3. Sichern des Stackpointers für den Prozessstack des aktuellen Prozesses
	os_processes[currentProc].sp.as_int = SP;
	
	//4. Setzen des SP-Registers auf den Scheduler-Stack
	SP = BOTTOM_OF_ISR_STACK;
	
	// Aufruf des Taskmanagers
	if (os_getInput() == 0b00001001) {
		os_waitForNoInput();
		os_taskManMain(); 
	}
	
	//5. Setzen des Prozesszustandes des aktuellen Prozesses auf OS_PS_READY
	if (os_processes[currentProc].state == OS_PS_RUNNING)
	{
		os_processes[currentProc].state = OS_PS_READY;
	}
	
	//checksum abspeichern
	os_processes[currentProc].checksum = os_getStackChecksum(currentProc);
	
	//6. Auswahl des nächsten fortzusetzenden Prozesses
	setCurrentProc();
	
	// checksum check
	if (os_getStackChecksum(currentProc) != os_processes[currentProc].checksum) {
		os_error("checksum changed!");
	}
	
	//7. Setzen des Prozesszustandes des fortzusetzenden Prozesses auf OS_PS_RUNNING
	os_processes[currentProc].state = OS_PS_RUNNING;
	
	//8. Wiederherstellen des Stackpointers für den Prozessstack des fortzusetzenden Prozesses
	SP = os_processes[currentProc].sp.as_int;
	
	//9. Wiederherstellen des Laufzeitkontextes des fortzusetzenden Prozesses
	restoreContext();
}

/*!
 *  This is the idle program. The idle process owns all the memory
 *  and processor time no other process wants to have.
 */
void idle(void) {
    while (1)
    {
		lcd_writeString(".");
		delayMs(DEFAULT_OUTPUT_DELAY);
    }
}

// Used to kill a running process and clear the corresponding process slot.
bool os_kill(ProcessID pid){
	os_enterCriticalSection();
	if (pid == 0)
	{
		os_leaveCriticalSection();
		return false;
	}
	if (pid >= MAX_NUMBER_OF_PROCESSES || os_processes[pid].state == OS_PS_UNUSED)
	{
		os_leaveCriticalSection();
		return false;
	}
	if (pid == os_getCurrentProc())
	{
		os_processes[pid].state = OS_PS_UNUSED;	
		os_processes[pid].program = NULL;
		for (uint8_t i = 0; i < os_getHeapListLength(); ++i)
		{
			os_freeProcessMemory(os_lookupHeap(i), pid);
		}
		while(criticalSectionCount > 0){
			os_leaveCriticalSection();}
		os_yield();
		return true;
	}
	else
	{
		os_processes[pid].state = OS_PS_UNUSED;
		os_processes[pid].program = NULL;
		for (uint8_t i = 0; i < os_getHeapListLength(); ++i)
		{
			os_freeProcessMemory(os_lookupHeap(i), pid);
		}
		os_leaveCriticalSection();
		return true;
	}
}


/*
Encapsulates any running process in order make it possible for processes to terminate
This wrapper enables the possibility to perfom a few necessary steps after the actual process function has finished.
*/
void os_dispatcher(){
	Program* p = os_processes[currentProc].program;
	if (*p == NULL)
	{
		os_error("dispatcher program null");
		return;
	}
	p();
	os_kill(currentProc);
	os_yield();
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
	ProcessID PID;
	//1. Freien Platz im Array os_processes finden
	for (PID = 0; PID < MAX_NUMBER_OF_PROCESSES; PID++)
	{
		if (os_processes[PID].state == OS_PS_UNUSED){
			break;
		}
	}
	if (PID == MAX_NUMBER_OF_PROCESSES)
	{
		os_leaveCriticalSection();
		return INVALID_PROCESS;
	}
	//2. Programmzeiger überprüfen
	if (*program == NULL) {
		os_leaveCriticalSection();
		return INVALID_PROCESS;
	}
	//3. Programm, Prozesszustand und Prozesspriorität speichern
	os_processes[PID].program = program;
	os_processes[PID].state = OS_PS_READY;
	os_processes[PID].priority = priority;
	
	//4. Prozessstack vorbereiten
	StackPointer sp;
	sp.as_int = PROCESS_STACK_BOTTOM(PID);
	// die initiale Rücksprungadresse speichern
	Program* p = &os_dispatcher;	
	*(sp.as_ptr) = (uint8_t) ((uint16_t)p); // Low byte
	sp.as_ptr--;
	*(sp.as_ptr) = (uint8_t) ((uint16_t)p >> 8); // High byte
	sp.as_ptr--;
	// den initialen Laufzeitkontext speichern
	for (int i = 0; i < 33; i++) {
		*(sp.as_ptr) = 0;
		sp.as_ptr--;
	}
	// der Stackpointer des neuen Prozesses auf die nun erste freie Speicherstelle des Prozessstacks gesetzt werden
	os_processes[PID].sp.as_ptr = sp.as_ptr;
	// Prüfsumme initialisieren
	os_processes[PID].checksum = os_getStackChecksum(PID);
	os_resetProcessSchedulingInformation(PID); // reset age of the new process
	os_leaveCriticalSection();
	return PID;
}

/*!
 *  If all processes have been registered for execution, the OS calls this
 *  function to start the idle program and the concurrent execution of the
 *  applications.
 */
void os_startScheduler(void) {
    currentProc = 0;
	os_processes[currentProc].state = OS_PS_RUNNING;
	SP = os_processes[currentProc].sp.as_int;
	restoreContext();
}

/*!
 *  In order for the Scheduler to work properly, it must have the chance to
 *  initialize its internal data-structures and register.
 */
void os_initScheduler(void) {
	// das Feld state aller Einträge auf OS_PS_UNUSED setzen
    for (int i = 0; i < MAX_NUMBER_OF_PROCESSES; i++)
    {
		os_processes[i].state = OS_PS_UNUSED;
    }
	
	// Starten des Leerlaufprozesses
	os_exec(idle, DEFAULT_PRIORITY);
	
	// Durchlaufen der autostart_head-Liste und Starten aller markierten Programme
	while (autostart_head != NULL) {
		os_exec(autostart_head->program, DEFAULT_PRIORITY);
		autostart_head = autostart_head->next;
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
    return currentProc;
}

/*!
 *  Sets the current scheduling strategy.
 *
 *  \param strategy The strategy that will be used after the function finishes.
 */
void os_setSchedulingStrategy(SchedulingStrategy strategy) {
    currentStrategy = strategy;
	if (strategy == OS_SS_ROUND_ROBIN || strategy == OS_SS_INACTIVE_AGING || strategy == OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE)
	{
		os_resetSchedulingInformation(strategy);
	}
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
	if (criticalSectionCount >= 0b11111111)
	{
		os_error("Overflow");
		return;
	}
    uint8_t sreg = SREG; // Speichern des Global Interrupt Enable Bit (GIEB) aus dem SREG
	SREG &= ~(1 << 7); // Deaktivieren des Global Interrupt Enable Bit
	criticalSectionCount++; // Inkrementieren der Verschachtelungstiefe des kritischen Bereiches
	TIMSK2 &= ~(1 << OCIE2A); // Deaktivieren des Schedulers
	SREG = sreg; // Wiederherstellen des (zuvor gespeicherten) Zustandes des Global Interrupt Enable Bit im SREG
}

/*!
 *  Leaves a critical code section by enabling the scheduler if needed.
 *  This function utilizes the nesting depth of critical sections
 *  stored by os_enterCriticalSection to check if the scheduler
 *  has to be reactivated.
 */
void os_leaveCriticalSection(void) {
	if (criticalSectionCount <= 0)
	{
		os_error("Underflow");
		return;
	}
   uint8_t sreg = SREG; // Speichern des Global Interrupt Enable Bit (GIEB) aus dem SREG
   SREG &= ~(1 << 7); // Deaktivieren des Global Interrupt Enable Bit
   criticalSectionCount--; // Decrementieren der Verschachtelungstiefe des kritischen Bereiches
   if (criticalSectionCount == 0)
   {
	   TIMSK2 |= (1 << OCIE2A); // aktivieren des Schedulers
   }
   SREG = sreg; // Wiederherstellen des (zuvor gespeicherten) Zustandes des Global Interrupt Enable Bit im SREG
}

/*!
 *  Calculates the checksum of the stack for a certain process.
 *
 *  \param pid The ID of the process for which the stack's checksum has to be calculated.
 *  \return The checksum of the pid'th stack.
 */
StackChecksum os_getStackChecksum(ProcessID pid) {
	StackChecksum sum = *(os_processes[pid].sp.as_ptr + 1);
	for (uint16_t i = os_processes[pid].sp.as_int + 2; i <= PROCESS_STACK_BOTTOM(pid); i++)
	{
		sum ^= *((uint8_t*)i);
	}
	return sum;
}

void os_yield(void) {
	os_enterCriticalSection();
	uint8_t criticalSectionCountTemp = criticalSectionCount;
	uint8_t sreg = SREG;
	if (os_processes[currentProc].state == OS_PS_RUNNING || os_processes[currentProc].state == OS_PS_READY) {
		os_processes[currentProc].state = OS_PS_BLOCKED;
	}
	criticalSectionCount = 1;
	os_leaveCriticalSection();
	TIMER2_COMPA_vect();
	SREG &= ~(1 << 7); // Deaktivieren des Global Interrupt Enable Bit
	SREG = sreg; // Re-enable global interrupts
	os_enterCriticalSection();
	criticalSectionCount = criticalSectionCountTemp;
	os_leaveCriticalSection();
}


