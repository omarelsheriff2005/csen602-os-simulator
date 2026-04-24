#ifndef SWAP_H
#define SWAP_H

#include "pcb.h"

void  swapOut(PCB* p);
void  swapIn(PCB* p, int newLower);
PCB*  chooseVictim(PCB** allProcesses, int count, int runningPid);
int   findFreeBlockWithSwap(PCB** allProcesses, int count, int blockSize, int runningPid);
void  ensureInMemory(PCB* p, PCB** allProcesses, int count, int blockSize);

#endif
