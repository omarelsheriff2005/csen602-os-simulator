#ifndef MEMORY_H
#define MEMORY_H

#define MEM_SIZE   40
#define PCB_SIZE    4
#define VAR_SIZE    3

#define PCB_OFFSET_PID    0
#define PCB_OFFSET_STATE  1
#define PCB_OFFSET_PC     2
#define PCB_OFFSET_BOUNDS 3
#define VAR_OFFSET        4
#define INSTR_OFFSET      7

extern char* memory[MEM_SIZE];

void  initMemory();
void  memWrite(int addr, char* val);
char* memRead(int addr);
int   findFreeBlock(int size);
void  freeBlock(int lower, int upper);
void  printMemory();

#endif
