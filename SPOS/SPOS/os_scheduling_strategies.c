/*! \file

Scheduling strategies used by the Interrupt Service RoutineA from Timer 2 (in scheduler.c)
to determine which process may continue its execution next.

The file contains five strategies:
-even
-random
-round-robin
-inactive-aging
-run-to-completion
*/

#include "os_scheduling_strategies.h"
#include "defines.h"
#include "os_core.h"

#include <stdlib.h>

// globale Variable
SchedulingInformation schedulingInfo;

/*!
 *  Reset the scheduling information for a specific strategy
 *  This is only relevant for RoundRobin and InactiveAging
 *  and is done when the strategy is changed through os_setSchedulingStrategy
 *
 *  \param strategy  The strategy to reset information for
 */
void os_resetSchedulingInformation(SchedulingStrategy strategy) {
	if (strategy == OS_SS_ROUND_ROBIN)
	{
		schedulingInfo.timeSlice = (*os_getProcessSlot(os_getCurrentProc())).priority;
	}
	if (strategy == OS_SS_INACTIVE_AGING)
	{
		for (uint8_t i = 0; i < MAX_NUMBER_OF_PROCESSES; i++)
		{
			schedulingInfo.age[i] = 0;
		}
	}
	if (strategy == OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE)
	{
		for (uint8_t i = 0; i < 4; i++) {
			pqueue_reset(MLFQ_getQueue(i));
		}
		for (ProcessID i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
			if (os_getProcessSlot(i)->state != OS_PS_UNUSED) {
				uint8_t prio = os_getProcessSlot(i)->priority;
				schedulingInfo.zeitScheiben[i] = MLFQ_getDefaultTimeslice(MLFQ_MapToQueue(prio));
				MLFQ_removePID(i);
				pqueue_append(MLFQ_getQueue(MLFQ_MapToQueue(prio)), i);
			}
		}
	}
}

/*!
 *  Reset the scheduling information for a specific process slot
 *  This is necessary when a new process is started to clear out any
 *  leftover data from a process that previously occupied that slot
 *
 *  \param id  The process slot to erase state for
 */
void os_resetProcessSchedulingInformation(ProcessID id) {
    schedulingInfo.age[id] = 0;
	uint8_t prio = os_getProcessSlot(id)->priority;
	schedulingInfo.zeitScheiben[id] = MLFQ_getDefaultTimeslice(MLFQ_MapToQueue(prio));
	MLFQ_removePID(id);
	pqueue_append(MLFQ_getQueue(MLFQ_MapToQueue(prio)), id);
}

/*!
 *  This function implements the even strategy. Every process gets the same
 *  amount of processing time and is rescheduled after each scheduler call
 *  if there are other processes running other than the idle process.
 *  The idle process is executed if no other process is ready for execution
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed determined on the basis of the even strategy.
 */
ProcessID os_Scheduler_Even(Process const processes[], ProcessID current) {
	ProcessID i;
	for(i = current + 1; i < MAX_NUMBER_OF_PROCESSES; ++i) {
		if(processes[i].state == OS_PS_READY) {
			return i;
		}
		if(processes[i].state == OS_PS_BLOCKED) {
			os_getProcessSlot(i)->state = OS_PS_READY;
		}
	}
	for(i = 1 ; i <= MAX_NUMBER_OF_PROCESSES; ++i) {
		if(processes[i].state == OS_PS_READY) {
			return i;
		}
		if(processes[i].state == OS_PS_BLOCKED) {
			os_getProcessSlot(i)->state = OS_PS_READY;
		}
	}
	for(i = 1 ; i <= MAX_NUMBER_OF_PROCESSES; ++i) {
		if(processes[i].state == OS_PS_READY) {
			return i;}
	}
	return 0;
}


/*!
 *  This function implements the random strategy. The next process is chosen based on
 *  the result of a pseudo random number generator.
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed determined on the basis of the random strategy.
 */
ProcessID os_Scheduler_Random(Process const processes[], ProcessID current) {
    ProcessID readyProcesses[MAX_NUMBER_OF_PROCESSES];
	int numReady = 0;
	
	for (int i = 1; i < MAX_NUMBER_OF_PROCESSES; i++)
	{
		if (processes[i].state == OS_PS_READY)
		{
			readyProcesses[numReady] = i;
			numReady++;
		}
		if (processes[i].state == OS_PS_BLOCKED)
		{
			os_getProcessSlot(i)->state = OS_PS_READY;
		}
	}
	
	if (numReady == 0)
	{
		return 0;
	}
	
	int next = rand() % numReady;
	return readyProcesses[next];
}

/*!
 *  This function implements the round-robin strategy. In this strategy, process priorities
 *  are considered when choosing the next process. A process stays active as long its time slice
 *  does not reach zero. This time slice is initialized with the priority of each specific process
 *  and decremented each time this function is called. If the time slice reaches zero, the even
 *  strategy is used to determine the next process to run.
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed determined on the basis of the round robin strategy.
 */
ProcessID os_Scheduler_RoundRobin(Process const processes[], ProcessID current) {
	if (processes[current].state != OS_PS_UNUSED && processes[current].state != OS_PS_BLOCKED && schedulingInfo.timeSlice > 1)
	{
		schedulingInfo.timeSlice--;
		return current;
	}else
	{
		ProcessID next = os_Scheduler_Even(processes, current);
		schedulingInfo.timeSlice = processes[next].priority;
		return next;
	}
}

/*!
 *  This function realizes the inactive-aging strategy. In this strategy a process specific integer ("the age") is used to determine
 *  which process will be chosen. At first, the age of every waiting process is increased by its priority. After that the oldest
 *  process is chosen. If the oldest process is not distinct, the one with the highest priority is chosen. If this is not distinct
 *  as well, the one with the lower ProcessID is chosen. Before actually returning the ProcessID, the age of the process who
 *  is to be returned is reset to its priority.
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed, determined based on the inactive-aging strategy.
 */
ProcessID os_Scheduler_InactiveAging(Process const processes[], ProcessID current) {
	// the age of every waiting process is increased by its priority
	for (uint8_t i = 0; i < MAX_NUMBER_OF_PROCESSES; i++)
	{
		if ((processes[i].state==OS_PS_READY))
		{
			schedulingInfo.age[i] += processes[i].priority;
		}
	}
	ProcessID next = current;
	uint8_t max = 0;
	for (uint8_t i = 0; i < MAX_NUMBER_OF_PROCESSES; i++){
		if (processes[i].state == OS_PS_BLOCKED)
		{
			os_getProcessSlot(i)->state = OS_PS_READY;
		}
		else{
			// the oldest process is chosen
			if (schedulingInfo.age[i] > max)
			{
				max = schedulingInfo.age[i];
				next = i;
				// If the oldest process is not distinct, the one with the highest priority is chosen
			}else if (schedulingInfo.age[i] == max)
			{
				if (processes[i].priority > processes[next].priority)
				{
					next = i;
					// If this is not distinct as well, the one with the lower ProcessID is chosen
				}else if (processes[i].priority == processes[next].priority)
				{
					if (i < next)
					{
						next = i;
					}
				}
			}
		}
	}
	schedulingInfo.age[next] = processes[next].priority;
	return next;
}

/*!
 *  This function realizes the run-to-completion strategy.
 *  As long as the process that has run before is still ready, it is returned again.
 *  If  it is not ready, the even strategy is used to determine the process to be returned
 *
 *  \param processes An array holding the processes to choose the next process from.
 *  \param current The id of the current process.
 *  \return The next process to be executed, determined based on the run-to-completion strategy.
 */
ProcessID os_Scheduler_RunToCompletion(Process const processes[], ProcessID current) {
    if ((current != 0) && (processes[current].state == OS_PS_READY))
    {
	    return current;
    }
    return os_Scheduler_Even(processes, current);
}


// This function implements the multilevel-feedback-queue with 4 priority-classes. 
// Every process is inserted into a queue of a priority-class 
// and gets a default amount of timeslices which are class dependent. 
// If a process has no timeslices left, it is moved to the next class. 
// If a process yields, it is moved to the end of the queue.
ProcessID os_Scheduler_MLFQ(Process const processes[], ProcessID current){
	ProcessID id = 0;
	bool rearranged = false; // to check if the 4 process queues are rearranged
	// check from the highest priority class
	for (uint8_t q = 0; q < 4; q++) {
		ProcessQueue *queue = MLFQ_getQueue(q);
		uint8_t oldhead = queue->head; // to record the actual end of the queue to stop the search correctly
		for (uint8_t i = queue->tail; i != oldhead; i = (i+1)%queue->size) {
			id = pqueue_getFirst(queue);
			if (processes[id].state == OS_PS_UNUSED || id == 0) {
				pqueue_dropFirst(queue);
			}
			// if the time slice of the process in this class turns to 0, put the process into the lower class
			else if (schedulingInfo.zeitScheiben[id] == 0) {
				pqueue_dropFirst(queue);
				if (processes[id].state == OS_PS_BLOCKED) {
					os_getProcessSlot(id)->state = OS_PS_READY;
				}
				if (q + 1 < 4)
				{
					pqueue_append(MLFQ_getQueue(q + 1), id);
					// initialize the new time slice of the process in the lower class
					schedulingInfo.zeitScheiben[id] = MLFQ_getDefaultTimeslice(q + 1);
				}
				// Befindet sich der Prozess bereits in der niedrigsten Klasse
				else
				{
					pqueue_append(MLFQ_getQueue(3), id);
					// renew the time slice of the process
					schedulingInfo.zeitScheiben[id] = MLFQ_getDefaultTimeslice(3);
				}
				rearranged = true;
			}
			else if (processes[id].state == OS_PS_READY) {
				schedulingInfo.zeitScheiben[id]--;
				return id;
			}
			else if (processes[id].state == OS_PS_BLOCKED) {
				pqueue_dropFirst(queue);
				os_getProcessSlot(id)->state = OS_PS_READY;
				pqueue_append(queue,id);
				rearranged = true;
			}
		}
	}
	// if the process queues are rearranged, there could be still process remainign it the queues, before the idle process is returned.
	// So as long as the process queues are rearranged, we need to keep checking the whole process queues.
	if (rearranged) {return os_Scheduler_MLFQ(processes,current);}
	return 0;
}

// Initializes the given ProcessQueue with a predefined size.
void pqueue_init(ProcessQueue *queue){
	queue->size = MAX_NUMBER_OF_PROCESSES;
	queue->head = 0;
	queue->tail = 0;
}

// Resets the given ProcessQueue.
void pqueue_reset(ProcessQueue *queue){
	queue->head = 0;
	queue->tail = 0;
}

// Checks whether there is next a ProcessID.
bool pqueue_hasNext(const ProcessQueue *queue){
	return queue->head != queue->tail;
}

// Returns the first ProcessID of the given ProcessQueue.
ProcessID pqueue_getFirst(const ProcessQueue *queue){
	if (pqueue_hasNext(queue)) {
		return queue->data[queue->tail];
	} else {
		return 0;
	}
}

// Drops the first ProcessID of the given ProcessQueue.
void pqueue_dropFirst(ProcessQueue *queue){
	if (pqueue_hasNext(queue)) {
		queue->tail = (queue->tail+1)%queue->size;
	}
}

// Appends a ProcessID to the given ProcessQueue.
void pqueue_append(ProcessQueue *queue, ProcessID pid){
	 queue->data[queue->head] = pid;
	 queue->head = (queue->head + 1) % queue->size;
}

// Removes a ProcessID from the given ProcessQueue.
void pqueue_removePID(ProcessQueue *queue, ProcessID pid) {
	os_enterCriticalSection();
	uint8_t oldHead = queue->head; // to record the actual end of the queue to stop the search correctly
	if (oldHead < queue->size) {
		for (uint8_t i = queue->tail; i != oldHead; i = (i+1)%queue->size) {
			ProcessID id = pqueue_getFirst(queue);
			pqueue_dropFirst(queue);
			if (id != pid) {
				pqueue_append(queue, id);
			}
		}
	}
	os_leaveCriticalSection();
}

// Initialises the scheduling information.
void os_initSchedulingInformation(void){
	for (int i = 0; i < 4; i++) {
		pqueue_init(MLFQ_getQueue(i));
	}	
}

// Returns the corresponding ProcessQueue.
ProcessQueue* MLFQ_getQueue(uint8_t queueID){
	return &schedulingInfo.queues[queueID];
}

// Function that removes the given ProcessID from the ProcessQueues.
void MLFQ_removePID(ProcessID pid){
	for (int i = 0; i < 4; i++) {
		pqueue_removePID(&schedulingInfo.queues[i], pid);
	}
}

// Returns the default number of timeslices for a specific ProcessQueue/priority class.
uint8_t MLFQ_getDefaultTimeslice(uint8_t queueID){
	uint8_t timeSlice = 0;
	switch(queueID){
		case 0:
			timeSlice = 1;
			break;
		case 1:
			timeSlice = 2;
			break;
		case 2:
			timeSlice = 4;
			break;
		case 3:
			timeSlice = 8;
			break;
		default:
			timeSlice = 0;
			break;
	}
	return timeSlice;
}

// Maps a process-priority to a priority class.
uint8_t MLFQ_MapToQueue(Priority prio){
	switch (prio & 0b11000000) {
		case 0b11000000: return 0;
		case 0b10000000: return 1;
		case 0b01000000: return 2;
		case 0b00000000: return 3;
	}
	return 0;
}

