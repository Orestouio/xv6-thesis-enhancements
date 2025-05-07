#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "types.h"

// Mutual exclusion lock.
struct spinlock
{
    uint locked;     // Is the lock held?
    char *name;      // Name of lock.
    struct cpu *cpu; // The cpu holding the lock.
    uint pcs[10];    // The call stack (an array of program counters)
};

// Function declarations
void initlock(struct spinlock *, char *);
void acquire(struct spinlock *);
void release(struct spinlock *);
int holding(struct spinlock *);
void pushcli(void);
void popcli(void);
void getcallerpcs(void *, uint *);

#endif