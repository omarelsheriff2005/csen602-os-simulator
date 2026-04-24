#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcb.h"
#include "memory.h"

PCB* createPCB(int pid, int lower, int upper, int arrivalTime) {
    PCB* p = malloc(sizeof(PCB));
    p->pid            = pid;
    p->state          = READY;
    p->programCounter = 0;
    p->memLower       = lower;
    p->memUpper       = upper;
    p->arrivalTime    = arrivalTime;
    p->burstTime      = 0;
    p->waitTime       = 0;
    p->inMemory       = 1;
    p->queueLevel     = 0;
    return p;
}

const char* stateToString(ProcessState s) {
    switch (s) {
        case READY:    return "READY";
        case RUNNING:  return "RUNNING";
        case BLOCKED:  return "BLOCKED";
        case FINISHED: return "FINISHED";
        default:       return "UNKNOWN";
    }
}

void printPCB(PCB* p) {
    printf("PCB[PID=%d | State=%-8s | PC=%d | Mem=%d-%d | Wait=%d | Burst=%d]\n",
           p->pid, stateToString(p->state), p->programCounter,
           p->memLower, p->memUpper, p->waitTime, p->burstTime);
}

void syncPCBToMemory(PCB* p) {
    char buf[64];

    sprintf(buf, "PID=%d", p->pid);
    memWrite(p->memLower + PCB_OFFSET_PID, buf);

    sprintf(buf, "STATE=%s", stateToString(p->state));
    memWrite(p->memLower + PCB_OFFSET_STATE, buf);

    sprintf(buf, "PC=%d", p->programCounter);
    memWrite(p->memLower + PCB_OFFSET_PC, buf);

    sprintf(buf, "BOUNDS=%d-%d", p->memLower, p->memUpper);
    memWrite(p->memLower + PCB_OFFSET_BOUNDS, buf);
}
