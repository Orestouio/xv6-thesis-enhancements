#ifndef PROC_H
#define PROC_H

#include "runqueue.h"
#include "param.h" // For NCPU, NOFILE
#include "mmu.h"   // For NSEGS, struct segdesc

struct spinlock ptable_lock;
struct spinlock load_balance_lock;

// Per-CPU state
struct cpu
{
  uchar apicid;
  struct context *scheduler;
  struct taskstate ts;
  struct segdesc gdt[NSEGS];
  volatile uint started;
  int ncli;
  int intena;
  struct proc *proc;
  struct runqueue rq;
};

extern struct cpu cpus[NCPU];
extern int ncpu;

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

struct proc
{
  uint sz;
  pde_t *pgdir;
  char *kstack;
  enum procstate state;
  int pid;
  struct proc *parent;
  struct trapframe *tf;
  struct context *context;
  void *chan;
  int killed;
  struct file *ofile[NOFILE];
  struct inode *cwd;
  char name[16];
  int tickets;
  int boost; // Temporary boost to tickets for fairness
  int ticks_scheduled;
  int expected_schedules; // Expected schedules based on ticket proportion (new field)
  int ticket_boost;       // Temporary boost to tickets for fairness (new field)
  int recent_schedules;   // New field to track recent scheduling frequency
  int cpu;
  uint last_scheduled; // Add this to track last scheduled time
};

void srand(unsigned int seed);

#endif