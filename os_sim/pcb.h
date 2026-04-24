#ifndef PCB_H
#define PCB_H

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} ProcessState;

typedef struct {
    int          pid;
    ProcessState state;
    int          programCounter;  /* index of next instruction to execute */
    int          memLower;        /* start of this process's memory block */
    int          memUpper;        /* end of this process's memory block (inclusive) */
    int          arrivalTime;
    int          burstTime;       /* total instructions remaining */
    int          waitTime;        /* incremented each tick while in ready queue */
    int          inMemory;        /* 1 = loaded in RAM, 0 = swapped to disk */
    int          queueLevel;      /* for MLFQ only (0-3) */
} PCB;

PCB*        createPCB(int pid, int lower, int upper, int arrivalTime);
const char* stateToString(ProcessState s);
void        printPCB(PCB* p);
void        syncPCBToMemory(PCB* p);

#endif
