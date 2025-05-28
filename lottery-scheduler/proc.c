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

// Process table and lock
struct proc ptable[NPROC];
struct spinlock ptable_lock;

static struct proc *initproc;
int nextpid = 1;

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable_lock, "ptable");
  for (int i = 0; i < ncpu; i++)
  {
    rq_init(&cpus[i].rq);
  }
}

// Get the current CPU ID (must be called with interrupts disabled)
int cpuid(void)
{
  return mycpu() - cpus;
}

// Get the current CPU structure (must be called with interrupts disabled)
struct cpu *mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Get the current process (disables interrupts to prevent rescheduling)
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
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == UNUSED)
    {
      p->state = EMBRYO;
      p->tickets = 1;         // Default to 1 ticket
      p->ticks_scheduled = 0; // Initialize scheduling counter
      p->pid = nextpid++;
      p->cpu = -1; // Initialize to -1 (not assigned to a CPU yet)
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

      // Set up context to start at forkret
      sp -= 4;
      *(uint *)sp = (uint)trapret;

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

// Set up the first user process
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;

  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // Start of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // Seed the random number generator
  acquire(&tickslock);
  srand(ticks + lapicid() + p->pid);
  release(&tickslock);

  acquire(&ptable_lock);
  p->state = RUNNABLE;
  p->cpu = 0;
  rq_add(&cpus[0].rq, p); // rq_add handles its own locking
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
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process by copying the parent
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  if ((np = allocproc()) == 0)
    return -1;

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

  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  np->tickets = curproc->tickets;

  acquire(&ptable_lock);
  // Find CPU with fewest tickets
  int min_tickets = 999999;
  int target_cpu = 0;
  for (i = 0; i < ncpu; i++)
  {
    int cpu_tickets = 0;
    acquire(&cpus[i].rq.lock);
    for (int j = 0; j < cpus[i].rq.count; j++)
      if (cpus[i].rq.procs[j])
        cpu_tickets += cpus[i].rq.procs[j]->tickets;
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

// Exit the current process
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable_lock);
  wakeup1(curproc->parent);

  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  curproc->state = ZOMBIE;
  if (curproc->cpu < 0 || curproc->cpu >= ncpu)
    panic("exit: invalid CPU assignment");
  rq_remove(&cpus[curproc->cpu].rq, curproc); // Remove from the run queue since it's ZOMBIE
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its PID
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
        continue;
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
          panic("wait: invalid CPU assignment");
        rq_remove(&cpus[p->cpu].rq, p); // Ensure the process is removed from the run queue
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

// Lottery scheduler
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  static int sched_count = 0;

  c->proc = 0;

  for (;;)
  {
    sti();

    p = rq_select(&c->rq, sched_count); // rq_select handles its own locking
    if (p == 0)                         // No runnable processes on this CPU
    {
      continue;
    }

    acquire(&ptable_lock); // Protect state changes and c->proc
    rq_remove(&c->rq, p);  // rq_remove handles its own locking
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    p->ticks_scheduled++;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;
    release(&ptable_lock);

    sched_count++;
  }
}

// Enter the scheduler (must hold ptable_lock)
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable_lock))
    panic("sched ptable_lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");

  // If the process is RUNNABLE, add it back to its assigned CPU's run queue
  if (p->state == RUNNABLE)
  {
    if (p->cpu < 0 || p->cpu >= ncpu)
      panic("sched: invalid CPU assignment");
    rq_add(&cpus[p->cpu].rq, p);
  }

  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Yield the CPU for one scheduling round
void yield(void)
{
  struct proc *p = myproc();
  if (p->cpu < 0 || p->cpu >= ncpu)
    panic("yield: invalid CPU assignment");

  acquire(&ptable_lock);
  p->state = RUNNABLE;
  sched();
  release(&ptable_lock);
}

// First scheduling of a fork child, returns to user space
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

// Sleep on a channel (atomically releases lock and reacquires on wakeup)
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");
  if (lk == 0)
    panic("sleep without lk");

  if (lk != &ptable_lock)
  {
    acquire(&ptable_lock);
    release(lk);
  }
  p->chan = chan;
  p->state = SLEEPING;
  if (p->cpu < 0 || p->cpu >= ncpu)
    panic("sleep: invalid CPU assignment");
  rq_remove(&cpus[p->cpu].rq, p); // Remove from the run queue since it's SLEEPING

  sched();

  p->chan = 0;

  if (lk != &ptable_lock)
  {
    release(&ptable_lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on a channel (must hold ptable_lock)
static void wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      if (p->cpu < 0 || p->cpu >= ncpu)
        panic("wakeup1: invalid CPU assignment");
      rq_add(&cpus[p->cpu].rq, p); // Add to the assigned CPU's run queue
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

// Kill the process with the given PID
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
          panic("kill: invalid CPU assignment");
        rq_add(&cpus[p->cpu].rq, p); // Add to the assigned CPU's run queue
      }
      release(&ptable_lock);
      return 0;
    }
  }
  release(&ptable_lock);
  return -1;
}

// Print process listing for debugging (triggered by ^P)
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
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s tickets=%d scheduled=%d\n",
            p->pid, state, p->name, p->tickets, p->ticks_scheduled);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}