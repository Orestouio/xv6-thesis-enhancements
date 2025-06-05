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
  struct runqueue rq;        // Per-CPU runqueue for priority scheduling
};

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
  enum procstate state;       // Process state
  int pid;                    // Process ID
  struct proc *parent;        // Parent process
  struct trapframe *tf;       // Trap frame for current syscall
  struct context *context;    // Context for switching to this process
  void *chan;                 // Channel on which the process is sleeping
  int killed;                 // If non-zero, process has been killed
  struct file *ofile[NOFILE]; // Open files
  struct inode *cwd;          // Current directory
  char name[16];              // Process name (for debugging)
  int priority;               // Priority level (0-10, 0 is highest)
  struct proc *next;          // Next process in priority queue
  int wait_ticks;             // Track waiting time for aging
  uint creation_time;         // Time when process was created
  uint completion_time;       // Time when process completed
  uint waiting_time;          // Total time spent in RUNNABLE state
  uint last_runnable_tick;    // Last time process became RUNNABLE
  uint first_run_time;        // Time when process first ran
  int has_run;                // Flag: 0 if hasn’t run yet, 1 if has
  uint cpu_time;              // Total CPU time used
  int cpu;                    // CPU on which the process is assigned (-1 if unassigned)
};

#endif