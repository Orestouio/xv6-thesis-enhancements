#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"
#include "mmu.h"
#include "spinlock.h"
#include "file.h"
#include "fs.h"

#define NSEM 10   // Maximum number of semaphores in the system
#define MAX_SEM 4 // Maximum semaphores a process can open

struct sem
{
  int value;                 // Semaphore value (count)
  struct spinlock lock;      // Lock for thread-safe access
  int in_use;                // Flag indicating if the semaphore slot is in use
  struct proc *queue[NPROC]; // Wait queue for processes blocked on this semaphore
  int queue_head;            // Head of the circular wait queue
  int queue_tail;            // Tail of the circular wait queue
};

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

// PAGEBREAK: 17
//  Saved registers for kernel context switches.
//  Don't need to save all the segment registers (%cs, etc),
//  because they are constant across kernel contexts.
//  Don't need to save %eax, %ecx, %edx, because the
//  x86 convention is that the caller has saved them.
//  Contexts are stored at the bottom of the stack they
//  describe; the stack pointer is the address of the context.
//  The layout of the context matches the layout of the stack in swtch.S
//  at the "Switch stacks" comment. Switch doesn't save eip explicitly,
//  but it is on the stack and allocproc() manipulates it.
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

// Maximum number of shared memory objects
#define NSHM 10

// Maximum number of shared memory mappings per process
#define MAX_SHM_MAPPINGS 4

// Shared memory structure
struct shm
{
  char name[16];        // Name of the shared memory object
  int in_use;           // 1 if in use, 0 if free
  void *phys_addr;      // Physical address of the shared memory
  int size;             // Size of the shared memory region (in bytes)
  int ref_count;        // Number of processes using this shared memory
  struct spinlock lock; // Lock for thread safety
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
  // Shared memory mappings
  void *shm_mappings[MAX_SHM_MAPPINGS];      // Virtual addresses of mapped shared memory
  struct shm *shm_objects[MAX_SHM_MAPPINGS]; // Pointers to shared memory objects
  int shm_count;                             // Number of shared memory mappings
  // Semaphores
  int sem_ids[MAX_SEM]; // Array of semaphore IDs this process has opened
  int sem_count;        // Number of semaphores this process has opened
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

// Shared memory table
extern struct shm shmtable[NSHM];

// Semaphore table
extern struct sem semtable[NSEM];

#endif // _PROC_H_