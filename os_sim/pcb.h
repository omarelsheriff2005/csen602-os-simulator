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
    int          programCounter;
    int          memLower;
    int          memUpper;
    int          arrivalTime;
    int          burstTime;
    int          waitTime;
    int          inMemory;
    int          queueLevel;
} PCB;

PCB*        createPCB(int pid, int lower, int upper, int arrivalTime);
const char* stateToString(ProcessState s);
void        printPCB(PCB* p);
void        syncPCBToMemory(PCB* p);

#endif
