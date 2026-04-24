#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "queue.h"
#include "pcb.h"

#define RR_QUANTUM 2   /* instructions per time slice — change here for evaluation */

/* Global process table — defined in main.c, used by scheduler and swap */
extern PCB* allProcesses[];
extern int  processCount;

void  scheduleRR(Queue* readyQ, Queue* blockedQ);
void  scheduleHRRN(Queue* readyQ, Queue* blockedQ);

/* MLFQ (bonus) */
extern Queue mlfqQueues[4];
void  initMLFQ();
void  mlfqEnqueue(PCB* p);
void  scheduleMLFQ(Queue* blockedQ);

float calcResponseRatio(PCB* p);
int   getBlockSize(PCB* p);

#endif
