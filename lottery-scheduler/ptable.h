#ifndef PTABLE_H
#define PTABLE_H

#include "proc.h"     // For struct proc
#include "spinlock.h" // For struct spinlock

extern struct proc ptable[];
extern struct spinlock ptable_lock;

#endif