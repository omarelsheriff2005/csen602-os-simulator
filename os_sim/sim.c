#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "sim.h"
#include "pcb.h"
#include "memory.h"
#include "queue.h"
#include "mutex.h"
#include "scheduler.h"
#include "swap.h"
#include "parser.h"

#define ARRIVAL_P1 0
#define ARRIVAL_P2 1
#define ARRIVAL_P3 4
#define PATH_BUF 512

int currentClock = 0;

PCB* allProcesses[3];
int  processCount = 0;

SimState simState;
Queue readyQ;
Queue blockedQ;
int guiMode = 0;

static int created[3];
static const char* files[3] = {
    "programs/Program1.txt",
    "programs/Program2.txt",
    "programs/Program3.txt"
};
static int arrivals[3] = {ARRIVAL_P1, ARRIVAL_P2, ARRIVAL_P3};
static char resolvedFiles[3][PATH_BUF];

static int allArrivalsHandled(void) {
    for (int i = 0; i < 3; i++) {
        if (!created[i]) return 0;
    }
    return 1;
}

static int allCreatedFinished(void) {
    if (processCount == 0) return 0;

    for (int i = 0; i < processCount; i++) {
        if (allProcesses[i] && allProcesses[i]->state != FINISHED)
            return 0;
    }

    return 1;
}

static int fileExists(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int resolveProgramPath(const char* relativePath, char* out, size_t outSize) {
    char candidate[PATH_BUF];

    if (fileExists(relativePath)) {
        snprintf(out, outSize, "%s", relativePath);
        return 1;
    }

    snprintf(candidate, sizeof(candidate), "os_sim/%s", relativePath);
    if (fileExists(candidate)) {
        snprintf(out, outSize, "%s", candidate);
        return 1;
    }

    return 0;
}

static int prepareProgramPaths(void) {
    for (int i = 0; i < 3; i++) {
        if (!resolveProgramPath(files[i], resolvedFiles[i], sizeof(resolvedFiles[i]))) {
            simFail("ERROR: Cannot locate required program file: %s", files[i]);
            return 0;
        }
    }

    return 1;
}

void simLog(const char* fmt, ...) {
    int idx;
    va_list args;
    char line[LOG_LINE_LEN];

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (simState.logCount < MAX_LOG_LINES) {
        idx = simState.logCount;
        simState.logCount++;
    } else {
        idx = simState.logHead;
        simState.logHead = (simState.logHead + 1) % MAX_LOG_LINES;
    }

    simState.logSerial++;
    strncpy(simState.log[idx], line, LOG_LINE_LEN - 1);
    simState.log[idx][LOG_LINE_LEN - 1] = '\0';

    if (strncmp(line, "ERROR", 5) == 0) {
        simState.hasError = 1;
        strncpy(simState.lastError, line, sizeof(simState.lastError) - 1);
        simState.lastError[sizeof(simState.lastError) - 1] = '\0';
    }

    printf("%s\n", line);
}

void simFail(const char* fmt, ...) {
    va_list args;
    char line[LOG_LINE_LEN];

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    simState.running = 0;
    simState.hasError = 1;
    strncpy(simState.lastError, line, sizeof(simState.lastError) - 1);
    simState.lastError[sizeof(simState.lastError) - 1] = '\0';
    simLog("%s", line);
}

void simEnqueueReady(PCB* p) {
    if (!p) return;

    p->state = READY;
    if (p->inMemory)
        syncPCBToMemory(p);

    if (strcmp(simState.schedulerMode, "mlfq") == 0) {
        if (p->queueLevel < 0) p->queueLevel = 0;
        if (p->queueLevel > 3) p->queueLevel = 3;
        enqueue(&mlfqQueues[p->queueLevel], p);
    } else {
        enqueue(&readyQ, p);
    }
}

void simMarkFinished(PCB* p) {
    char swapFile[32];

    if (!p) return;

    p->state = FINISHED;
    if (p->inMemory) {
        freeBlock(p->memLower, p->memUpper);
        p->inMemory = 0;
    }

    snprintf(swapFile, sizeof(swapFile), "swap_pid%d.txt", p->pid);
    remove(swapFile);

    simLog("[SIM] P%d FINISHED", p->pid);
}

void simSetCurrentInstruction(PCB* p, const char* instr) {
    simState.currentlyRunning = p;
    if (instr) {
        strncpy(simState.currentInstr, instr, sizeof(simState.currentInstr) - 1);
        simState.currentInstr[sizeof(simState.currentInstr) - 1] = '\0';
    } else {
        simState.currentInstr[0] = '\0';
    }
}

void simClearCurrentInstruction(void) {
    simState.currentlyRunning = NULL;
    simState.currentInstr[0] = '\0';
}

static int createProcess(int pid, const char* filepath, int arrivalTime) {
    FILE* f = fopen(filepath, "r");
    int lineCount = 0;
    int lower;
    int blockSize;
    int upper;
    int n;
    PCB* p;
    char buf[256];

    if (!f) {
        simFail("ERROR: Cannot open %s", filepath);
        return 0;
    }

    while (fgets(buf, sizeof(buf), f)) {
        if (strlen(buf) > 1) lineCount++;
    }
    fclose(f);

    blockSize = PCB_SIZE + VAR_SIZE + lineCount;
    lower = findFreeBlockWithSwap(allProcesses, processCount, blockSize, -1);
    if (lower == -1) {
        simFail("ERROR: Cannot allocate memory for P%d", pid);
        return 0;
    }

    upper = lower + blockSize - 1;
    p = createPCB(pid, lower, upper, arrivalTime);
    allProcesses[processCount++] = p;

    syncPCBToMemory(p);
    for (int i = 0; i < VAR_SIZE; i++)
        memWrite(lower + VAR_OFFSET + i, "");

    n = loadProgram((char*)filepath, p);
    if (n < 0) {
        freeBlock(lower, upper);
        free(p);
        allProcesses[--processCount] = NULL;
        simFail("ERROR: Failed to load program for P%d", pid);
        return 0;
    }

    p->burstTime = n;
    p->queueLevel = 0;
    simEnqueueReady(p);

    simLog("[CREATE] Process %d | Memory: %d-%d | Instructions: %d",
           pid, lower, upper, n);
    return 1;
}

void simInit(const char* mode) {
    memset(&simState, 0, sizeof(SimState));
    strncpy(simState.schedulerMode, mode, sizeof(simState.schedulerMode) - 1);
    simState.schedulerMode[sizeof(simState.schedulerMode) - 1] = '\0';
    simState.running = 1;

    currentClock = 0;
    processCount = 0;
    for (int i = 0; i < 3; i++) {
        created[i] = 0;
        allProcesses[i] = NULL;
    }

    initMemory();
    initMutexes();
    initQueue(&readyQ);
    initQueue(&blockedQ);

    if (strcmp(mode, "mlfq") == 0)
        initMLFQ();

    if (!prepareProgramPaths())
        return;

    simLog("Simulation initialised with scheduler: %s", mode);
}

void simReset(const char* mode) {
    simShutdown();
    simInit(mode);
}

void simProvideInput(const char* value) {
    PCB* p;
    int nextAddr;

    if (!simState.waitingForInput || !simState.inputProcess) return;

    p = simState.inputProcess;
    setVar(p, simState.inputVarName, (char*)value);
    simLog("[INPUT] P%d: %s = %s", p->pid, simState.inputVarName, value);

    removeById(&blockedQ, p->pid);
    p->programCounter++;
    nextAddr = p->memLower + INSTR_OFFSET + p->programCounter;

    if (simState.inputAutoRelease)
        semSignal("userInput", &readyQ, &blockedQ);

    if (nextAddr > p->memUpper || memRead(nextAddr) == NULL) {
        simMarkFinished(p);
    } else {
        simEnqueueReady(p);
    }

    simState.waitingForInput = 0;
    simState.inputProcess = NULL;
    simState.inputAutoRelease = 0;
    simState.inputVarName[0] = '\0';
    simState.inputPrompt[0] = '\0';
}

void simShutdown(void) {
    char swapFile[32];

    for (int i = 0; i < processCount; i++) {
        free(allProcesses[i]);
        allProcesses[i] = NULL;
    }
    processCount = 0;

    initMemory();

    for (int i = 0; i < 3; i++) {
        snprintf(swapFile, sizeof(swapFile), "swap_pid%d.txt", i + 1);
        remove(swapFile);
    }
}

int simStep(void) {
    if (!simState.running) return 0;
    if (simState.waitingForInput) return 1;

    simState.clock = currentClock;
    simLog("========== Clock Cycle %d ==========", currentClock);

    for (int i = 0; i < 3; i++) {
        if (!created[i] && currentClock == arrivals[i]) {
            if (!createProcess(i + 1, resolvedFiles[i], arrivals[i]))
                return 0;
            created[i] = 1;
        }
    }

    simClearCurrentInstruction();
    if (strcmp(simState.schedulerMode, "hrrn") == 0)
        scheduleHRRN(&readyQ, &blockedQ);
    else if (strcmp(simState.schedulerMode, "mlfq") == 0)
        scheduleMLFQ(&blockedQ);
    else
        scheduleRR(&readyQ, &blockedQ);

    printMemory();
    printMutexState();

    if (allArrivalsHandled() && allCreatedFinished()) {
        simLog("[SIM] All processes finished at clock %d.", currentClock);
        simState.running = 0;
        return 0;
    }

    currentClock++;
    if (currentClock > 1000) {
        simFail("ERROR: Exceeded 1000 cycles - possible deadlock");
        return 0;
    }

    return 1;
}
