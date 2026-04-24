#ifndef QUEUE_H
#define QUEUE_H

#include "pcb.h"

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
int   removeById(Queue* q, int pid);   /* returns 1 if found and removed */
int   isEmpty(Queue* q);
void  printQueue(Queue* q, char* label);

#endif
