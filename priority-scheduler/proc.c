/*
 * proc.c: Implements process management for the xv6 priority scheduler.
 * Manages process creation, termination, scheduling, and priority updates.
 * Maintains a per-CPU runqueue and tracks context switches and scheduling logs.
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

// Global process table and lock
struct proc ptable[NPROC];
struct spinlock ptable_lock;

// Initial process pointer
static struct proc *initproc;

// Next available PID
int nextpid = 1;

// Context switch counter
int context_switches = 0;

// Scheduling log structure and index
#define LOG_SIZE 100
struct
{
  int tick;     // Timestamp of scheduling event
  int pid;      // Process ID
  int priority; // Process priority
  int cs_count; // Context switch count
} sched_log[LOG_SIZE];
int log_index = 0;

// External function declarations
extern void forkret(void);
extern void trapret(void);

// Forward declaration for static function
static void wakeup1(void *chan);

// Log a scheduling event.
void log_schedule(int tick, int pid, int priority, int cs_count)
{
  if (log_index < LOG_SIZE)
  {
    // Store event details in log
    sched_log[log_index].tick = tick;
    sched_log[log_index].pid = pid;
    sched_log[log_index].priority = priority;
    sched_log[log_index].cs_count = cs_count;
    log_index++;
  }
}

// Print and clear the scheduling log.
void print_sched_log(void)
{
  // Iterate through log entries
  for (int i = 0; i < log_index; i++)
  {
    // cprintf("Tick %d: Switch to PID %d, Priority %d, CS %d\n",
    //         sched_log[i].tick, sched_log[i].pid, sched_log[i].priority, sched_log[i].cs_count);
  }

  // Reset log index
  log_index = 0;
}

// Initialize process table and per-CPU runqueues.
void pinit(void)
{
  // Initialize process table lock
  initlock(&ptable_lock, "ptable");

  // Initialize runqueue for each CPU
  for (int i = 0; i < ncpu; i++)
    rq_init(&cpus[i].rq);
}

// Return the ID of the current CPU.
int cpuid(void)
{
  return mycpu() - cpus;
}

// Return a pointer to the current CPU structure.
struct cpu *mycpu(void)
{
  int apicid, i;

  // Check for interrupts (should be disabled)
  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  // Find CPU by APIC ID
  apicid = lapicid();
  for (i = 0; i < ncpu; ++i)
    if (cpus[i].apicid == apicid)
      return &cpus[i];

  panic("unknown apicid\n");
}

// Return a pointer to the current process.
struct proc *myproc(void)
{
  struct cpu *c;
  struct proc *p;

  // Disable interrupts to ensure consistency
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();

  return p;
}

// Allocate a new process structure from the process table.
static struct proc *allocproc(void)
{
  struct proc *p;
  char *sp;

  // Acquire process table lock
  acquire(&ptable_lock);

  // Find an unused process slot
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == UNUSED)
    {
      // Initialize process fields
      p->state = EMBRYO;
      p->pid = nextpid++;
      p->priority = 5; // Default priority
      p->wait_ticks = 0;
      p->next = 0;
      p->creation_time = ticks;
      p->completion_time = 0;
      p->waiting_time = 0;
      p->last_runnable_tick = 0;
      p->first_run_time = 0;
      p->has_run = 0;
      p->cpu_time = 0;
      p->cpu = -1;
      release(&ptable_lock);

      // Allocate kernel stack
      if ((p->kstack = kalloc()) == 0)
      {
        acquire(&ptable_lock);
        p->state = UNUSED;
        release(&ptable_lock);
        return 0;
      }

      // Set up stack for trap frame and context
      sp = p->kstack + KSTACKSIZE;
      sp -= sizeof *p->tf;
      p->tf = (struct trapframe *)sp;
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

// Initialize the first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  // Allocate process structure
  p = allocproc();
  initproc = p;

  // Set up page directory and user memory
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;

  // Initialize trap frame
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;

  // Set process name and current directory
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // Make process runnable on CPU 0
  acquire(&ptable_lock);
  p->state = RUNNABLE;
  p->cpu = 0;
  p->last_runnable_tick = ticks;
  rq_add(&cpus[0].rq, p);
  release(&ptable_lock);
}

// Grow or shrink the calling process's memory by n bytes.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    // Allocate additional memory
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    // Deallocate memory
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new child process by duplicating the calling process.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate new process
  if ((np = allocproc()) == 0)
    return -1;

  // Copy process memory
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    acquire(&ptable_lock);
    np->state = UNUSED;
    release(&ptable_lock);
    return -1;
  }

  // Initialize child process fields
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  np->tf->eax = 0;

  // Duplicate open files
  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);

  // Duplicate current directory
  np->cwd = idup(curproc->cwd);
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  pid = np->pid;

  // Assign process to CPU with fewest processes
  acquire(&ptable_lock);
  int min_procs = NPROC;
  int target_cpu = 0;
  for (i = 0; i < ncpu; i++)
  {
    acquire(&cpus[i].rq.lock);
    if (cpus[i].rq.count < min_procs)
    {
      min_procs = cpus[i].rq.count;
      target_cpu = i;
    }
    release(&cpus[i].rq.lock);
  }

  // Make process runnable
  np->state = RUNNABLE;
  np->cpu = target_cpu;
  np->last_runnable_tick = ticks;
  rq_add(&cpus[target_cpu].rq, np);
  release(&ptable_lock);

  return pid;
}

// Terminate the calling process.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  // Prevent init process from exiting
  if (curproc == initproc)
    panic("init exiting");

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

  // Acquire process table lock
  acquire(&ptable_lock);

  // Wake parent process
  wakeup1(curproc->parent);

  // Reassign child processes to init
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Mark process as zombie
  curproc->state = ZOMBIE;
  curproc->completion_time = ticks;

  // Validate CPU assignment
  if (curproc->cpu < 0 || curproc->cpu >= ncpu)
    panic("exit: invalid CPU assignment");

  // Remove from runqueue
  rq_remove(&cpus[curproc->cpu].rq, curproc);
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its PID.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  // Acquire process table lock
  acquire(&ptable_lock);

  for (;;)
  {
    havekids = 0;
    // Check for zombie children
    for (p = ptable; p < &ptable[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Clean up zombie child
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

        // Validate CPU assignment
        if (p->cpu < 0 || p->cpu >= ncpu)
          panic("wait: invalid CPU assignment");

        rq_remove(&cpus[p->cpu].rq, p);
        release(&ptable_lock);
        return pid;
      }
    }

    // No children or killed: return -1
    if (!havekids || curproc->killed)
    {
      release(&ptable_lock);
      return -1;
    }

    // Sleep until a child exits
    sleep(curproc, &ptable_lock);
  }
}

// Update process priorities based on aging and lifetime.
void update_priorities(void)
{
  // Acquire process table lock
  acquire(&ptable_lock);

  // Iterate through process table
  for (struct proc *p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p && (p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING))
    {
      // Kill long-running processes (except PID 1, 2)
      if (ticks - p->creation_time > 10000 && p->pid != 1 && p->pid != 2)
      {
        p->killed = 1;
        if (p->state == SLEEPING)
        {
          p->state = RUNNABLE;
          p->last_runnable_tick = ticks;
          if (p->cpu < 0 || p->cpu >= ncpu)
            panic("update_priorities: invalid CPU assignment");
          rq_add(&cpus[p->cpu].rq, p);
        }
        continue;
      }

      // Reset high-PID processes to priority 5
      if (p->pid > 100 && p->priority != 5)
      {
        if (p->state == RUNNABLE)
          rq_remove(&cpus[p->cpu].rq, p);
        p->priority = 5;
        if (p->state == RUNNABLE)
          rq_add(&cpus[p->cpu].rq, p);
      }

      // Increment waiting time
      p->wait_ticks++;

      // Age processes: increase priority after 50 ticks
      if (p->wait_ticks >= 50)
      {
        if (p->priority > 0 && p->pid <= 100)
        {
          if (p->state == RUNNABLE)
            rq_remove(&cpus[p->cpu].rq, p);
          p->priority--;
          if (p->state == RUNNABLE)
            rq_add(&cpus[p->cpu].rq, p);
        }
        p->wait_ticks = 0;
      }
    }
  }

  release(&ptable_lock);
}

// Schedule processes on the current CPU.
void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    // Disable interrupts
    cli();

    // Update process priorities
    update_priorities();

    // Acquire process table lock
    acquire(&ptable_lock);

    // Select next process from runqueue
    struct proc *p = rq_select(&c->rq);
    if (!p)
    {
      release(&ptable_lock);
      sti();
      continue;
    }

    // Set current process
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    // Update timing fields
    p->waiting_time += ticks - p->last_runnable_tick;
    p->last_runnable_tick = ticks;
    if (!p->has_run)
    {
      p->first_run_time = ticks;
      p->has_run = 1;
    }

    // Log scheduling event
    if (log_index < LOG_SIZE)
    {
      log_schedule(ticks, p->pid, p->priority, context_switches + 1);
    }

    // Increment context switch counter
    context_switches++;

    // Switch to process context
    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Clear current process
    c->proc = 0;
    release(&ptable_lock);

    // Re-enable interrupts
    sti();
  }
}

// Switch to the scheduler context.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  // Validate scheduling conditions
  if (!holding(&ptable_lock))
    panic("sched ptable_lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");

  // Re-add runnable process to runqueue
  if (p->state == RUNNABLE)
  {
    if (p->cpu < 0 || p->cpu >= ncpu)
      panic("sched: invalid CPU assignment");
    rq_add(&cpus[p->cpu].rq, p);
  }

  // Switch to scheduler context
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Yield the CPU to other processes.
void yield(void)
{
  struct proc *p = myproc();

  // Validate CPU assignment
  if (p->cpu < 0 || p->cpu >= ncpu)
    panic("yield: invalid CPU assignment");

  // Make process runnable and schedule
  acquire(&ptable_lock);
  p->state = RUNNABLE;
  p->last_runnable_tick = ticks;
  sched();
  release(&ptable_lock);
}

// Handle return from fork system call.
void forkret(void)
{
  static int first = 1;

  // Release lock acquired by allocproc
  release(&ptable_lock);

  // Initialize file system on first call
  if (first)
  {
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
}

// Put the calling process to sleep on a channel.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Validate inputs
  if (!p)
    panic("sleep");
  if (!lk)
    panic("sleep without lk");

  // Acquire process table lock if needed
  if (lk != &ptable_lock)
  {
    acquire(&ptable_lock);
    release(lk);
  }

  // Set sleep state and remove from runqueue
  p->chan = chan;
  p->state = SLEEPING;
  if (p->cpu < 0 || p->cpu >= ncpu)
    panic("sleep: invalid CPU assignment");
  rq_remove(&cpus[p->cpu].rq, p);

  sched();

  // Clear channel
  p->chan = 0;

  // Release locks appropriately
  if (lk != &ptable_lock)
  {
    release(&ptable_lock);
    acquire(lk);
  }
}

// Wake up processes sleeping on a channel (internal).
static void wakeup1(void *chan)
{
  struct proc *p;

  // Check all processes in table
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == SLEEPING && p->chan == chan)
    {
      // Make process runnable
      p->state = RUNNABLE;
      p->last_runnable_tick = ticks;

      // Adjust priority for certain conditions
      if (p->priority > 0 && p->priority != 5)
      {
        rq_remove(&cpus[p->cpu].rq, p);
        p->priority = 0;
        rq_add(&cpus[p->cpu].rq, p);
      }

      // Validate CPU assignment
      if (p->cpu < 0 || p->cpu >= ncpu)
        panic("wakeup1: invalid CPU assignment");

      rq_add(&cpus[p->cpu].rq, p);
    }
  }
}

// Wake up processes sleeping on a channel.
void wakeup(void *chan)
{
  // Acquire lock and call internal wakeup
  acquire(&ptable_lock);
  wakeup1(chan);
  release(&ptable_lock);
}

// Terminate a process by PID.
int kill(int pid)
{
  struct proc *p;

  // Acquire process table lock
  acquire(&ptable_lock);

  // Find process with matching PID
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Make sleeping process runnable
        p->state = RUNNABLE;
        p->last_runnable_tick = ticks;
        if (p->cpu < 0 || p->cpu >= ncpu)
          panic("kill: invalid CPU assignment");
        rq_add(&cpus[p->cpu].rq, p);
      }
      release(&ptable_lock);
      return 0;
    }
  }

  release(&ptable_lock);
  return -1;
}

// Print process information for debugging.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused", [EMBRYO] "embryo", [SLEEPING] "sleep ", [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;
  uint pc[10];

  // Iterate through process table
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;

    // Determine state string
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    // Print process details
    cprintf("%d %s %s priority=%d\n", p->pid, state, p->name, p->priority);

    // Print call stack for sleeping processes
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (int i = 0; i < 10 && pc[i]; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}