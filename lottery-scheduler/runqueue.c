#include "types.h"
#include "defs.h"
#include "proc.h"
#include "runqueue.h"
#include "rand.h" // Add this include

// Initialize the run queue
void rq_init(struct runqueue *rq)
{
    initlock(&rq->lock, "runqueue");
    rq->count = 0;
    for (int i = 0; i < MAX_PROCS; i++)
    {
        rq->procs[i] = 0;
    }
}

// Add a process to the run queue
void rq_add(struct runqueue *rq, struct proc *p)
{
    acquire(&rq->lock);
    if (rq->count >= MAX_PROCS)
    {
        panic("runqueue full");
    }
    for (int i = 0; i < MAX_PROCS; i++)
    {
        if (rq->procs[i] == 0)
        {
            rq->procs[i] = p;
            rq->count++;
            break;
        }
    }
    release(&rq->lock);
}

// Remove a process from the run queue
void rq_remove(struct runqueue *rq, struct proc *p)
{
    acquire(&rq->lock);
    for (int i = 0; i < MAX_PROCS; i++)
    {
        if (rq->procs[i] == p)
        {
            rq->procs[i] = 0;
            rq->count--;
            for (int j = i; j < MAX_PROCS - 1; j++)
            {
                rq->procs[j] = rq->procs[j + 1];
            }
            rq->procs[MAX_PROCS - 1] = 0;
            break;
        }
    }
    release(&rq->lock);
}

// Select a process to run using lottery scheduling
struct proc *rq_select(struct runqueue *rq, int sched_count)
{
    acquire(&rq->lock);
    if (rq->count == 0)
    {
        release(&rq->lock);
        return 0;
    }

    int total_tickets = 0;
    for (int i = 0; i < MAX_PROCS; i++)
    {
        if (rq->procs[i])
        {
            total_tickets += rq->procs[i]->tickets;
        }
    }

    if (total_tickets == 0)
    {
        release(&rq->lock);
        return 0;
    }

    struct proc *temp_procs[MAX_PROCS];
    int temp_count = 0;
    for (int i = 0; i < MAX_PROCS; i++)
    {
        if (rq->procs[i])
        {
            temp_procs[temp_count++] = rq->procs[i];
        }
    }
    for (int i = temp_count - 1; i > 0; i--)
    {
        srand(ticks + lapicid() + randstate + i);
        int j = rand_range(i + 1);
        struct proc *temp = temp_procs[i];
        temp_procs[i] = temp_procs[j];
        temp_procs[j] = temp;
    }

    srand(ticks + lapicid() + randstate + temp_count + sched_count);
    int winner = rand_range(total_tickets);
    int current_tickets = 0;
    struct proc *selected = 0;
    for (int i = 0; i < temp_count; i++)
    {
        struct proc *p = temp_procs[i];
        int effective_tickets = p->tickets;
        if (winner < effective_tickets + current_tickets)
        {
            selected = p;
            break;
        }
        current_tickets += effective_tickets;
    }

    release(&rq->lock);
    return selected;
}