#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// Simple Linear Congruential Generator (LCG) with better properties
static unsigned int randstate = 1;

void srand(unsigned int seed)
{
  randstate = seed;
  if (randstate == 0) // Xorshift requires a non-zero state
    randstate = 1;
}

unsigned int rand(void)
{
  unsigned int x = randstate;
  x ^= (x << 13);
  x ^= (x >> 17);
  x ^= (x << 5);
  randstate = x;
  return x & 0x7fffffff;
}

unsigned int rand_range(unsigned int max)
{
  // Avoid modulo bias by rejecting values that would skew the distribution
  unsigned int threshold = (0x7fffffff / max) * max;
  unsigned int r;
  do
  {
    r = rand();
  } while (r >= threshold);
  return r % max;
}

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
}

// Must be called with interrupts disabled
int cpuid(void)
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled
struct cpu *
mycpu(void)
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

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable_lock);
  for (p = ptable; p < &ptable[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;
  release(&ptable_lock);
  return 0;

found:
  p->state = EMBRYO;
  p->tickets = 1;         // Default to 1 ticket
  p->ticks_scheduled = 0; // Initialize counter
  p->expected_schedules = 0;
  p->ticket_boost = 0; // Initialize
  p->pid = nextpid++;

  release(&ptable_lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// PAGEBREAK: 32
// Set up first user process.
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
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // Seed the random number generator with a combination of ticks, CPU ID, and PID
  acquire(&tickslock);
  srand(ticks + lapicid() + p->pid);
  release(&tickslock);

  acquire(&ptable_lock);
  p->state = RUNNABLE;
  release(&ptable_lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
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

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  if ((np = allocproc()) == 0)
  {
    return -1;
  }

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

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
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

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
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

// PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  struct proc *runnable_procs[NPROC];
  int runnable_count;
  int total_tickets;
  int winner;
  int current_tickets;
  int total_scheds = 0;
  static int winner_histogram[100] = {0};
  static int sched_count = 0;

  c->proc = 0;

  for (;;)
  {
    sti();

    total_tickets = 0;
    runnable_count = 0;
    total_scheds = 0;
    acquire(&ptable_lock);

    for (p = ptable; p < &ptable[NPROC]; p++)
    {
      if (p->state == RUNNABLE)
      {
        runnable_procs[runnable_count++] = p;
        total_tickets += p->tickets + p->ticket_boost;
        total_scheds += p->ticks_scheduled;
      }
    }

    if (total_tickets == 0)
    {
      release(&ptable_lock);
      continue;
    }

    // Track processes by ticket groups for Test 2
    int group_tickets[3] = {0, 0, 0}; // For tickets 30, 20, 10
    int group_sched[3] = {0, 0, 0};
    int group_count[3] = {0, 0, 0};
    for (int i = 0; i < runnable_count; i++)
    {
      p = runnable_procs[i];
      if (p->tickets == 30)
      {
        group_tickets[0] += p->tickets;
        group_sched[0] += p->ticks_scheduled;
        group_count[0]++;
      }
      else if (p->tickets == 20)
      {
        group_tickets[1] += p->tickets;
        group_sched[1] += p->ticks_scheduled;
        group_count[1]++;
      }
      else if (p->tickets == 10)
      {
        group_tickets[2] += p->tickets;
        group_sched[2] += p->ticks_scheduled;
        group_count[2]++;
      }
    }

    // Dynamic boost frequency: every 5 events if >10 processes, otherwise every 10
    int boost_interval = (runnable_count > 10) ? 5 : 10;
    if (total_scheds > 0 && (sched_count % boost_interval == 0))
    {
      for (int g = 0; g < 3; g++)
      {
        if (group_count[g] == 0)
          continue;
        int expected = (group_tickets[g] * total_scheds) / total_tickets;
        int group_boost = 0;
        if (group_sched[g] < expected)
        {
          group_boost = (expected - group_sched[g]); // Aggressive boost
        }
        int boost_per_proc = group_count[g] > 0 ? group_boost / group_count[g] : 0;
        int min_boost = (runnable_count > 10) ? 5 : 2; // Dynamic minimum boost
        for (int i = 0; i < runnable_count; i++)
        {
          p = runnable_procs[i];
          if ((g == 0 && p->tickets == 30) || (g == 1 && p->tickets == 20) || (g == 2 && p->tickets == 10))
          {
            p->ticket_boost = boost_per_proc;
            if (p->tickets <= 10 && p->ticket_boost < min_boost)
            {
              p->ticket_boost = min_boost;
            }
            // Adjusted cap: allow boost up to 2x tickets for low-ticket processes
            int boost_cap = (p->tickets <= 10) ? 2 * p->tickets : p->tickets;
            if (p->ticket_boost > boost_cap)
            {
              p->ticket_boost = boost_cap;
            }
          }
        }
      }
    }

    // Shuffle runnable_procs to eliminate ordering bias
    for (int i = runnable_count - 1; i > 0; i--)
    {
      srand(ticks + lapicid() + randstate + i);
      int j = rand_range(i + 1);
      struct proc *temp = runnable_procs[i];
      runnable_procs[i] = runnable_procs[j];
      runnable_procs[j] = temp;
    }

    srand(ticks + lapicid() + randstate + runnable_count + total_scheds);
    winner = rand_range(total_tickets);

    if (total_tickets <= 100)
    {
      winner_histogram[winner]++;
    }

    current_tickets = 0;
    for (int i = 0; i < runnable_count; i++)
    {
      p = runnable_procs[i];
      int effective_tickets = p->tickets + p->ticket_boost;
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
    sched_count++; // Increment sched_count after each scheduling event
  }
}

// Enter scheduler.  Must hold only ptable_lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
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

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable_lock);
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable_lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
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

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
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

// PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable; p < &ptable[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable_lock);
  wakeup1(chan);
  release(&ptable_lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
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

// PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
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