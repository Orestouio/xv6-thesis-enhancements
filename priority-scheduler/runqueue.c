/*
 * runqueue.c: Manages per-CPU runqueues for the priority scheduler.
 * Provides functions to initialize, add, remove, and select processes from runqueues,
 * organizing processes by priority (0-10, 0 highest) and a special queue for priority 5.
 */

#include "types.h"
#include "defs.h"
#include "proc.h"
#include "runqueue.h"

// Initialize a runqueue, setting up its lock and priority queues.
void rq_init(struct runqueue *rq)
{
    // Initialize the spinlock for thread-safe runqueue access
    initlock(&rq->lock, "runqueue");

    // Reset process count
    rq->count = 0;

    // Clear priority queues (0-10) for all priority levels
    for (int i = 0; i < 11; i++)
    {
        rq->priority_head[i] = 0;
        rq->priority_tail[i] = 0;
    }

    // Clear special queue for short-lived processes (priority 5)
    rq->short_lived_head = 0;
    rq->short_lived_tail = 0;
}

// Add a process to the runqueue based on its priority.
void rq_add(struct runqueue *rq, struct proc *p)
{
    // Acquire runqueue lock for thread safety
    acquire(&rq->lock);

    // Check if runqueue is full
    if (rq->count >= MAX_PROCS)
        panic("runqueue full");

    // Validate process pointer
    if (!p)
        panic("rq_add: null proc");

    // Handle special case: priority 5 processes go to short-lived queue
    if (p->priority == 5)
    {
        p->next = 0; // Clear next pointer
        if (rq->short_lived_head == 0)
        {
            // Empty queue: set process as both head and tail
            rq->short_lived_head = p;
            rq->short_lived_tail = p;
        }
        else
        {
            // Non-empty queue: append to tail
            if (!rq->short_lived_tail)
                panic("rq_add: null short_lived_tail");
            rq->short_lived_tail->next = p;
            rq->short_lived_tail = p;
        }
    }
    else
    {
        // Handle other priorities (0-4, 6-10)
        int prio = p->priority;
        if (prio < 0 || prio > 10)
            panic("rq_add: invalid priority");

        p->next = 0; // Clear next pointer
        if (rq->priority_head[prio] == 0)
        {
            // Empty queue for this priority: set as head and tail
            rq->priority_head[prio] = p;
            rq->priority_tail[prio] = p;
        }
        else
        {
            // Non-empty queue: append to tail
            if (!rq->priority_tail[prio])
                panic("rq_add: null priority_tail");
            rq->priority_tail[prio]->next = p;
            rq->priority_tail[prio] = p;
        }
    }

    // Increment process count
    rq->count++;

    // Release runqueue lock
    release(&rq->lock);
}

// Remove a process from the runqueue.
void rq_remove(struct runqueue *rq, struct proc *p)
{
    // Acquire runqueue lock for thread safety
    acquire(&rq->lock);

    // Validate process pointer
    if (!p)
        panic("rq_remove: null proc");

    // Handle priority 5 processes in short-lived queue
    if (p->priority == 5)
    {
        struct proc *curr = rq->short_lived_head;
        struct proc *prev = 0;

        // Traverse queue to find process
        while (curr != 0)
        {
            if (curr == p)
            {
                // Update links to remove process
                if (prev == 0)
                    rq->short_lived_head = curr->next;
                else
                    prev->next = curr->next;

                // Update tail if removing last process
                if (curr->next == 0)
                    rq->short_lived_tail = prev;

                p->next = 0; // Clear next pointer
                rq->count--; // Decrement count
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    else
    {
        // Handle other priorities (0-4, 6-10)
        int prio = p->priority;
        if (prio < 0 || prio > 10)
            panic("rq_remove: invalid priority");

        struct proc *curr = rq->priority_head[prio];
        struct proc *prev = 0;

        // Traverse queue to find process
        while (curr != 0)
        {
            if (curr == p)
            {
                // Update links to remove process
                if (prev == 0)
                    rq->priority_head[prio] = curr->next;
                else
                    prev->next = curr->next;

                // Update tail if removing last process
                if (curr->next == 0)
                    rq->priority_tail[prio] = prev;

                p->next = 0; // Clear next pointer
                rq->count--; // Decrement count
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }

    // Release runqueue lock
    release(&rq->lock);
}

// Select and remove the highest-priority process from the runqueue.
struct proc *rq_select(struct runqueue *rq)
{
    // Acquire runqueue lock for thread safety
    acquire(&rq->lock);

    // Return null if runqueue is empty
    if (rq->count == 0)
    {
        release(&rq->lock);
        return 0;
    }

    // Check short-lived queue (priority 5) first
    struct proc *p = rq->short_lived_head;
    if (p)
    {
        // Remove process from head
        rq->short_lived_head = p->next;
        if (!p->next)
            rq->short_lived_tail = 0;

        p->next = 0; // Clear next pointer
        rq->count--; // Decrement count
        release(&rq->lock);
        return p;
    }

    // Check priority queues (0-10) in ascending order
    for (int i = 0; i < 11; i++)
    {
        if (rq->priority_head[i])
        {
            // Remove process from head
            p = rq->priority_head[i];
            rq->priority_head[i] = p->next;
            if (!p->next)
                rq->priority_tail[i] = 0;

            p->next = 0; // Clear next pointer
            rq->count--; // Decrement count
            release(&rq->lock);
            return p;
        }
    }

    // Release lock and return null if no process found
    release(&rq->lock);
    return 0;
}