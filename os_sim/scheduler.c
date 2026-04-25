#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "parser.h"
#include "swap.h"
#include "pcb.h"
#include "queue.h"
#include "sim.h"

Queue mlfqQueues[4];

int getBlockSize(PCB* p) {
    return p->memUpper - p->memLower + 1;
}

void scheduleRR(Queue* readyQ, Queue* blockedQ) {
    if (isEmpty(readyQ)) return;

    {
        PCB* p = dequeue(readyQ);
        int executed = 0;

        simState.currentlyRunning = p;
        ensureInMemory(p, allProcesses, processCount, getBlockSize(p));
        if (!p->inMemory) {
            simFail("ERROR: Cannot continue P%d because it could not be swapped in", p->pid);
            return;
        }
        p->state = RUNNING;
        syncPCBToMemory(p);
        simLog("[SCHED] RR selected P%d", p->pid);

        while (executed < RR_QUANTUM && p->state == RUNNING) {
            ensureInMemory(p, allProcesses, processCount, getBlockSize(p));
            if (!executeInstruction(p, readyQ, blockedQ)) break;
            executed++;
        }

        if (p->state == RUNNING) {
            simEnqueueReady(p);
            simLog("[SCHED] RR time slice expired for P%d", p->pid);
        }
    }

    printQueue(readyQ, "Ready");
    printQueue(blockedQ, "Blocked");
}

float calcResponseRatio(PCB* p) {
    if (p->burstTime <= 0) return 0.0f;
    return (float)(p->waitTime + p->burstTime) / (float)p->burstTime;
}

void scheduleHRRN(Queue* readyQ, Queue* blockedQ) {
    if (isEmpty(readyQ)) return;

    {
        QNode* node = readyQ->head;
        PCB* best = node->pcb;
        float bestRR = calcResponseRatio(best);

        while (node != NULL) {
            float rr = calcResponseRatio(node->pcb);
            if (rr > bestRR) {
                bestRR = rr;
                best = node->pcb;
            }
            node = node->next;
        }

        removeById(readyQ, best->pid);
        simState.currentlyRunning = best;
        ensureInMemory(best, allProcesses, processCount, getBlockSize(best));
        if (!best->inMemory) {
            simFail("ERROR: Cannot continue P%d because it could not be swapped in", best->pid);
            return;
        }
        best->state = RUNNING;
        syncPCBToMemory(best);
        simLog("[SCHED] HRRN selected P%d (RR=%.2f)", best->pid, bestRR);

        while (best->state == RUNNING) {
            ensureInMemory(best, allProcesses, processCount, getBlockSize(best));
            if (!executeInstruction(best, readyQ, blockedQ)) break;
        }
    }

    {
        QNode* node = readyQ->head;
        while (node != NULL) {
            node->pcb->waitTime++;
            node = node->next;
        }
    }

    printQueue(readyQ, "Ready");
    printQueue(blockedQ, "Blocked");
}

static int mlfqQuantums[4] = {1, 2, 4, 8};

void initMLFQ() {
    for (int i = 0; i < 4; i++)
        initQueue(&mlfqQueues[i]);
}

void mlfqEnqueue(PCB* p) {
    p->queueLevel = 0;
    enqueue(&mlfqQueues[0], p);
}

void scheduleMLFQ(Queue* blockedQ) {
    int level = -1;

    for (int i = 0; i < 4; i++) {
        if (!isEmpty(&mlfqQueues[i])) {
            level = i;
            break;
        }
    }
    if (level == -1) return;

    {
        PCB* p = dequeue(&mlfqQueues[level]);
        int quantum = mlfqQuantums[level];
        int executed = 0;

        simState.currentlyRunning = p;
        ensureInMemory(p, allProcesses, processCount, getBlockSize(p));
        if (!p->inMemory) {
            simFail("ERROR: Cannot continue P%d because it could not be swapped in", p->pid);
            return;
        }
        p->state = RUNNING;
        syncPCBToMemory(p);
        simLog("[SCHED] MLFQ selected P%d from Q%d (quantum=%d)",
               p->pid, level, quantum);

        while (executed < quantum && p->state == RUNNING) {
            ensureInMemory(p, allProcesses, processCount, getBlockSize(p));
            if (!executeInstruction(p, &mlfqQueues[level], blockedQ)) break;
            executed++;
        }

        if (p->state == RUNNING) {
            p->queueLevel = (level < 3) ? (level + 1) : 3;
            simEnqueueReady(p);
            simLog("[SCHED] MLFQ demoted P%d to Q%d", p->pid, p->queueLevel);
        }
    }

    for (int i = 0; i < 4; i++) {
        char label[16];
        sprintf(label, "Q%d", i);
        printQueue(&mlfqQueues[i], label);
    }
    printQueue(blockedQ, "Blocked");
}
