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
#include <stdbool.h>
#include <stdlib.h>

SchedulingInformation schedulingInfo;


// Helper Function
void resetMLFQProcessSchedulingInformation(ProcessID pid) {
	schedulingInfo.timeSliceMLFQ[pid] = MLFQ_getDefaultTimeslice(MLFQ_MapToQueue(os_getProcessSlot(pid)->priority));
	MLFQ_removePID(pid);
	pqueue_append(MLFQ_getQueue(MLFQ_MapToQueue(os_getProcessSlot(pid)->priority)),pid);
}
/*!
 *  Reset the scheduling information for a specific strategy
 *  This is only relevant for RoundRobin and InactiveAging
 *  and is done when the strategy is changed through os_setSchedulingStrategy
 *
 *  \param strategy  The strategy to reset information for
 */
void os_resetSchedulingInformation(SchedulingStrategy strategy) {
    // This is a presence task
	switch(strategy){
		case OS_SS_ROUND_ROBIN:
			schedulingInfo.timeSlice = os_getProcessSlot(os_getCurrentProc())->priority;
			break;
		case OS_SS_INACTIVE_AGING:
			for(uint8_t i=0; i<MAX_NUMBER_OF_PROCESSES;i++){
				schedulingInfo.age[i] = 0;
			}
			break;
		case OS_SS_MULTI_LEVEL_FEEDBACK_QUEUE:
			for (uint8_t i = 0; i < NUMBER_OF_PROCESS_QUEUES; i++) {
				pqueue_reset(MLFQ_getQueue(i));
			}
			for (ProcessID i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
				if (os_getProcessSlot(i)->state != OS_PS_UNUSED) {
					resetMLFQProcessSchedulingInformation(i);
				}
			}
			break;
		default: 
			break;
	}
	
}

/*!
 *  Reset the scheduling information for a specific process slot
 *  This is necessary when a new process is started to clear out any
 *  leftover data from a process that previously occupied that slot
 *
 *  \param id  The process slot to erase state for
 */
void os_resetProcessSchedulingInformation(ProcessID pid) {
    // This is a presence task
	schedulingInfo.age[pid] = 0;	
	resetMLFQProcessSchedulingInformation(pid);
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
    //#warning IMPLEMENT STH. HERE
	for (uint8_t i = 1; i <= MAX_NUMBER_OF_PROCESSES; i++) {
		if (processes[(current+i)%MAX_NUMBER_OF_PROCESSES].state == OS_PS_BLOCKED) {
			os_getProcessSlot((current+i)%MAX_NUMBER_OF_PROCESSES)->state = OS_PS_READY;
			return os_Scheduler_Even(processes,i);
		}
		if (processes[(current+i)%MAX_NUMBER_OF_PROCESSES].state == OS_PS_READY && (current+i)%MAX_NUMBER_OF_PROCESSES != 0) {
			return (current+i)%MAX_NUMBER_OF_PROCESSES;
		}
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
    //#warning IMPLEMENT STH. HERE
	uint8_t numberReady = 0;
	uint16_t randInt = rand();
	for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if (processes[i].state == OS_PS_READY || processes[i].state == OS_PS_BLOCKED) {numberReady++;}
	} 
	if (numberReady == 0) {return 0;}
	randInt = randInt % numberReady;
	for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if (randInt == 0 && processes[i].state == OS_PS_READY) {return i;}
		if (randInt == 0 && processes[i].state == OS_PS_BLOCKED) {os_getProcessSlot(i)->state = OS_PS_READY; return os_Scheduler_Random(processes,current);}
		if (processes[i].state == OS_PS_READY || processes[i].state == OS_PS_BLOCKED) {randInt--;}
	}
	return 0;
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
    // This is a presence 
	schedulingInfo.timeSlice--;
	if(schedulingInfo.timeSlice != 0 && processes[current].state == OS_PS_READY){
		return current;
	}
	ProcessID next = os_Scheduler_Even(processes,current);
	schedulingInfo.timeSlice = processes[next].priority;
	return next;
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
    // This is a presence task
	ProcessID oldest = 0;
    for(uint8_t i=1;i<MAX_NUMBER_OF_PROCESSES;i++){
		if(processes[i].state == OS_PS_READY && i != current){
			schedulingInfo.age[i] += processes[i].priority;
		}
	}
	for(uint8_t i=1;i<MAX_NUMBER_OF_PROCESSES;i++){
		if(processes[i].state == OS_PS_READY){
			if(schedulingInfo.age[oldest] < schedulingInfo.age[i] || oldest == 0){
				// The condition oldest == 0 prevents the edge-case where the only ready process has age 0
				oldest = i;
			}
			if(schedulingInfo.age[oldest] == schedulingInfo.age[i]){
				if(processes[oldest].priority < processes[i].priority){
					oldest = i;
				}
			}
		}
	}
	// A Blocked Process should only be ignored one time.
	// The Idle Process should only be returned if there are neither ready nor blocked processes 
	for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if (processes[i].state == OS_PS_BLOCKED) {
			for (uint8_t ii = i; ii < MAX_NUMBER_OF_PROCESSES; ii++) {
				if (processes[ii].state == OS_PS_BLOCKED) {os_getProcessSlot(ii)->state = OS_PS_READY;}
			}
			if (oldest == 0) {return os_Scheduler_InactiveAging(processes,current);}
		}
	}
	
	schedulingInfo.age[oldest] = processes[oldest].priority;
	return oldest;
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
	// This is a presence task
	if(processes[current].state == OS_PS_READY && current != 0){
		return current;
	}
	return os_Scheduler_Even(processes,current);
}

//initializes all ProcessQueues for the MLFQ.
void os_initSchedulingInformation() {
	for (uint8_t i = 0; i < NUMBER_OF_PROCESS_QUEUES; i++ ) {
		pqueue_init(MLFQ_getQueue(i));	
	}
}


// MultiLevelFeedbackQueue strategy
ProcessID os_Scheduler_MLFQ(Process const processes[], ProcessID current){
	ProcessID cur = 0;
	bool repeat = false;
	for (uint8_t queueID = 0; queueID < NUMBER_OF_PROCESS_QUEUES; queueID++) {
		ProcessQueue *queue = MLFQ_getQueue(queueID);
		if (queue->head >= queue->size) {os_errorPStr(PSTR("queueHead OOB in MLFQ"));}
		uint8_t oldhead = queue->head;
		for (uint8_t i = queue->tail; i != oldhead; i = (i+1)%queue->size) {
			cur = pqueue_getFirst(queue);
			if (processes[cur].state == OS_PS_UNUSED || cur == 0) {
				pqueue_dropFirst(queue);
			}
			else if (schedulingInfo.timeSliceMLFQ[cur] == 0) {
				pqueue_dropFirst(queue);
				if (processes[cur].state == OS_PS_BLOCKED) {
					os_getProcessSlot(cur)->state = OS_PS_READY;
				}
				pqueue_append(MLFQ_getQueue( queueID+1 < NUMBER_OF_PROCESS_QUEUES ? queueID+1 : NUMBER_OF_PROCESS_QUEUES - 1),cur);
				schedulingInfo.timeSliceMLFQ[cur] = MLFQ_getDefaultTimeslice(queueID+1 < NUMBER_OF_PROCESS_QUEUES ? queueID+1 : NUMBER_OF_PROCESS_QUEUES - 1);
				repeat = true;
			}
			else if (processes[cur].state == OS_PS_READY) {
				schedulingInfo.timeSliceMLFQ[cur]--;
				return cur;
			}
			else if (processes[cur].state == OS_PS_BLOCKED) {
				pqueue_dropFirst(queue);
				os_getProcessSlot(cur)->state = OS_PS_READY;
				pqueue_append(queue,cur);
				repeat = true;
			}
		}
	}
	// This flag is necessary to avoid returning the idle Process if there are ready or blocking processes left
	if (repeat) {return os_Scheduler_MLFQ(processes,current);}
	return 0;
}

// Function that removes the given ProcessID from the ProcessQueues.
void MLFQ_removePID(ProcessID pid) {
	for (uint8_t i = 0; i < NUMBER_OF_PROCESS_QUEUES; i++) {
		pqueue_removePID(MLFQ_getQueue(i),pid);
	}
}

//Returns the corresponding ProcessQueue.
ProcessQueue * MLFQ_getQueue(uint8_t queueID) {
	return &(schedulingInfo.ProcessQueues[queueID]);
}

//Returns the default number of timeslices for a specific ProcessQueue/priority class.
uint8_t MLFQ_getDefaultTimeslice(uint8_t queueID) {
	switch (queueID) {
		case 0: return 1;
		case 1: return 2;
		case 2: return 4;
		case 3: return 8;
		default: os_errorPStr(PSTR("Invalid queueID")); return 0;
	}
}

//Maps a process-priority to a priority class.
uint8_t MLFQ_MapToQueue(Priority prio) {
	switch (prio & 0b11000000) {
		case 0b11000000: return 0;
		case 0b10000000: return 1;
		case 0b01000000: return 2;
		case 0b00000000: return 3;
	}
	return 0;
}

//Initializes the given ProcessQueue with a predefined size.
void pqueue_init(ProcessQueue * queue){
	queue->head = 0;
	queue->tail = 0;
	queue->size = MAX_NUMBER_OF_PROCESSES;	
}

//Resets the given ProcessQueue.
void pqueue_reset(ProcessQueue * queue) {
	queue->head = 0;
	queue->tail = 0;
}

//Checks whether there is next a ProcessID.
bool pqueue_hasNext(const ProcessQueue *queue) {
	return (queue->head != queue->tail);
}

//Returns the first ProcessID of the given ProcessQueue.
ProcessID pqueue_getFirst(const ProcessQueue *queue) {
	if (pqueue_hasNext(queue)) {return queue->data[queue->tail];}
	return 0;
}

//Drops the first ProcessID of the given ProcessQueue.
void pqueue_dropFirst(ProcessQueue *queue) {
	if (pqueue_hasNext(queue)) {
		queue->tail = (queue->tail+1)%queue->size;
	}
}

// Appends a ProcessID to the given ProcessQueue.
void pqueue_append(ProcessQueue *queue, ProcessID pid) {
	if ((queue->head+1)%queue->size != queue->tail) {
		queue->data[queue->head] = pid;
		queue->head = (queue->head+1) % queue->size;
	} else {
		// queue is already full
		os_errorPStr(PSTR("queue_append failed"));
	}
}

// Removes a ProcessID from the given ProcessQueue.
void pqueue_removePID(ProcessQueue *queue, ProcessID pid) {
	/*for (uint8_t i = queue->tail; i != queue->head; i = (i+1)%queue->size) {
		if (queue->data[i] == pid) {
			for (uint8_t ii = i; ii != queue->tail; ii != 0 ? ii-- : ii = queue->size-1) {
				queue->data[ii] = queue->data[ii != 0 ? ii-1 :  queue->size-1];
			}
			queue->tail = (queue->tail+1)%queue->size;
		}
	}*/
	os_enterCriticalSection();
	ProcessID cur;
	uint8_t oldhead = queue->head;
	if (oldhead >= queue->size) {os_errorPStr(PSTR("queueHead OOB"));}
	for (uint8_t i = queue->tail; i != oldhead; i = (i+1)%queue->size) {
		cur = pqueue_getFirst(queue);
		pqueue_dropFirst(queue);
		if (cur != pid) {
			pqueue_append(queue,cur);
		}
	}
	os_leaveCriticalSection();
}

//Function used to determine whether there is any process ready (except the idle process)
bool isAnyProcReady(Process const processes[]) {
	for (uint8_t i = 1; i < MAX_NUMBER_OF_PROCESSES; i++) {
		if (processes[i].state == OS_PS_READY) {return true;}
	}
	return false;
}
