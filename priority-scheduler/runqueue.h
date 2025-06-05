#ifndef RUNQUEUE_H
#define RUNQUEUE_H

#include "spinlock.h"

#define MAX_PROCS 64

struct runqueue
{
    struct proc *priority_head[11]; // Head of linked list for each priority (0-10)
    struct proc *priority_tail[11]; // Tail of linked list for each priority
    struct proc *short_lived_head;  // Head of short-lived FIFO queue
    struct proc *short_lived_tail;  // Tail of short-lived FIFO queue
    int count;                      // Total number of processes in runqueue
    struct spinlock lock;           // Lock for runqueue operations
};

void rq_init(struct runqueue *rq);
void rq_add(struct runqueue *rq, struct proc *p);
void rq_remove(struct runqueue *rq, struct proc *p);
struct proc *rq_select(struct runqueue *rq);

#endif