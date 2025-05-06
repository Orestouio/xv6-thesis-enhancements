#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// Random number generator state
static unsigned int randstate = 1;

// Seed the random number generator
void srand(unsigned int seed)
{
  randstate = seed;
  if (randstate == 0) // Ensure non-zero state for Xorshift
    randstate = 1;
}

// Generate a random number using Xorshift algorithm
unsigned int rand(void)
{
  unsigned int x = randstate;
  x ^= (x << 13);
  x ^= (x >> 17);
  x ^= (x << 5);
  randstate = x;
  return x & 0x7fffffff;
}

// Generate a random number in the range [0, max)
unsigned int rand_range(unsigned int max)
{
  unsigned int threshold = (0x7fffffff / max) * max;
  unsigned int r;
  do
  {
    r = rand();
  } while (r >= threshold); // Avoid modulo bias
  return r % max;
}

// Process table and lock
struct proc ptable[NPROC];
struct spinlock ptable_lock;

static struct proc *initproc;
int nextpid = 1;

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// Initialize the process table lock
void pinit(void)
{
  initlock(&ptable_lock, "ptable");
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
    np->state = UNUSED;
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
  np->state = RUNNABLE;
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
  struct proc *runnable_procs[NPROC];
  int runnable_count;
  int total_tickets;
  int winner;
  int current_tickets;
  static int sched_count = 0;

  c->proc = 0;

  for (;;)
  {
    sti();

    total_tickets = 0;
    runnable_count = 0;
    acquire(&ptable_lock);

    // Collect all RUNNABLE processes
    for (p = ptable; p < &ptable[NPROC]; p++)
    {
      if (p->state == RUNNABLE)
      {
        runnable_procs[runnable_count++] = p;
        total_tickets += p->tickets;
      }
    }

    if (total_tickets == 0)
    {
      release(&ptable_lock);
      continue;
    }

    // Shuffle runnable processes to eliminate ordering bias
    for (int i = runnable_count - 1; i > 0; i--)
    {
      srand(ticks + lapicid() + randstate + i);
      int j = rand_range(i + 1);
      struct proc *temp = runnable_procs[i];
      runnable_procs[i] = runnable_procs[j];
      runnable_procs[j] = temp;
    }

    // Pick a random winning ticket
    srand(ticks + lapicid() + randstate + runnable_count + sched_count);
    winner = rand_range(total_tickets);

    // Select the process whose ticket range includes the winner
    current_tickets = 0;
    for (int i = 0; i < runnable_count; i++)
    {
      p = runnable_procs[i];
      int effective_tickets = p->tickets;
      if (winner < effective_tickets + current_tickets)
      {
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->ticks_scheduled++;
        swtch(&(c->scheduler), p->context);
        switchkvm();
        c->proc = 0;
        break;
      }
      current_tickets += effective_tickets;
    }

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
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Yield the CPU for one scheduling round
void yield(void)
{
  acquire(&ptable_lock);
  myproc()->state = RUNNABLE;
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
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
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
        p->state = RUNNABLE;
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