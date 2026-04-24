#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "swap.h"
#include "memory.h"
#include "pcb.h"
#include "sim.h"

void swapOut(PCB* p) {
    char filename[32];
    sprintf(filename, "swap_pid%d.txt", p->pid);
    FILE* f = fopen(filename, "w");
    if (!f) {
        simLog("ERROR: swapOut failed for P%d", p->pid);
        return;
    }

    for (int i = p->memLower; i <= p->memUpper; i++) {
        if (memory[i] != NULL)
            fprintf(f, "%s\n", memory[i]);
        else
            fprintf(f, "\n");
    }
    fclose(f);

    freeBlock(p->memLower, p->memUpper);
    p->inMemory = 0;
    simLog("[SWAP] Process %d swapped OUT to %s", p->pid, filename);
}

void swapIn(PCB* p, int newLower) {
    char filename[32];
    sprintf(filename, "swap_pid%d.txt", p->pid);
    FILE* f = fopen(filename, "r");
    if (!f) {
        simLog("ERROR: swapIn failed for P%d", p->pid);
        return;
    }

    int blockSize = p->memUpper - p->memLower + 1;
    char line[256];
    for (int i = 0; i < blockSize; i++) {
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            memWrite(newLower + i, line);
        }
    }
    fclose(f);
    remove(filename);

    p->memLower = newLower;
    p->memUpper = newLower + blockSize - 1;
    p->inMemory = 1;

    {
        char bounds[32];
        sprintf(bounds, "BOUNDS=%d-%d", p->memLower, p->memUpper);
        memWrite(p->memLower + PCB_OFFSET_BOUNDS, bounds);
    }

    simLog("[SWAP] Process %d swapped IN at addresses %d-%d",
           p->pid, p->memLower, p->memUpper);
}

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

int findFreeBlockWithSwap(PCB** allProcesses, int count, int blockSize, int runningPid) {
    int lower = findFreeBlock(blockSize);

    while (lower == -1) {
        PCB* victim = chooseVictim(allProcesses, count, runningPid);
        if (!victim) break;
        swapOut(victim);
        lower = findFreeBlock(blockSize);
    }

    return lower;
}

void ensureInMemory(PCB* p, PCB** allProcesses, int count, int blockSize) {
    if (p->inMemory) return;

    {
        int lower = findFreeBlockWithSwap(allProcesses, count, blockSize, p->pid);
        if (lower != -1)
            swapIn(p, lower);
        else
            simLog("ERROR: Cannot swap in process %d - no memory", p->pid);
    }
}
