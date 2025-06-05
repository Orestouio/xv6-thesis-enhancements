/*
 * proc.h - Process Management Definitions for xv6
 *
 * This header file defines the structures and constants used for process
 * management in the xv6 operating system with a lottery scheduler. It includes
 * definitions for the per-CPU state, process state, and process table.
 *
 * Key Structures:
 * - struct cpu: Represents per-CPU state, including the runqueue for lottery scheduling.
 * - struct proc: Represents a process, including its scheduling parameters (tickets, ticks_scheduled).
 * - struct context: Defines the saved registers for context switching.
 *
 * Note: This implementation uses per-CPU runqueues for scalability in multi-CPU setups.
 */

#ifndef PROC_H
#define PROC_H

#include "runqueue.h"
#include "param.h"
#include "mmu.h"

// Global spinlock for the process table
struct spinlock ptable_lock;

// Per-CPU state structure
struct cpu
{
  uchar apicid;              // Local APIC ID
  struct context *scheduler; // Scheduler context for this CPU
  struct taskstate ts;       // Task state segment
  struct segdesc gdt[NSEGS]; // Global descriptor table
  volatile uint started;     // Has this CPU started?
  int ncli;                  // Depth of pushcli nesting
  int intena;                // Were interrupts enabled before pushcli?
  struct proc *proc;         // The currently running process on this CPU
  struct runqueue rq;        // Per-CPU runqueue for lottery scheduling
};

// Global array of CPUs and count
extern struct cpu cpus[NCPU];
extern int ncpu;

// Context structure for saving registers during a context switch
struct context
{
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

// Process state enumeration
enum procstate
{
  UNUSED,   // Process slot is free
  EMBRYO,   // Process is being created
  SLEEPING, // Process is sleeping on a channel
  RUNNABLE, // Process is ready to run
  RUNNING,  // Process is currently running
  ZOMBIE    // Process has exited but not yet cleaned up
};

// Process structure
struct proc
{
  uint sz;                    // Size of process memory (bytes)
  pde_t *pgdir;               // Page directory
  char *kstack;               // Bottom of kernel stack for this process
  enum procstate state;       // Process state (UNUSED, RUNNABLE, etc.)
  int pid;                    // Process ID
  struct proc *parent;        // Parent process
  struct trapframe *tf;       // Trap frame for current syscall
  struct context *context;    // Context for switching to this process
  void *chan;                 // Channel on which the process is sleeping
  int killed;                 // If non-zero, process has been killed
  struct file *ofile[NOFILE]; // Open files
  struct inode *cwd;          // Current directory
  char name[16];              // Process name (for debugging)
  int tickets;                // Number of lottery tickets for scheduling
  int ticks_scheduled;        // Number of times the process has been scheduled
  int recent_schedules;       // Recent scheduling count (used for decay in scheduler)
  int cpu;                    // CPU on which the process is assigned (-1 if unassigned)
  uint last_scheduled;        // Last tick when the process was scheduled
};

#endif