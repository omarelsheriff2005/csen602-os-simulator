#ifndef MUTEX_H
#define MUTEX_H

#include "queue.h"

typedef struct {
    char  name[20];
    int   locked;
    int   ownerPid;
    Queue blockedQueue;
} Mutex;


extern Mutex mutexUserInput;
extern Mutex mutexUserOutput;
extern Mutex mutexFile;

void   initMutexes();
Mutex* getMutex(char* name);
void   semWait(char* name, PCB* p, Queue* readyQ, Queue* blockedQ);
void   semSignal(char* name, Queue* readyQ, Queue* blockedQ);
void   printMutexState();

#endif
