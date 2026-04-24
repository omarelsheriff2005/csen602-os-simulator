#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "memory.h"
#include "syscalls.h"
#include "mutex.h"
#include "sim.h"

extern int currentClock;

static int finishProcess(PCB* p) {
    simMarkFinished(p);
    return 0;
}

int loadProgram(char* filepath, PCB* p) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        simLog("ERROR: Cannot open program file: %s", filepath);
        return -1;
    }

    {
        char line[256];
        int instrIndex = 0;
        int instrBase = p->memLower + INSTR_OFFSET;

        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            if (strlen(line) == 0) continue;

            if (instrBase + instrIndex > p->memUpper) {
                fclose(f);
                simLog("ERROR: Program too large for allocated memory");
                return -1;
            }

            memWrite(instrBase + instrIndex, line);
            instrIndex++;
        }

        fclose(f);
        p->burstTime = instrIndex;
        return instrIndex;
    }
}

void setVar(PCB* p, char* name, char* value) {
    int base = p->memLower + VAR_OFFSET;
    char key[128];

    for (int i = 0; i < VAR_SIZE; i++) {
        char* slot = memRead(base + i);
        if (slot == NULL) continue;
        sscanf(slot, "%[^=]", key);
        if (strcmp(key, name) == 0) {
            char newVal[256];
            snprintf(newVal, sizeof(newVal), "%s=%s", name, value);
            memWrite(base + i, newVal);
            return;
        }
    }

    for (int i = 0; i < VAR_SIZE; i++) {
        char* slot = memRead(base + i);
        if (slot == NULL || strlen(slot) == 0) {
            char newVal[256];
            snprintf(newVal, sizeof(newVal), "%s=%s", name, value);
            memWrite(base + i, newVal);
            return;
        }
    }

    simLog("ERROR: No variable slots available for process %d", p->pid);
}

char* getVar(PCB* p, char* name) {
    int base = p->memLower + VAR_OFFSET;
    size_t nameLen = strlen(name);

    for (int i = 0; i < VAR_SIZE; i++) {
        char* slot = memRead(base + i);
        if (slot == NULL) continue;
        if (strncmp(slot, name, nameLen) == 0 && slot[nameLen] == '=')
            return slot + nameLen + 1;
    }

    return NULL;
}

char* resolveArg(PCB* p, char* arg) {
    char* val = getVar(p, arg);
    return (val != NULL) ? val : arg;
}

int executeInstruction(PCB* p, Queue* readyQ, Queue* blockedQ) {
    int instrAddr = p->memLower + INSTR_OFFSET + p->programCounter;

    if (instrAddr > p->memUpper || memRead(instrAddr) == NULL)
        return finishProcess(p);

    {
        char instr[256];
        char* cmd;
        char* arg1;
        char* arg2;

        strncpy(instr, memRead(instrAddr), sizeof(instr));
        instr[sizeof(instr) - 1] = '\0';

        simSetCurrentInstruction(p, instr);
        simLog("[EXEC] P%d | Clock %d | %s", p->pid, currentClock, instr);

        cmd = strtok(instr, " ");
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");

        if (cmd == NULL) {
            p->programCounter++;
            syncPCBToMemory(p);
            return 1;
        }

        if (strcmp(cmd, "print") == 0) {
            char* value = resolveArg(p, arg1);
            sysPrint(value);
        } else if (strcmp(cmd, "assign") == 0) {
            if (arg2 != NULL && strcmp(arg2, "input") == 0) {
                if (guiMode) {
                    simState.waitingForInput = 1;
                    simState.inputProcess = p;
                    strncpy(simState.inputVarName, arg1, sizeof(simState.inputVarName) - 1);
                    simState.inputVarName[sizeof(simState.inputVarName) - 1] = '\0';
                    snprintf(simState.inputPrompt, sizeof(simState.inputPrompt),
                             "P%d needs input for variable '%s':", p->pid, arg1);
                    p->state = BLOCKED;
                    syncPCBToMemory(p);
                    removeById(blockedQ, p->pid);
                    enqueue(blockedQ, p);
                    simLog("[INPUT] P%d waiting for value for %s", p->pid, arg1);
                    return 0;
                } else {
                    char* val = sysGetInput();
                    setVar(p, arg1, val);
                    simLog("[INPUT] P%d: %s = %s", p->pid, arg1, val);
                }
            } else if (arg2 != NULL && strcmp(arg2, "readFile") == 0) {
                char* arg3 = strtok(NULL, " ");
                char* filename = resolveArg(p, arg3);
                char* content = sysReadFile(filename);
                if (content) {
                    setVar(p, arg1, content);
                    free(content);
                }
            } else {
                setVar(p, arg1, resolveArg(p, arg2));
            }
        } else if (strcmp(cmd, "writeFile") == 0) {
            char* filename = resolveArg(p, arg1);
            char* data = resolveArg(p, arg2);
            sysWriteFile(filename, data);
        } else if (strcmp(cmd, "readFile") == 0) {
            char* filename = resolveArg(p, arg1);
            char* content = sysReadFile(filename);
            if (content) {
                sysPrint(content);
                free(content);
            }
        } else if (strcmp(cmd, "printFromTo") == 0) {
            char* fromVal = resolveArg(p, arg1);
            char* toVal = resolveArg(p, arg2);
            int from = atoi(fromVal);
            int to = atoi(toVal);
            for (int n = from; n <= to; n++) {
                char num[32];
                snprintf(num, sizeof(num), "%d", n);
                sysPrint(num);
            }
        } else if (strcmp(cmd, "semWait") == 0) {
            semWait(arg1, p, readyQ, blockedQ);
        } else if (strcmp(cmd, "semSignal") == 0) {
            semSignal(arg1, readyQ, blockedQ);
        } else {
            simLog("ERROR: Unknown instruction '%s'", cmd);
        }
    }

    if (p->state != BLOCKED) {
        int nextAddr;

        p->programCounter++;
        syncPCBToMemory(p);
        nextAddr = p->memLower + INSTR_OFFSET + p->programCounter;

        if (nextAddr > p->memUpper || memRead(nextAddr) == NULL)
            return finishProcess(p);
    }

    return 1;
}
