#ifndef MUTEX_H
#define MUTEX_H

#include "queue.h"

typedef struct {
    char  name[20];       /* "userInput", "userOutput", or "file" */
    int   locked;         /* 0 = free, 1 = held */
    int   ownerPid;       /* PID of the holding process (-1 if free) */
    Queue blockedQueue;   /* processes waiting for THIS mutex */
} Mutex;

/* Three global mutexes — declared in mutex.c, extern'd here */
extern Mutex mutexUserInput;
extern Mutex mutexUserOutput;
extern Mutex mutexFile;

void   initMutexes();
Mutex* getMutex(char* name);
void   semWait(char* name, PCB* p, Queue* readyQ, Queue* blockedQ);
void   semSignal(char* name, Queue* readyQ, Queue* blockedQ);
void   printMutexState();

#endif
