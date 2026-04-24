#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include "sim.h"

void initQueue(Queue* q) {
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

void enqueue(Queue* q, PCB* p) {
    QNode* node = malloc(sizeof(QNode));
    node->pcb  = p;
    node->next = NULL;

    if (q->tail == NULL) {
        q->head = node;
        q->tail = node;
    } else {
        q->tail->next = node;
        q->tail = node;
    }
    q->size++;
}

PCB* dequeue(Queue* q) {
    if (q->head == NULL) return NULL;

    QNode* node = q->head;
    PCB*   p    = node->pcb;
    q->head = node->next;
    if (q->head == NULL) q->tail = NULL;
    free(node);
    q->size--;
    return p;
}

PCB* peek(Queue* q) {
    if (q->head == NULL) return NULL;
    return q->head->pcb;
}

int removeById(Queue* q, int pid) {
    QNode* prev = NULL;
    QNode* curr = q->head;

    while (curr != NULL) {
        if (curr->pcb->pid == pid) {
            if (prev == NULL)
                q->head = curr->next;
            else
                prev->next = curr->next;
            if (curr == q->tail)
                q->tail = prev;
            free(curr);
            q->size--;
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

int isEmpty(Queue* q) {
    return q->head == NULL;
}

void printQueue(Queue* q, char* label) {
    char buf[LOG_LINE_LEN];
    int written = snprintf(buf, sizeof(buf), "%s Queue: [", label);
    QNode* node = q->head;
    while (node != NULL) {
        if (written < (int)sizeof(buf))
            written += snprintf(buf + written, sizeof(buf) - written, "P%d", node->pcb->pid);
        if (node->next != NULL && written < (int)sizeof(buf))
            written += snprintf(buf + written, sizeof(buf) - written, ", ");
        node = node->next;
    }
    if (written < (int)sizeof(buf))
        snprintf(buf + written, sizeof(buf) - written, "]");
    simLog("%s", buf);
}
