/*
 * proc.c - Process Management Implementation for xv6
 *
 * This file implements process management for the xv6 operating system with a
 * lottery scheduler. It includes functions for process creation, scheduling,
 * context switching, and termination. The lottery scheduler uses per-CPU runqueues
 * to manage processes, with each process assigned a number of tickets to determine
 * its scheduling probability. A starvation prevention mechanism boosts tickets for
 * processes that have waited too long.
 *
 * Key Functions:
 * - pinit(): Initializes the process table and per-CPU runqueues.
 * - allocproc(): Allocates a new process structure.
 * - scheduler(): Main scheduling loop using lottery scheduling.
 * - fork(): Creates a new child process.
 * - exit(): Terminates the current process.
 * - wait(): Waits for a child process to terminate.
 */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "runqueue.h"
#include "rand.h"

// Global process table and lock
struct proc ptable[NPROC];
struct spinlock ptable_lock;

// Initial process and next PID counter
static struct proc *initproc;
int nextpid = 1;

// External function declarations
extern void forkret(void);
extern void trapret(void);

// Forward declaration
static void wakeup1(void *chan);

// Initialize the process table and per-CPU runqueues
void pinit(void)
{
  initlock(&ptable_lock, "ptable");
  for (int i = 0; i < ncpu; i++)
  {
    rq_init(&cpus[i].rq);
  }
}

// Get the ID of the current CPU
int cpuid(void)
{
  return mycpu() - cpus;
}

// Get a pointer to the current CPU structure
struct cpu *mycpu(void)
{
  int apicid, i;

  // Check if interrupts are enabled (should not be)
  if (readeflags() & FL_IF)
  {
    panic("mycpu called with interrupts enabled\n");
  }

  apicid = lapicid();
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
    {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Get a pointer to the current process
struct proc *myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// Allocate a new process structure from the process table
static struct proc *allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable_lock);
  // Find an unused process slot
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == UNUSED)
    {
      p->state = EMBRYO;
      p->tickets = 1;          // Default ticket count
      p->ticks_scheduled = 0;  // Initialize scheduling count
      p->recent_schedules = 0; // Initialize recent scheduling count
      p->last_scheduled = 0;   // Initialize last scheduled tick
      p->pid = nextpid++;      // Assign a new PID
      p->cpu = -1;             // Initially unassigned to any CPU
      release(&ptable_lock);

      // Allocate kernel stack
      if ((p->kstack = kalloc()) == 0)
      {
        acquire(&ptable_lock);
        p->state = UNUSED;
        release(&ptable_lock);
        return 0;
      }
      sp = p->kstack + KSTACKSIZE;

      // Set up trap frame
      sp -= sizeof *p->tf;
      p->tf = (struct trapframe *)sp;

      // Set up return address to trapret
      sp -= 4;
      *(uint *)sp = (uint)trapret;

      // Set up context for forkret
      sp -= sizeof *p->context;
      p->context = (struct context *)sp;
      memset(p->context, 0, sizeof *p->context);
      p->context->eip = (uint)forkret;

      return p;
    }
  }
  release(&ptable_lock);
  return 0;
}

// Initialize the first user process (initcode)
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;

  // Set up page directory and user memory
  if ((p->pgdir = setupkvm()) == 0)
  {
    panic("userinit: out of memory?");
  }
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // Add the process to CPU 0's runqueue
  acquire(&ptable_lock);
  p->state = RUNNABLE;
  p->cpu = 0;
  rq_add(&cpus[0].rq, p);
  release(&ptable_lock);
}

// Grow the current process's memory by n bytes
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
    {
      return -1;
    }
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new child process by duplicating the current process
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate a new process
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy the parent's memory
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    acquire(&ptable_lock);
    np->state = UNUSED;
    release(&ptable_lock);
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Set return value for child
  np->tf->eax = 0;

  // Duplicate open files and current directory
  for (i = 0; i < NOFILE; i++)
  {
    if (curproc->ofile[i])
    {
      np->ofile[i] = filedup(curproc->ofile[i]);
    }
  }
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  np->tickets = curproc->tickets; // Inherit parent's ticket count

  // Assign the new process to the CPU with the least total tickets
  acquire(&ptable_lock);
  int min_tickets = 999999;
  int target_cpu = 0;
  for (i = 0; i < ncpu; i++)
  {
    int cpu_tickets = 0;
    acquire(&cpus[i].rq.lock);
    for (int j = 0; j < cpus[i].rq.count; j++)
    {
      if (cpus[i].rq.procs[j])
      {
        cpu_tickets += cpus[i].rq.procs[j]->tickets;
      }
    }
    release(&cpus[i].rq.lock);
    if (cpu_tickets < min_tickets)
    {
      min_tickets = cpu_tickets;
      target_cpu = i;
    }
  }
  np->state = RUNNABLE;
  np->cpu = target_cpu;
  rq_add(&cpus[target_cpu].rq, np);
  release(&ptable_lock);

  return pid;
}

// Terminate the current process
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
  {
    panic("init exiting");
  }

  // Close all open files
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  // Release current directory
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable_lock);
  wakeup1(curproc->parent);

  // Reassign children to initproc
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
      {
        wakeup1(initproc);
      }
    }
  }

  // Mark process as ZOMBIE and remove from runqueue
  curproc->state = ZOMBIE;
  if (curproc->cpu < 0 || curproc->cpu >= ncpu)
  {
    panic("exit: invalid CPU assignment");
  }
  rq_remove(&cpus[curproc->cpu].rq, curproc);
  sched();
  panic("zombie exit");
}

// Wait for a child process to terminate
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable_lock);
  for (;;)
  {
    havekids = 0;
    for (p = ptable; p < &ptable[NPROC]; p++)
    {
      if (p->parent != curproc)
      {
        continue;
      }
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        if (p->cpu < 0 || p->cpu >= ncpu)
        {
          panic("wait: invalid CPU assignment");
        }
        rq_remove(&cpus[p->cpu].rq, p);
        release(&ptable_lock);
        return pid;
      }
    }

    if (!havekids || curproc->killed)
    {
      release(&ptable_lock);
      return -1;
    }

    sleep(curproc, &ptable_lock);
  }
}

// Main scheduler loop using lottery scheduling
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  static int sched_count = 0; // Counter for scheduling decisions

  c->proc = 0;

  for (;;)
  {
    cli(); // Disable interrupts

    // Periodically decay recent_schedules to prevent long-term bias
    if (sched_count % 100 == 0)
    {
      acquire(&ptable_lock);
      for (struct proc *proc = ptable; proc < &ptable[NPROC]; proc++)
      {
        if (proc->state == RUNNABLE || proc->state == RUNNING)
        {
          proc->recent_schedules = proc->recent_schedules * 3 / 4;
        }
      }
      release(&ptable_lock);
    }

    // Select a process to run using lottery scheduling
    p = rq_select(&c->rq, sched_count);
    if (p == 0)
    {
      // No process selected; check for any runnable processes
      acquire(&ptable_lock);
      int runnable_count = 0;
      for (struct proc *proc = ptable; proc < &ptable[NPROC]; proc++)
      {
        if (proc->state == RUNNABLE && proc->cpu == c->apicid)
        {
          runnable_count++;
        }
      }
      release(&ptable_lock);
      sti(); // Re-enable interrupts
      continue;
    }

    // Run the selected process
    acquire(&ptable_lock);
    rq_remove(&c->rq, p);
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    p->ticks_scheduled++;
    p->recent_schedules++;
    p->last_scheduled = ticks;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;
    release(&ptable_lock);

    sched_count++;
    sti(); // Re-enable interrupts
  }
}

// Context switch to the scheduler
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  // Sanity checks
  if (!holding(&ptable_lock))
  {
    panic("sched ptable_lock");
  }
  if (mycpu()->ncli != 1)
  {
    panic("sched locks");
  }
  if (p->state == RUNNING)
  {
    panic("sched running");
  }
  if (readeflags() & FL_IF)
  {
    panic("sched interruptible");
  }

  // Add process back to runqueue if it's still runnable
  if (p->state == RUNNABLE)
  {
    if (p->cpu < 0 || p->cpu >= ncpu)
    {
      panic("sched: invalid CPU assignment");
    }
    rq_add(&cpus[p->cpu].rq, p);
  }

  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Yield the CPU to another process
void yield(void)
{
  struct proc *p = myproc();
  if (p->cpu < 0 || p->cpu >= ncpu)
  {
    panic("yield: invalid CPU assignment");
  }

  acquire(&ptable_lock);
  p->state = RUNNABLE;
  sched();
  release(&ptable_lock);
}

// Handle return from fork in the child process
void forkret(void)
{
  static int first = 1;
  release(&ptable_lock);

  if (first)
  {
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
}

// Put the current process to sleep on a channel
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
  {
    panic("sleep");
  }
  if (lk == 0)
  {
    panic("sleep without lk");
  }

  // Acquire ptable_lock if not already held
  if (lk != &ptable_lock)
  {
    acquire(&ptable_lock);
    release(lk);
  }
  p->chan = chan;
  p->state = SLEEPING;
  p->recent_schedules = 0;
  if (p->cpu < 0 || p->cpu >= ncpu)
  {
    panic("sleep: invalid CPU assignment");
  }
  rq_remove(&cpus[p->cpu].rq, p);

  sched();

  p->chan = 0;

  // Release ptable_lock if necessary
  if (lk != &ptable_lock)
  {
    release(&ptable_lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on a channel (internal function)
static void wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      p->recent_schedules = 0;
      if (p->cpu < 0 || p->cpu >= ncpu)
      {
        panic("wakeup1: invalid CPU assignment");
      }
      rq_add(&cpus[p->cpu].rq, p);
    }
  }
}

// Wake up all processes sleeping on a channel
void wakeup(void *chan)
{
  acquire(&ptable_lock);
  wakeup1(chan);
  release(&ptable_lock);
}

// Kill a process with the given PID
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable_lock);
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        p->state = RUNNABLE;
        if (p->cpu < 0 || p->cpu >= ncpu)
        {
          panic("kill: invalid CPU assignment");
        }
        rq_add(&cpus[p->cpu].rq, p);
      }
      release(&ptable_lock);
      return 0;
    }
  }
  release(&ptable_lock);
  return -1;
}

// Dump process table information for debugging
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == UNUSED)
    {
      continue;
    }
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
    {
      state = states[p->state];
    }
    else
    {
      state = "???";
    }
    cprintf("%d %s %s tickets=%d scheduled=%d\n",
            p->pid, state, p->name, p->tickets, p->ticks_scheduled);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
      {
        cprintf(" %p", pc[i]);
      }
    }
    cprintf("\n");
  }
}