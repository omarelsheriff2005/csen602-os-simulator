#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syscalls.h"
#include "memory.h"
#include "sim.h"

void sysPrint(char* val) {
    simLog("OUTPUT: %s", val ? val : "");
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
    if (!f) {
        simLog("ERROR: sysWriteFile failed for %s", filename);
        return;
    }
    fputs(data, f);
    fputc('\n', f);
    fclose(f);
    simLog("[SYS] Wrote to file: %s", filename);
}

char* sysReadFile(char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        simLog("ERROR: [SYS] File not found: %s", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    while (size > 0 && (buf[size - 1] == '\n' || buf[size - 1] == '\r')) {
        buf[size - 1] = '\0';
        size--;
    }

    return buf;
}

char* sysMemRead(PCB* p, int addr) {
    if (!p || addr < p->memLower || addr > p->memUpper) {
        simLog("ERROR: P%d attempted memory read outside its bounds at %d",
               p ? p->pid : -1, addr);
        return NULL;
    }

    return memRead(addr);
}

void sysMemWrite(PCB* p, int addr, char* val) {
    if (!p || addr < p->memLower || addr > p->memUpper) {
        simLog("ERROR: P%d attempted memory write outside its bounds at %d",
               p ? p->pid : -1, addr);
        return;
    }

    memWrite(addr, val);
}
