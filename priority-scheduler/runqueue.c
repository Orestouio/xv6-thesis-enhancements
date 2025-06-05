#include "types.h"
#include "defs.h"
#include "proc.h"
#include "runqueue.h"

void rq_init(struct runqueue *rq)
{
    initlock(&rq->lock, "runqueue");
    rq->count = 0;
    for (int i = 0; i < 11; i++)
    {
        rq->priority_head[i] = 0;
        rq->priority_tail[i] = 0;
    }
    rq->short_lived_head = 0;
    rq->short_lived_tail = 0;
}

void rq_add(struct runqueue *rq, struct proc *p)
{
    acquire(&rq->lock);
    if (rq->count >= MAX_PROCS)
        panic("runqueue full");
    if (!p)
        panic("rq_add: null proc");
    if (p->priority == 5)
    {
        p->next = 0;
        if (rq->short_lived_head == 0)
        {
            rq->short_lived_head = p;
            rq->short_lived_tail = p;
        }
        else
        {
            if (!rq->short_lived_tail)
                panic("rq_add: null short_lived_tail");
            rq->short_lived_tail->next = p;
            rq->short_lived_tail = p;
        }
    }
    else
    {
        int prio = p->priority;
        if (prio < 0 || prio > 10)
            panic("rq_add: invalid priority ");
        p->next = 0;
        if (rq->priority_head[prio] == 0)
        {
            rq->priority_head[prio] = p;
            rq->priority_tail[prio] = p;
        }
        else
        {
            if (!rq->priority_tail[prio])
                panic("rq_add: null priority_tail");
            rq->priority_tail[prio]->next = p;
            rq->priority_tail[prio] = p;
        }
    }
    rq->count++;
    release(&rq->lock);
}

void rq_remove(struct runqueue *rq, struct proc *p)
{
    acquire(&rq->lock);
    if (!p)
        panic("rq_remove: null proc");
    if (p->priority == 5)
    {
        struct proc *curr = rq->short_lived_head;
        struct proc *prev = 0;
        while (curr != 0)
        {
            if (curr == p)
            {
                if (prev == 0)
                    rq->short_lived_head = curr->next;
                else
                    prev->next = curr->next;
                if (curr->next == 0)
                    rq->short_lived_tail = prev;
                p->next = 0;
                rq->count--;
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    else
    {
        int prio = p->priority;
        if (prio < 0 || prio > 10)
            panic("rq_remove: invalid priority ");
        struct proc *curr = rq->priority_head[prio];
        struct proc *prev = 0;
        while (curr != 0)
        {
            if (curr == p)
            {
                if (prev == 0)
                    rq->priority_head[prio] = curr->next;
                else
                    prev->next = curr->next;
                if (curr->next == 0)
                    rq->priority_tail[prio] = prev;
                p->next = 0;
                rq->count--;
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    release(&rq->lock);
}

struct proc *rq_select(struct runqueue *rq)
{
    acquire(&rq->lock);
    if (rq->count == 0)
    {
        release(&rq->lock);
        return 0;
    }
    struct proc *p = rq->short_lived_head;
    if (p)
    {
        rq->short_lived_head = p->next;
        if (!p->next)
            rq->short_lived_tail = 0;
        p->next = 0;
        rq->count--;
        release(&rq->lock);
        return p;
    }
    for (int i = 0; i < 11; i++)
    {
        if (rq->priority_head[i])
        {
            p = rq->priority_head[i];
            rq->priority_head[i] = p->next;
            if (!p->next)
                rq->priority_tail[i] = 0;
            p->next = 0;
            rq->count--;
            release(&rq->lock);
            return p;
        }
    }
    release(&rq->lock);
    return 0;
}