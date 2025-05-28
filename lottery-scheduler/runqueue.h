#ifndef RUNQUEUE_H
#define RUNQUEUE_H

#include "spinlock.h"

#define MAX_PROCS 64 // Maximum processes per run queue (same as NPROC)

struct runqueue
{
    struct proc *procs[MAX_PROCS]; // Array of pointers to runnable processes
    int count;                     // Number of runnable processes
    struct spinlock lock;          // Lock for this run queue
};

void rq_init(struct runqueue *rq);
void rq_add(struct runqueue *rq, struct proc *p);
void rq_remove(struct runqueue *rq, struct proc *p);
struct proc *rq_select(struct runqueue *rq, int sched_count);

#endif