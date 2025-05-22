#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"
#include "param.h"
#include "spinlock.h"

extern int context_switches;
extern int padding_before[];
extern int padding_after[];

// Forward declarations for structs defined elsewhere
struct taskstate; // From mmu.h
struct segdesc;   // From mmu.h
struct trapframe; // From traps.h
struct file;      // From file.h
struct inode;     // From file.h

// Per-CPU state
struct cpu
{
  uchar apicid;              // Local APIC ID
  struct context *scheduler; // swtch() here to enter scheduler
  struct taskstate ts;       // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS]; // x86 global descriptor table
  volatile uint started;     // Has the CPU started?
  int ncli;                  // Depth of pushcli nesting.
  int intena;                // Were interrupts enabled before pushcli?
  struct proc *proc;         // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Saved registers for kernel context switches
struct context
{
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate
{
  UNUSED,
  EMBRYO,
  SLEEPING,
  RUNNABLE,
  RUNNING,
  ZOMBIE
};

// Per-process state
struct proc
{
  uint sz;                    // Size of process memory (bytes)
  uint *pgdir;                // Page table (simplified from pde_t)
  char *kstack;               // Bottom of kernel stack for this process
  enum procstate state;       // Process state
  int pid;                    // Process ID
  struct proc *parent;        // Parent process
  struct trapframe *tf;       // Trap frame for current syscall
  struct context *context;    // swtch() here to run process
  void *chan;                 // If non-zero, sleeping on chan
  int killed;                 // If non-zero, have been killed
  struct file *ofile[NOFILE]; // Open files
  struct inode *cwd;          // Current directory
  char name[16];              // Process name (debugging)
  int priority;               // Priority level (0-10, 0 is highest)
  int just_woke;              // Flag for just woken from sleep
  uint creation_time;         // Time when process was created
  uint completion_time;       // Time when process completed
  uint waiting_time;          // Total time spent in RUNNABLE state
  uint last_runnable_tick;    // Last time process became RUNNABLE
  uint first_run_time;        // Time when process first ran
  int has_run;                // Flag: 0 if hasn’t run yet, 1 if has
  uint cpu_time;              // Total CPU time used
  int wait_ticks;             // Track waiting time for aging
  struct proc *next;          // Next process in priority queue
  int time_slice;             // Field for tracking time slice
};

// Process table structure
struct ptable
{
  struct spinlock lock;     // Actual spinlock instance, not pointer
  struct proc *proc[NPROC]; // Array of pointers to proc structs
};
extern struct ptable ptable; // Global process table

// Function declarations
void pinit(void);
int fork(void);
void scheduler(void);
struct proc *myproc(void);
void forkret(void);
void trapret(void);
void print_sched_log(void);

#endif