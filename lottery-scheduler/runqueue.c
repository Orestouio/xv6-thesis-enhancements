/*
 * runqueue.c - Lottery Scheduler Runqueue Implementation for xv6
 *
 * This file implements the runqueue management functions for a lottery scheduler
 * in the xv6 operating system. Each CPU has its own runqueue, and processes are
 * scheduled using a lottery mechanism based on their ticket counts.
 *
 * Key Features:
 * - Per-CPU runqueues for scalability in multi-CPU setups.
 * - Lottery scheduling: Processes are selected probabilistically based on ticket counts.
 */

#include "types.h"
#include "defs.h"
#include "proc.h"
#include "runqueue.h"
#include "rand.h"

// Initialize a runqueue for a CPU
// Sets up the spinlock and clears the process array
void rq_init(struct runqueue *rq)
{
    initlock(&rq->lock, "runqueue");
    rq->count = 0;
    for (int i = 0; i < MAX_PROCS; i++)
    {
        rq->procs[i] = 0;
    }
}

// Add a process to the runqueue
// Finds the first empty slot and adds the process, panics if the runqueue is full
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

// Remove a process from the runqueue
// Optimized to move the last process to the removed slot instead of shifting all processes
void rq_remove(struct runqueue *rq, struct proc *p)
{
    acquire(&rq->lock);
    for (int i = 0; i < MAX_PROCS; i++)
    {
        if (rq->procs[i] == p)
        {
            // Move the last process to the current slot to avoid shifting
            rq->procs[i] = rq->procs[rq->count - 1];
            rq->procs[rq->count - 1] = 0;
            rq->count--;
            break;
        }
    }
    release(&rq->lock);
}

// Select a process to run using lottery scheduling
// Returns a process based on ticket proportions
struct proc *rq_select(struct runqueue *rq, int sched_count)
{
    acquire(&rq->lock);
    if (rq->count == 0)
    {
        release(&rq->lock);
        return 0; // No processes to schedule
    }

    // Step 1: Calculate total tickets
    int total_tickets = 0;
    for (int i = 0; i < MAX_PROCS; i++)
    {
        if (rq->procs[i])
        {
            struct proc *p = rq->procs[i];
            int effective_tickets = p->tickets;
            if (effective_tickets < 1)
                effective_tickets = 1; // Ensure at least 1 ticket
            total_tickets += effective_tickets;
        }
    }

    // Debug check: This should never happen since count > 0
    if (total_tickets == 0)
    {
        cprintf("rq_select: total_tickets = 0 despite count = %d\n", rq->count);
        release(&rq->lock);
        return 0;
    }

    // Step 2: Create a temporary array of processes for shuffling
    struct proc *temp_procs[MAX_PROCS];
    int temp_count = 0;
    for (int i = 0; i < MAX_PROCS; i++)
    {
        if (rq->procs[i])
        {
            temp_procs[temp_count++] = rq->procs[i];
        }
    }

    // Step 3: Map APIC ID to CPU ID for random number generation
    int cpu_id = lapicid();
    for (int i = 0; i < ncpu; i++)
    {
        if (cpus[i].apicid == cpu_id)
        {
            cpu_id = i;
            break;
        }
    }

    // Step 4: Shuffle the process array to avoid bias in selection
    for (int i = temp_count - 1; i > 0; i--)
    {
        int j = rand_range(i + 1);
        struct proc *temp = temp_procs[i];
        temp_procs[i] = temp_procs[j];
        temp_procs[j] = temp;
    }

    // Step 5: Select a winner based on ticket proportions
    int winner = rand_range(total_tickets);
    int current_tickets = 0;
    struct proc *selected = 0;
    for (int i = 0; i < temp_count; i++)
    {
        struct proc *p = temp_procs[i];
        int effective_tickets = p->tickets;
        if (effective_tickets < 1)
            effective_tickets = 1;

        // Check if the random winner falls within this process's ticket range
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