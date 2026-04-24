# CSEN 602 — OS Simulation Project
## Complete Implementation Guide (C Language)
**Due:** 18 April 2026 | **Language:** C | **Team size:** 5

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [File Structure](#2-file-structure)
3. [Build System (Makefile)](#3-build-system-makefile)
4. [Data Structures](#4-data-structures)
   - 4.1 [PCB](#41-pcb)
   - 4.2 [Memory Array](#42-memory-array)
   - 4.3 [Queue](#43-queue)
   - 4.4 [Mutex](#44-mutex)
5. [Memory Management](#5-memory-management)
   - 5.1 [Layout Convention](#51-memory-layout-convention)
   - 5.2 [Core Functions](#52-core-memory-functions)
   - 5.3 [Disk Swapping](#53-disk-swapping)
6. [Process Control Block (PCB)](#6-process-control-block)
7. [Parser & Interpreter](#7-parser--interpreter)
   - 7.1 [File Loader](#71-file-loader)
   - 7.2 [Variable Helpers](#72-variable-helpers)
   - 7.3 [Instruction Executor](#73-instruction-executor)
8. [System Calls](#8-system-calls)
9. [Mutexes & Mutual Exclusion](#9-mutexes--mutual-exclusion)
   - 9.1 [Mutex Struct](#91-mutex-struct)
   - 9.2 [semWait](#92-semwait)
   - 9.3 [semSignal](#93-semsignal)
10. [Scheduler](#10-scheduler)
    - 10.1 [Round Robin (RR)](#101-round-robin-rr)
    - 10.2 [HRRN](#102-highest-response-ratio-next-hrrn)
    - 10.3 [MLFQ (Bonus)](#103-multi-level-feedback-queue-mlfq--bonus)
11. [Process Creation](#11-process-creation)
12. [Main Simulation Loop](#12-main-simulation-loop)
13. [The Three Program Files](#13-the-three-program-files)
14. [Required Output Format](#14-required-output-format)
15. [GUI (Optional Bonus)](#15-gui-optional-bonus)
16. [Testing Checklist](#16-testing-checklist)
17. [Work Distribution Summary](#17-work-distribution-summary)
18. [Common Pitfalls](#18-common-pitfalls)

---

## 1. Project Overview

You are building a **simulated operating system** in C. The simulator:

- Reads `.txt` program files and executes them as processes
- Manages a **40-word memory array** shared by all processes
- Implements **mutexes** for mutual exclusion over 3 shared resources
- Runs a **scheduler** using HRRN and Round Robin algorithms
- Supports **disk swapping** when memory is full
- Prints the state of queues, memory, and execution at every clock cycle

The three programs arrive at fixed times:

| Process | Arrival Time |
|---------|-------------|
| P1      | Clock 0     |
| P2      | Clock 1     |
| P3      | Clock 4     |

These arrival times are **subject to change during evaluation** — store them as constants, not magic numbers.

---

## 2. File Structure

```
os_sim/
├── Makefile
├── main.c              ← simulation loop, process arrival
├── pcb.h / pcb.c       ← PCB struct, state enum, helpers
├── memory.h / memory.c ← 40-word array, read/write, free block
├── queue.h / queue.c   ← generic linked-list queue
├── parser.h / parser.c ← file loader, variable helpers, instruction executor
├── syscalls.h / syscalls.c ← all 6 system calls
├── mutex.h / mutex.c   ← Mutex struct, semWait, semSignal
├── scheduler.h / scheduler.c ← RR, HRRN, MLFQ
├── swap.h / swap.c     ← swapOut, swapIn, chooseVictim
└── programs/
    ├── Program1.txt
    ├── Program2.txt
    └── Program3.txt
```

**Rule:** Never `#include` a `.c` file. Only include `.h` files. Each `.h` file must have an include guard:

```c
#ifndef MEMORY_H
#define MEMORY_H
// declarations here
#endif
```

---

## 3. Build System (Makefile)

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -fsanitize=address

SRCS = main.c pcb.c memory.c queue.c parser.c syscalls.c mutex.c scheduler.c swap.c
OBJS = $(SRCS:.c=.o)
TARGET = os_sim

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) swap_pid*.txt

.PHONY: all clean
```

**Usage:**
```bash
make              # build
./os_sim rr       # run with Round Robin
./os_sim hrrn     # run with HRRN
./os_sim mlfq     # run with MLFQ (bonus)
make clean        # remove build artifacts
```

The `-fsanitize=address` flag catches memory bugs during development. Remove it for the final submission if it causes issues on the evaluation machine.

---

## 4. Data Structures

### 4.1 PCB

```c
// pcb.h

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    FINISHED
} ProcessState;

typedef struct {
    int          pid;
    ProcessState state;
    int          programCounter;  // index of next instruction to execute
    int          memLower;        // start of this process's memory block
    int          memUpper;        // end of this process's memory block (inclusive)
    int          arrivalTime;
    int          burstTime;       // total instructions remaining
    int          waitTime;        // incremented each tick while in ready queue
    int          inMemory;        // 1 = loaded in RAM, 0 = swapped to disk
    int          queueLevel;      // for MLFQ only (0–3)
} PCB;
```

**Key rule:** `programCounter` is a relative index (0 = first instruction of this process), not an absolute memory address. To get the absolute address of the current instruction:

```c
int instrAddr = p->memLower + PCB_SIZE + VAR_SIZE + p->programCounter;
// PCB_SIZE = 4 words (pid, state, PC, bounds)
// VAR_SIZE = 3 words (variable slots)
```

---

### 4.2 Memory Array

```c
// memory.h

#define MEM_SIZE   40
#define PCB_SIZE    4   // words reserved for PCB fields per process
#define VAR_SIZE    3   // words reserved for variables per process

extern char* memory[MEM_SIZE];
```

Each element of `memory[]` is a `char*` that points to a heap-allocated string or is `NULL` (free slot).

Examples of what a slot might hold:
- `"PID=1"`
- `"STATE=READY"`
- `"PC=3"`
- `"BOUNDS=7-19"`
- `"x=42"`
- `"filename=out.txt"`
- `"assign a 5"`  ← a raw instruction line

---

### 4.3 Queue

```c
// queue.h

typedef struct QNode {
    PCB*          pcb;
    struct QNode* next;
} QNode;

typedef struct {
    QNode* head;
    QNode* tail;
    int    size;
} Queue;

void  initQueue(Queue* q);
void  enqueue(Queue* q, PCB* p);
PCB*  dequeue(Queue* q);
PCB*  peek(Queue* q);
int   removeById(Queue* q, int pid);   // returns 1 if found and removed
int   isEmpty(Queue* q);
void  printQueue(Queue* q, char* label);
```

**Implementation notes:**
- `enqueue` always adds to the tail.
- `dequeue` always removes from the head and returns the PCB pointer (or NULL if empty).
- `removeById` walks the list and unlinks the matching node — used when a process blocks mid-queue.
- `printQueue` prints something like: `Ready Queue: [P1, P2, P3]`

---

### 4.4 Mutex

```c
// mutex.h

typedef struct {
    char  name[20];       // "userInput", "userOutput", or "file"
    int   locked;         // 0 = free, 1 = held
    int   ownerPid;       // PID of the holding process (-1 if free)
    Queue blockedQueue;   // processes waiting for THIS mutex
} Mutex;

// Three global mutexes — declared in mutex.c, extern'd in mutex.h
extern Mutex mutexUserInput;
extern Mutex mutexUserOutput;
extern Mutex mutexFile;

void   initMutexes();
Mutex* getMutex(char* name);
void   semWait(char* name, PCB* p, Queue* readyQ, Queue* blockedQ);
void   semSignal(char* name, Queue* readyQ, Queue* blockedQ);
void   printMutexState();
```

---

## 5. Memory Management

### 5.1 Memory Layout Convention

Every process occupies a **contiguous block** of memory. The block is divided into three sections:

```
[ memLower ]
  Slot +0   →  "PID=<n>"
  Slot +1   →  "STATE=READY"
  Slot +2   →  "PC=0"
  Slot +3   →  "BOUNDS=<lower>-<upper>"
  Slot +4   →  variable slot 1  (e.g. "x=5")
  Slot +5   →  variable slot 2  (e.g. "filename=out.txt")
  Slot +6   →  variable slot 3  (e.g. "data=hello")
  Slot +7   →  instruction line 0
  Slot +8   →  instruction line 1
  ...
[ memUpper ]
```

So for a process with N instruction lines:
- **Block size** = PCB_SIZE (4) + VAR_SIZE (3) + N = 7 + N words
- For a 5-line program: block size = 12 words

Define the offsets as constants so every file uses the same values:

```c
#define PCB_OFFSET_PID    0
#define PCB_OFFSET_STATE  1
#define PCB_OFFSET_PC     2
#define PCB_OFFSET_BOUNDS 3
#define VAR_OFFSET        4   // first variable slot (relative to memLower)
#define INSTR_OFFSET      7   // first instruction slot (relative to memLower)
```

---

### 5.2 Core Memory Functions

```c
// memory.c

char* memory[MEM_SIZE];

void initMemory() {
    for (int i = 0; i < MEM_SIZE; i++)
        memory[i] = NULL;
}

// Write a string value into a memory slot (with bounds check)
void memWrite(int addr, char* val) {
    if (addr < 0 || addr >= MEM_SIZE) {
        fprintf(stderr, "ERROR: memWrite out of bounds at %d\n", addr);
        return;
    }
    free(memory[addr]);                   // free old value if present
    memory[addr] = strdup(val);           // heap-allocate a copy
}

// Read a string value from a memory slot
char* memRead(int addr) {
    if (addr < 0 || addr >= MEM_SIZE) {
        fprintf(stderr, "ERROR: memRead out of bounds at %d\n", addr);
        return NULL;
    }
    return memory[addr];
}

// Find the first contiguous block of 'size' free slots
// Returns the start index, or -1 if no block found
int findFreeBlock(int size) {
    int count = 0, start = 0;
    for (int i = 0; i < MEM_SIZE; i++) {
        if (memory[i] == NULL) {
            if (count == 0) start = i;
            count++;
            if (count == size) return start;
        } else {
            count = 0;
        }
    }
    return -1;
}

// Free a process's memory block
void freeBlock(int lower, int upper) {
    for (int i = lower; i <= upper; i++) {
        free(memory[i]);
        memory[i] = NULL;
    }
}

// Print all 40 memory slots in a readable table
void printMemory() {
    printf("\n+-------+-----------------------------+\n");
    printf("| Addr  | Value                       |\n");
    printf("+-------+-----------------------------+\n");
    for (int i = 0; i < MEM_SIZE; i++) {
        if (memory[i] != NULL)
            printf("| %5d | %-27s |\n", i, memory[i]);
        else
            printf("| %5d | <empty>                     |\n", i);
    }
    printf("+-------+-----------------------------+\n");
}
```

---

### 5.3 Disk Swapping

A process is swapped out to free memory for a new process. The swapped process **stays in the ready queue** — it just cannot execute until it is swapped back in.

**Swap file format** (`swap_pid2.txt`):
```
PID=2
STATE=READY
PC=3
BOUNDS=12-23
x=5
filename=
data=
assign a 10
assign b 20
semWait userOutput
printFromTo a b
semSignal userOutput
```
One memory word per line, in order from `memLower` to `memUpper`.

```c
// swap.c

void swapOut(PCB* p) {
    char filename[32];
    sprintf(filename, "swap_pid%d.txt", p->pid);
    FILE* f = fopen(filename, "w");
    if (!f) { perror("swapOut fopen"); return; }

    for (int i = p->memLower; i <= p->memUpper; i++) {
        if (memory[i] != NULL)
            fprintf(f, "%s\n", memory[i]);
        else
            fprintf(f, "\n");
    }
    fclose(f);

    freeBlock(p->memLower, p->memUpper);
    p->inMemory = 0;
    printf("[SWAP] Process %d swapped OUT to %s\n", p->pid, filename);
}

void swapIn(PCB* p, int newLower) {
    char filename[32];
    sprintf(filename, "swap_pid%d.txt", p->pid);
    FILE* f = fopen(filename, "r");
    if (!f) { perror("swapIn fopen"); return; }

    int blockSize = p->memUpper - p->memLower + 1;
    char line[256];
    for (int i = 0; i < blockSize; i++) {
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';  // strip newline
            memWrite(newLower + i, line);
        }
    }
    fclose(f);
    remove(filename);

    int oldSize = p->memUpper - p->memLower;
    p->memLower  = newLower;
    p->memUpper  = newLower + oldSize;
    p->inMemory  = 1;

    // Update the BOUNDS word in memory to reflect new location
    char bounds[32];
    sprintf(bounds, "BOUNDS=%d-%d", p->memLower, p->memUpper);
    memWrite(p->memLower + PCB_OFFSET_BOUNDS, bounds);

    printf("[SWAP] Process %d swapped IN at addresses %d-%d\n",
           p->pid, p->memLower, p->memUpper);
}

// Choose which process to evict — lowest PID not currently running
PCB* chooseVictim(PCB** allProcesses, int count, int runningPid) {
    PCB* victim = NULL;
    for (int i = 0; i < count; i++) {
        PCB* p = allProcesses[i];
        if (p->inMemory && p->pid != runningPid && p->state != FINISHED) {
            if (victim == NULL || p->pid < victim->pid)
                victim = p;
        }
    }
    return victim;
}

// Call this before executing any process
void ensureInMemory(PCB* p, PCB** allProcesses, int count, int blockSize) {
    if (p->inMemory) return;

    int lower = findFreeBlock(blockSize);
    if (lower == -1) {
        PCB* victim = chooseVictim(allProcesses, count, p->pid);
        if (victim) swapOut(victim);
        lower = findFreeBlock(blockSize);
    }
    if (lower != -1)
        swapIn(p, lower);
    else
        fprintf(stderr, "ERROR: Cannot swap in process %d — no memory\n", p->pid);
}
```

---

## 6. Process Control Block

```c
// pcb.c

PCB* createPCB(int pid, int lower, int upper, int arrivalTime) {
    PCB* p = malloc(sizeof(PCB));
    p->pid            = pid;
    p->state          = READY;
    p->programCounter = 0;
    p->memLower       = lower;
    p->memUpper       = upper;
    p->arrivalTime    = arrivalTime;
    p->burstTime      = 0;     // set after counting instruction lines
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

// Sync in-memory PCB state fields back to the memory array
// Call this whenever state or PC changes
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
```

---

## 7. Parser & Interpreter

### 7.1 File Loader

```c
// parser.c

int loadProgram(char* filepath, PCB* p) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open program file: %s\n", filepath);
        return -1;
    }

    char line[256];
    int  instrIndex = 0;
    int  instrBase  = p->memLower + INSTR_OFFSET;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';  // strip newline
        if (strlen(line) == 0) continue;  // skip blank lines

        int addr = instrBase + instrIndex;
        if (addr > p->memUpper) {
            fprintf(stderr, "ERROR: Program too large for allocated memory\n");
            fclose(f);
            return -1;
        }
        memWrite(addr, line);
        instrIndex++;
    }
    fclose(f);

    p->burstTime = instrIndex;  // total instructions = burst time estimate
    return instrIndex;
}
```

---

### 7.2 Variable Helpers

Each process has 3 variable slots starting at `memLower + VAR_OFFSET`.
Variables are stored as `"name=value"` strings.

```c
// Set or update a variable in the process's variable slots
void setVar(PCB* p, char* name, char* value) {
    int base = p->memLower + VAR_OFFSET;
    char key[128];

    // Check if variable already exists — update it
    for (int i = 0; i < VAR_SIZE; i++) {
        char* slot = memRead(base + i);
        if (slot == NULL) continue;
        sscanf(slot, "%[^=]", key);
        if (strcmp(key, name) == 0) {
            char newVal[256];
            sprintf(newVal, "%s=%s", name, value);
            memWrite(base + i, newVal);
            return;
        }
    }

    // Find an empty slot and create new variable
    for (int i = 0; i < VAR_SIZE; i++) {
        if (memRead(base + i) == NULL) {
            char newVal[256];
            sprintf(newVal, "%s=%s", name, value);
            memWrite(base + i, newVal);
            return;
        }
    }
    fprintf(stderr, "ERROR: No variable slots available for process %d\n", p->pid);
}

// Get the value of a variable (returns NULL if not found)
char* getVar(PCB* p, char* name) {
    int base = p->memLower + VAR_OFFSET;
    char key[128];
    static char value[256];

    for (int i = 0; i < VAR_SIZE; i++) {
        char* slot = memRead(base + i);
        if (slot == NULL) continue;
        if (sscanf(slot, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, name) == 0)
                return value;
        }
    }
    return NULL;
}

// Resolve an argument: if it's a variable name, return its value.
// Otherwise return the argument itself (literal string or integer).
char* resolveArg(PCB* p, char* arg) {
    char* val = getVar(p, arg);
    return (val != NULL) ? val : arg;
}
```

---

### 7.3 Instruction Executor

```c
// Execute ONE instruction for process p.
// Returns 1 if instruction executed, 0 if process is finished or blocked.
int executeInstruction(PCB* p, Queue* readyQ, Queue* blockedQ) {
    int instrAddr = p->memLower + INSTR_OFFSET + p->programCounter;

    if (instrAddr > p->memUpper || memRead(instrAddr) == NULL) {
        p->state = FINISHED;
        syncPCBToMemory(p);
        printf("[P%d] FINISHED\n", p->pid);
        return 0;
    }

    // Copy the instruction line so strtok doesn't modify memory
    char instr[256];
    strncpy(instr, memRead(instrAddr), sizeof(instr));
    instr[sizeof(instr)-1] = '\0';

    printf("[P%d | Clock %d] Executing: %s\n", p->pid, currentClock, instr);

    char* cmd  = strtok(instr, " ");
    char* arg1 = strtok(NULL, " ");
    char* arg2 = strtok(NULL, " ");

    if (strcmp(cmd, "print") == 0) {
        semWait("userOutput", p, readyQ, blockedQ);
        if (p->state != BLOCKED) {
            sysPrint(resolveArg(p, arg1));
            semSignal("userOutput", readyQ, blockedQ);
        }
    }
    else if (strcmp(cmd, "assign") == 0) {
        if (strcmp(arg2, "input") == 0) {
            semWait("userInput", p, readyQ, blockedQ);
            if (p->state != BLOCKED) {
                char* val = sysGetInput();
                setVar(p, arg1, val);
                semSignal("userInput", readyQ, blockedQ);
            }
        } else {
            setVar(p, arg1, resolveArg(p, arg2));
        }
    }
    else if (strcmp(cmd, "writeFile") == 0) {
        // semWait/Signal are expected to appear explicitly in the program file
        sysWriteFile(resolveArg(p, arg1), resolveArg(p, arg2));
    }
    else if (strcmp(cmd, "readFile") == 0) {
        char* content = sysReadFile(resolveArg(p, arg1));
        if (content) { sysPrint(content); free(content); }
    }
    else if (strcmp(cmd, "printFromTo") == 0) {
        int from = atoi(resolveArg(p, arg1));
        int to   = atoi(resolveArg(p, arg2));
        for (int n = from; n <= to; n++)
            printf("%d\n", n);
    }
    else if (strcmp(cmd, "semWait") == 0) {
        semWait(arg1, p, readyQ, blockedQ);
    }
    else if (strcmp(cmd, "semSignal") == 0) {
        semSignal(arg1, readyQ, blockedQ);
    }
    else {
        fprintf(stderr, "ERROR: Unknown instruction '%s'\n", cmd);
    }

    if (p->state != BLOCKED) {
        p->programCounter++;
        syncPCBToMemory(p);
        // Check if this was the last instruction
        int nextAddr = p->memLower + INSTR_OFFSET + p->programCounter;
        if (nextAddr > p->memUpper || memRead(nextAddr) == NULL) {
            p->state = FINISHED;
            syncPCBToMemory(p);
            printf("[P%d] FINISHED\n", p->pid);
            return 0;
        }
    }
    return 1;
}
```

---

## 8. System Calls

```c
// syscalls.c

void sysPrint(char* val) {
    printf("OUTPUT: %s\n", val);
}

char* sysGetInput() {
    printf("Please enter a value: ");
    static char buf[256];
    if (fgets(buf, sizeof(buf), stdin))
        buf[strcspn(buf, "\n")] = '\0';
    return buf;
}

void sysWriteFile(char* filename, char* data) {
    FILE* f = fopen(filename, "w");
    if (!f) { perror("sysWriteFile"); return; }
    fputs(data, f);
    fputc('\n', f);
    fclose(f);
    printf("[SYS] Wrote to file: %s\n", filename);
}

char* sysReadFile(char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "[SYS] File not found: %s\n", filename);
        return NULL;
    }
    // Read entire file into heap-allocated string
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;  // caller must free()
}

// Thin wrappers exposing memory as a system call
char* sysMemRead(int addr)             { return memRead(addr); }
void  sysMemWrite(int addr, char* val) { memWrite(addr, val);  }
```

---

## 9. Mutexes & Mutual Exclusion

### 9.1 Mutex Struct

```c
// mutex.c

Mutex mutexUserInput  = { "userInput",  0, -1 };
Mutex mutexUserOutput = { "userOutput", 0, -1 };
Mutex mutexFile       = { "file",       0, -1 };

void initMutexes() {
    initQueue(&mutexUserInput.blockedQueue);
    initQueue(&mutexUserOutput.blockedQueue);
    initQueue(&mutexFile.blockedQueue);
}

Mutex* getMutex(char* name) {
    if (strcmp(name, "userInput")  == 0) return &mutexUserInput;
    if (strcmp(name, "userOutput") == 0) return &mutexUserOutput;
    if (strcmp(name, "file")       == 0) return &mutexFile;
    fprintf(stderr, "ERROR: Unknown mutex '%s'\n", name);
    return NULL;
}
```

---

### 9.2 semWait

```c
void semWait(char* name, PCB* p, Queue* readyQ, Queue* blockedQ) {
    Mutex* m = getMutex(name);
    if (!m) return;

    if (!m->locked) {
        // Resource is free — acquire it
        m->locked   = 1;
        m->ownerPid = p->pid;
        printf("[MUTEX] Process %d acquired '%s'\n", p->pid, name);
    } else {
        // Resource is held — block this process
        p->state = BLOCKED;
        syncPCBToMemory(p);
        removeById(readyQ, p->pid);
        enqueue(&m->blockedQueue, p);
        enqueue(blockedQ, p);
        printf("[MUTEX] Process %d BLOCKED on '%s' (held by P%d)\n",
               p->pid, name, m->ownerPid);
    }
}
```

---

### 9.3 semSignal

```c
void semSignal(char* name, Queue* readyQ, Queue* blockedQ) {
    Mutex* m = getMutex(name);
    if (!m) return;

    if (!m->locked) {
        fprintf(stderr, "WARNING: semSignal on unlocked mutex '%s'\n", name);
        return;
    }

    printf("[MUTEX] Process %d released '%s'\n", m->ownerPid, name);

    if (!isEmpty(&m->blockedQueue)) {
        // Hand the mutex directly to the next waiting process
        PCB* next = dequeue(&m->blockedQueue);
        removeById(blockedQ, next->pid);
        next->state = READY;
        m->ownerPid = next->pid;
        enqueue(readyQ, next);
        syncPCBToMemory(next);
        printf("[MUTEX] Process %d UNBLOCKED, acquired '%s'\n", next->pid, name);
    } else {
        // No one waiting — release completely
        m->locked   = 0;
        m->ownerPid = -1;
    }
}

void printMutexState() {
    printf("[MUTEX STATE]\n");
    printf("  userInput  : %s (owner: P%d)\n",
           mutexUserInput.locked  ? "LOCKED" : "free", mutexUserInput.ownerPid);
    printf("  userOutput : %s (owner: P%d)\n",
           mutexUserOutput.locked ? "LOCKED" : "free", mutexUserOutput.ownerPid);
    printf("  file       : %s (owner: P%d)\n",
           mutexFile.locked       ? "LOCKED" : "free", mutexFile.ownerPid);
}
```

---

## 10. Scheduler

### 10.1 Round Robin (RR)

```c
// scheduler.c

#define RR_QUANTUM 2   // instructions per time slice — change here for evaluation

void scheduleRR(Queue* readyQ, Queue* blockedQ) {
    if (isEmpty(readyQ)) return;

    PCB* p = dequeue(readyQ);
    p->state = RUNNING;
    syncPCBToMemory(p);
    printf("[SCHED-RR] Running P%d\n", p->pid);

    int executed = 0;
    while (executed < RR_QUANTUM && p->state == RUNNING) {
        ensureInMemory(p, allProcesses, processCount, getBlockSize(p));
        int ran = executeInstruction(p, readyQ, blockedQ);
        if (!ran) break;  // finished or blocked
        executed++;
    }

    if (p->state == RUNNING) {
        // Time slice expired — move to back of queue
        p->state = READY;
        syncPCBToMemory(p);
        enqueue(readyQ, p);
        printf("[SCHED-RR] P%d time slice expired — re-queued\n", p->pid);
    }

    printQueue(readyQ,  "Ready");
    printQueue(blockedQ, "Blocked");
}
```

---

### 10.2 Highest Response Ratio Next (HRRN)

```c
float calcResponseRatio(PCB* p) {
    if (p->burstTime <= 0) return 0;
    return (float)(p->waitTime + p->burstTime) / (float)p->burstTime;
}

void scheduleHRRN(Queue* readyQ, Queue* blockedQ) {
    if (isEmpty(readyQ)) return;

    // Find the process with the highest response ratio
    QNode* node   = readyQ->head;
    PCB*   best   = node->pcb;
    float  bestRR = calcResponseRatio(best);

    while (node != NULL) {
        float rr = calcResponseRatio(node->pcb);
        if (rr > bestRR) {
            bestRR = rr;
            best   = node->pcb;
        }
        node = node->next;
    }

    removeById(readyQ, best->pid);
    best->state = RUNNING;
    syncPCBToMemory(best);
    printf("[SCHED-HRRN] Running P%d (RR=%.2f)\n", best->pid, bestRR);

    // HRRN is non-preemptive: run to completion (or until blocked)
    while (best->state == RUNNING) {
        ensureInMemory(best, allProcesses, processCount, getBlockSize(best));
        executeInstruction(best, readyQ, blockedQ);
    }

    // Increment wait time for all other ready processes
    node = readyQ->head;
    while (node != NULL) {
        node->pcb->waitTime++;
        node = node->next;
    }

    printQueue(readyQ,  "Ready");
    printQueue(blockedQ, "Blocked");
}
```

---

### 10.3 Multi-Level Feedback Queue (MLFQ) — Bonus

```c
// 4 queues: index 0 (highest priority) to 3 (lowest)
Queue mlfqQueues[4];
int   mlfqQuantums[4] = {1, 2, 4, 8};  // 2^i instructions

void initMLFQ() {
    for (int i = 0; i < 4; i++)
        initQueue(&mlfqQueues[i]);
}

// New processes always enter queue 0
void mlfqEnqueue(PCB* p) {
    p->queueLevel = 0;
    enqueue(&mlfqQueues[0], p);
}

void scheduleMLFQ(Queue* blockedQ) {
    // Find the highest-priority non-empty queue
    int level = -1;
    for (int i = 0; i < 4; i++) {
        if (!isEmpty(&mlfqQueues[i])) { level = i; break; }
    }
    if (level == -1) return;  // nothing to run

    PCB* p = dequeue(&mlfqQueues[level]);
    p->state = RUNNING;
    syncPCBToMemory(p);
    int quantum = mlfqQuantums[level];
    printf("[SCHED-MLFQ] Running P%d from Q%d (quantum=%d)\n",
           p->pid, level, quantum);

    int executed = 0;
    while (executed < quantum && p->state == RUNNING) {
        ensureInMemory(p, allProcesses, processCount, getBlockSize(p));
        int ran = executeInstruction(p, &mlfqQueues[level], blockedQ);
        if (!ran) break;
        executed++;
    }

    if (p->state == RUNNING) {
        // Used full quantum — demote to next queue
        if (level < 3) p->queueLevel = level + 1;
        else           p->queueLevel = 3;  // stays in Q3 (RR)
        p->state = READY;
        syncPCBToMemory(p);
        enqueue(&mlfqQueues[p->queueLevel], p);
        printf("[SCHED-MLFQ] P%d demoted to Q%d\n", p->pid, p->queueLevel);
    }
    // If blocked: semWait already handled queue removal
    // If finished: state is FINISHED, don't re-enqueue

    for (int i = 0; i < 4; i++) {
        char label[16];
        sprintf(label, "Q%d", i);
        printQueue(&mlfqQueues[i], label);
    }
}
```

---

## 11. Process Creation

```c
// Called when a process's arrival time matches the current clock tick

int  processCount = 0;
PCB* allProcesses[3];  // max 3 processes in this project

void createProcess(int pid, char* filepath, int arrivalTime,
                   Queue* readyQ) {
    // Count lines in program to determine block size needed
    FILE* f = fopen(filepath, "r");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filepath); return; }
    int lineCount = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), f))
        if (strlen(buf) > 1) lineCount++;
    fclose(f);

    int blockSize = PCB_SIZE + VAR_SIZE + lineCount;
    int lower = findFreeBlock(blockSize);

    if (lower == -1) {
        // No space — swap out a victim
        PCB* victim = chooseVictim(allProcesses, processCount, -1);
        if (victim) {
            swapOut(victim);
            lower = findFreeBlock(blockSize);
        }
    }

    if (lower == -1) {
        fprintf(stderr, "ERROR: Cannot allocate memory for P%d\n", pid);
        return;
    }

    int upper = lower + blockSize - 1;
    PCB* p = createPCB(pid, lower, upper, arrivalTime);
    allProcesses[processCount++] = p;

    // Write PCB fields to memory
    syncPCBToMemory(p);

    // Initialise variable slots to empty
    for (int i = 0; i < VAR_SIZE; i++)
        memWrite(lower + VAR_OFFSET + i, "");

    // Load program instructions into memory
    int n = loadProgram(filepath, p);
    p->burstTime = n;

    p->state = READY;
    syncPCBToMemory(p);
    enqueue(readyQ, p);

    printf("[CREATE] Process %d created | Memory: %d-%d | Instructions: %d\n",
           pid, lower, upper, n);
}
```

---

## 12. Main Simulation Loop

```c
// main.c

#include <stdio.h>
#include <string.h>
#include "pcb.h"
#include "memory.h"
#include "queue.h"
#include "mutex.h"
#include "scheduler.h"
#include "swap.h"

// Arrival times — change these for evaluation
#define ARRIVAL_P1 0
#define ARRIVAL_P2 1
#define ARRIVAL_P3 4

int currentClock = 0;

int main(int argc, char* argv[]) {
    // Determine scheduler from command-line argument
    char mode[8] = "rr";
    if (argc > 1) strncpy(mode, argv[1], sizeof(mode));

    // Initialise everything
    initMemory();
    initMutexes();

    Queue readyQ, blockedQ;
    initQueue(&readyQ);
    initQueue(&blockedQ);

    int created[3] = {0, 0, 0};
    char* files[3] = {
        "programs/Program1.txt",
        "programs/Program2.txt",
        "programs/Program3.txt"
    };
    int arrivals[3] = {ARRIVAL_P1, ARRIVAL_P2, ARRIVAL_P3};

    // Run until all processes finished
    while (1) {
        printf("\n========== Clock Cycle %d ==========\n", currentClock);

        // Check for arriving processes
        for (int i = 0; i < 3; i++) {
            if (!created[i] && currentClock == arrivals[i]) {
                createProcess(i + 1, files[i], arrivals[i], &readyQ);
                created[i] = 1;
            }
        }

        // Run the scheduler
        if      (strcmp(mode, "hrrn") == 0) scheduleHRRN(&readyQ, &blockedQ);
        else if (strcmp(mode, "mlfq") == 0) scheduleMLFQ(&blockedQ);
        else                                scheduleRR(&readyQ, &blockedQ);

        // Print memory every cycle
        printMemory();
        printMutexState();

        // Check termination condition
        int allDone = 1;
        for (int i = 0; i < processCount; i++) {
            if (allProcesses[i]->state != FINISHED) { allDone = 0; break; }
        }
        if (allDone) {
            printf("\n[SIM] All processes finished at clock %d.\n", currentClock);
            break;
        }

        currentClock++;

        // Safety: prevent infinite loop during debugging
        if (currentClock > 1000) {
            fprintf(stderr, "ERROR: Simulation exceeded 1000 cycles — possible deadlock\n");
            break;
        }
    }

    // Free all PCBs
    for (int i = 0; i < processCount; i++) free(allProcesses[i]);
    return 0;
}
```

---

## 13. The Three Program Files

### Program 1 — Print numbers between two inputs

```
assign a input
assign b input
semWait userOutput
printFromTo a b
semSignal userOutput
```

### Program 2 — Write data to a file

```
assign filename input
assign data input
semWait file
writeFile filename data
semSignal file
```

### Program 3 — Read and print a file's contents

```
assign filename input
semWait file
readFile filename
semSignal file
```

**Important:** The `semWait` and `semSignal` calls **wrap** the file/output operations inside the program file itself. The interpreter dispatches these as regular instructions. This is intentional — the program controls when it acquires and releases resources.

---

## 14. Required Output Format

The evaluator will check for all of the following at every scheduling event:

```
========== Clock Cycle 3 ==========

[CREATE] Process 2 created | Memory: 12-23 | Instructions: 5

[SCHED-RR] Running P1
[P1 | Clock 3] Executing: semWait userOutput
[MUTEX] Process 1 acquired 'userOutput'
[P1 | Clock 3] Executing: printFromTo a b
OUTPUT: 1
OUTPUT: 2
OUTPUT: 3

[SCHED-RR] P1 time slice expired — re-queued

Ready Queue  : [P1, P2]
Blocked Queue: []

[MUTEX STATE]
  userInput  : free (owner: P-1)
  userOutput : LOCKED (owner: P1)
  file       : free (owner: P-1)

+-------+-----------------------------+
| Addr  | Value                       |
+-------+-----------------------------+
|     0 | PID=1                       |
|     1 | STATE=READY                 |
|     2 | PC=2                        |
...
```

**Checklist of required outputs:**
- [ ] Queue state after every scheduling event (who was chosen, who blocked, who finished)
- [ ] Which process is currently executing
- [ ] Which instruction is currently executing
- [ ] Memory table every clock cycle (all 40 slots)
- [ ] PID of any process swapped in or swapped out
- [ ] Disk swap file format visible in output or as actual files

---

## 15. GUI (Optional Bonus)

The GUI is optional for C and worth bonus marks. The recommended approach is **ncurses** (terminal UI — no extra window needed).

### ncurses setup on Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-pdcurses
```

Add to Makefile:
```makefile
CFLAGS += -lpdcurses
```

### Suggested panel layout

```
+--------------------+---------------------------+
|  MEMORY (40 slots) |  READY QUEUE              |
|  [0]  PID=1        |  P1 -> P2 -> P3           |
|  [1]  STATE=READY  +---------------------------+
|  ...               |  BLOCKED QUEUE            |
|                    |  (empty)                  |
+--------------------+---------------------------+
|  RUNNING: P1       |  MUTEXES                  |
|  Instr: printFrom  |  userOutput: LOCKED (P1)  |
|  Clock: 5          |  file: free               |
+--------------------+---------------------------+
|  [STEP]  [RUN]  [PAUSE]  Scheduler: [RR]       |
+------------------------------------------------+
```

### Key ncurses calls

```c
initscr();
cbreak();
noecho();
start_color();

WINDOW* memWin   = newwin(20, 35, 0,  0);
WINDOW* queueWin = newwin(20, 35, 0, 36);
WINDOW* runWin   = newwin(5,  70, 20, 0);

box(memWin, 0, 0);
mvwprintw(memWin, 1, 2, "Memory");
wrefresh(memWin);

// Step-through: wait for keypress before each clock tick
getch();
```

---

## 16. Testing Checklist

Run these scenarios before submission:

### Unit tests
- [ ] `queue.c`: enqueue 3 items, dequeue all, verify order and size
- [ ] `memory.c`: write to all 40 slots, read them back, free a range, verify NULLs
- [ ] `mutex.c`: semWait twice on same mutex, verify second call blocks. semSignal, verify unblock
- [ ] `parser.c`: load each program file, print memory, verify instructions stored correctly
- [ ] `syscalls.c`: writeFile then readFile, verify content matches

### Integration tests
- [ ] RR with all 3 programs: verify output is not interleaved (mutex working)
- [ ] HRRN: manually compute response ratios and verify the right process is always picked
- [ ] Memory full scenario: reduce `MEM_SIZE` to 20 in testing, verify swap-out/swap-in triggers correctly
- [ ] Blocked process: verify a blocked process is not scheduled and eventually unblocks
- [ ] Change `RR_QUANTUM` to 1 and 3, verify behaviour changes correctly
- [ ] Change arrival times, verify processes are created at the right clock ticks

### Final check
- [ ] Run with `gcc -fsanitize=address` and check for no memory errors
- [ ] Run with Valgrind: `valgrind --leak-check=full ./os_sim rr`
- [ ] Verify all swap files (`swap_pid*.txt`) are deleted after simulation ends
- [ ] Output is clean and readable at all clock cycles

---

## 17. Work Distribution Summary

| Person | Files | Key Tasks |
|--------|-------|-----------|
| P1 — Foundation | `queue.c/h`, `pcb.c/h`, `memory.c/h` | Queue linked list, PCB struct, 40-word memory array |
| P2 — Parser | `parser.c/h` | File loader, variable helpers, instruction executor, write 3 program .txt files |
| P3 — Scheduler | `scheduler.c/h` | RR, HRRN, MLFQ (bonus), process arrival logic |
| P4 — Mutexes & Syscalls | `mutex.c/h`, `syscalls.c/h` | Mutex struct, semWait/semSignal, all 6 system calls |
| P5 — Integration | `main.c`, `swap.c/h` | Disk swap, process creation, main loop, testing |

**Build order:**
`queue` → `pcb` → `memory` → `mutex` → `syscalls` → `parser` → `scheduler` → `swap` → `main`

---

## 18. Common Pitfalls

**strtok modifies the string it's called on.**
Always copy the instruction string before tokenising:
```c
char copy[256];
strncpy(copy, memRead(addr), sizeof(copy));
char* cmd = strtok(copy, " ");
```

**strdup allocates memory — always free it.**
Every `memWrite` call should `free` the old pointer before assigning a new one. See the `memWrite` implementation above.

**programCounter is relative, not absolute.**
The absolute address of the current instruction is always `memLower + INSTR_OFFSET + programCounter`. Never store or use an absolute instruction address in the PCB.

**A blocked process stays in the ready queue for HRRN wait-time counting.**
No — a blocked process goes to the blocked queue only. Only processes in the ready queue have their `waitTime` incremented each tick.

**Swapped-out processes must stay in the scheduler.**
When you call `swapOut(p)`, do NOT remove `p` from the ready queue. Only its memory is freed. The scheduler will call `ensureInMemory(p)` before executing it.

**semSignal should re-lock the mutex for the next process.**
When `semSignal` unblocks a waiting process, the mutex should NOT become fully free — it should be immediately transferred to the newly unblocked process. Otherwise a third process could sneak in and acquire it first.

**Deadlock can occur if processes never call semSignal.**
If your program files are missing `semSignal` calls, the mutex will never be released. The simulation will loop forever. Add the 1000-cycle safety limit in `main.c` during development.

**fgets includes the newline — always strip it.**
```c
line[strcspn(line, "\n")] = '\0';
```

**Memory leaks from sysReadFile.**
`sysReadFile` returns a heap-allocated string. The caller must `free()` it after use.
