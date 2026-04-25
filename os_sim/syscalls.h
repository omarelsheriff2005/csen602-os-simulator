#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "pcb.h"

void  sysPrint(char* val);
char* sysGetInput();
void  sysWriteFile(char* filename, char* data);
char* sysReadFile(char* filename);
char* sysMemRead(PCB* p, int addr);
void  sysMemWrite(PCB* p, int addr, char* val);

#endif
