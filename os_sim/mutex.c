#include <stdio.h>
#include <string.h>
#include "mutex.h"
#include "pcb.h"
#include "queue.h"
#include "memory.h"
#include "sim.h"

Mutex mutexUserInput  = { "userInput",  0, -1, {0} };
Mutex mutexUserOutput = { "userOutput", 0, -1, {0} };
Mutex mutexFile       = { "file",       0, -1, {0} };

void initMutexes() {
    initQueue(&mutexUserInput.blockedQueue);
    initQueue(&mutexUserOutput.blockedQueue);
    initQueue(&mutexFile.blockedQueue);
}

Mutex* getMutex(char* name) {
    if (strcmp(name, "userInput") == 0) return &mutexUserInput;
    if (strcmp(name, "userOutput") == 0) return &mutexUserOutput;
    if (strcmp(name, "file") == 0) return &mutexFile;

    simLog("ERROR: Unknown mutex '%s'", name);
    return NULL;
}

void semWait(char* name, PCB* p, Queue* readyQ, Queue* blockedQ) {
    Mutex* m = getMutex(name);
    if (!m) return;

    if (!m->locked) {
        m->locked = 1;
        m->ownerPid = p->pid;
        simLog("[MUTEX] Process %d acquired '%s'", p->pid, name);
    } else {
        p->state = BLOCKED;
        syncPCBToMemory(p);
        removeById(readyQ, p->pid);
        removeById(blockedQ, p->pid);
        enqueue(&m->blockedQueue, p);
        enqueue(blockedQ, p);
        simLog("[MUTEX] Process %d BLOCKED on '%s' (held by P%d)",
               p->pid, name, m->ownerPid);
    }
}

void semSignal(char* name, Queue* readyQ, Queue* blockedQ) {
    Mutex* m = getMutex(name);
    (void)readyQ;
    if (!m) return;

    if (!m->locked) {
        simLog("WARNING: semSignal on unlocked mutex '%s'", name);
        return;
    }

    simLog("[MUTEX] Process %d released '%s'", m->ownerPid, name);

    if (!isEmpty(&m->blockedQueue)) {
        PCB* next = dequeue(&m->blockedQueue);
        int nextAddr;

        removeById(blockedQ, next->pid);
        next->programCounter++;
        m->locked = 1;
        m->ownerPid = next->pid;

        nextAddr = next->memLower + INSTR_OFFSET + next->programCounter;
        if (nextAddr > next->memUpper || memRead(nextAddr) == NULL) {
            simMarkFinished(next);
        } else {
            simEnqueueReady(next);
            simLog("[MUTEX] Process %d UNBLOCKED and acquired '%s'", next->pid, name);
        }
    } else {
        m->locked = 0;
        m->ownerPid = -1;
    }
}

void printMutexState() {
    simLog("[MUTEX STATE]");
    simLog("  userInput  : %s (owner: P%d)",
           mutexUserInput.locked ? "LOCKED" : "free", mutexUserInput.ownerPid);
    simLog("  userOutput : %s (owner: P%d)",
           mutexUserOutput.locked ? "LOCKED" : "free", mutexUserOutput.ownerPid);
    simLog("  file       : %s (owner: P%d)",
           mutexFile.locked ? "LOCKED" : "free", mutexFile.ownerPid);
}
