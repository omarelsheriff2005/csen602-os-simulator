#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "sim.h"

char* memory[MEM_SIZE];

void initMemory() {
    for (int i = 0; i < MEM_SIZE; i++) {
        free(memory[i]);
        memory[i] = NULL;
    }
}

void memWrite(int addr, char* val) {
    if (addr < 0 || addr >= MEM_SIZE) {
        simLog("ERROR: memWrite out of bounds at %d", addr);
        return;
    }
    free(memory[addr]);
    memory[addr] = strdup(val);
}

char* memRead(int addr) {
    if (addr < 0 || addr >= MEM_SIZE) {
        simLog("ERROR: memRead out of bounds at %d", addr);
        return NULL;
    }
    return memory[addr];
}

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

void freeBlock(int lower, int upper) {
    for (int i = lower; i <= upper; i++) {
        free(memory[i]);
        memory[i] = NULL;
    }
}

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
