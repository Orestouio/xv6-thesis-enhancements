#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"
#include "param.h"    // For NPROC, NCPU, NOFILE
#include "spinlock.h" // For struct spinlock in ptable

// Forward declarations
struct taskstate; // From mmu.h
struct segdesc;   // From mmu.h
struct trapframe; // From traps.h
struct file;      // From file.h
struct inode;     // From file.h

extern int context_switches; // Declare global variable

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
    pde_t *pgdir;               // Page table
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
    int start_ticks;            // Creation time (for timingtests)
    int first_run_ticks;        // First run time
    int run_ticks;              // Total CPU time
    int wait_ticks;             // Total waiting time in RUNNABLE state
    int end_ticks;              // Completion time
};

// Process table structure
struct ptable
{
    struct spinlock lock; // Actual spinlock instance
    struct proc proc[NPROC];
};
extern struct ptable ptable; // Global process table

// Process stats for getpinfo
struct proc_stat
{
    int pid;
    int turnaround; // end_ticks - start_ticks
    int response;   // first_run_ticks - start_ticks
    int waiting;    // wait_ticks
    int cpu;        // run_ticks
};

// Function declarations
void pinit(void);
int cpuid(void);
struct cpu *mycpu(void);
struct proc *myproc(void);
int fork(void);
void scheduler(void);
void sched(void);
void yield(void);
void forkret(void);
void sleep(void *, struct spinlock *);
void wakeup(void *);
int kill(int);
void procdump(void);

#endif