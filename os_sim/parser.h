#ifndef PARSER_H
#define PARSER_H

#include "pcb.h"
#include "queue.h"

int   loadProgram(char* filepath, PCB* p);
void  setVar(PCB* p, char* name, char* value);
char* getVar(PCB* p, char* name);
char* resolveArg(PCB* p, char* arg);
int   executeInstruction(PCB* p, Queue* readyQ, Queue* blockedQ);

#endif
