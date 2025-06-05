#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "runqueue.h"

struct proc ptable[NPROC];
struct spinlock ptable_lock;
static struct proc *initproc;
int nextpid = 1;
int context_switches = 0;

extern void forkret(void);
extern void trapret(void);
static void wakeup1(void *chan);

#define LOG_SIZE 100
struct
{
  int tick;
  int pid;
  int priority;
  int cs_count;
} sched_log[LOG_SIZE];
int log_index = 0;

extern void forkret(void);
extern void trapret(void);
static void wakeup1(void *chan);

void log_schedule(int tick, int pid, int priority, int cs_count)
{
  if (log_index < LOG_SIZE)
  {
    sched_log[log_index].tick = tick;
    sched_log[log_index].pid = pid;
    sched_log[log_index].priority = priority;
    sched_log[log_index].cs_count = cs_count;
    log_index++;
  }
}

void print_sched_log(void)
{
  for (int i = 0; i < log_index; i++)
  {
    // cprintf("Tick %d: Switch to PID %d, Priority %d, CS %d\n",
    //         sched_log[i].tick, sched_log[i].pid, sched_log[i].priority, sched_log[i].cs_count);
  }
  log_index = 0;
}

void pinit(void)
{
  initlock(&ptable_lock, "ptable");
  for (int i = 0; i < ncpu; i++)
    rq_init(&cpus[i].rq);
}

int cpuid(void)
{
  return mycpu() - cpus;
}

struct cpu *mycpu(void)
{
  int apicid, i;
  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");
  apicid = lapicid();
  for (i = 0; i < ncpu; ++i)
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  panic("unknown apicid\n");
}

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
      p->pid = nextpid++;
      p->priority = 5;
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

      if ((p->kstack = kalloc()) == 0)
      {
        acquire(&ptable_lock);
        p->state = UNUSED;
        release(&ptable_lock);
        return 0;
      }
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
  p->tf->eip = 0;
  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  acquire(&ptable_lock);
  p->state = RUNNABLE;
  p->cpu = 0;
  p->last_runnable_tick = ticks;
  rq_add(&cpus[0].rq, p);
  release(&ptable_lock);
}

int growproc(int n)
{
  // Same as lottery scheduler
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
  np->state = RUNNABLE;
  np->cpu = target_cpu;
  np->last_runnable_tick = ticks;
  rq_add(&cpus[target_cpu].rq, np);
  release(&ptable_lock);
  return pid;
}

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
  curproc->completion_time = ticks;
  if (curproc->cpu < 0 || curproc->cpu >= ncpu)
    panic("exit: invalid CPU assignment");
  rq_remove(&cpus[curproc->cpu].rq, curproc);
  sched();
  panic("zombie exit");
}

int wait(void)
{
  // Same as lottery scheduler, with CPU assignment checks
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

void update_priorities(void)
{
  acquire(&ptable_lock);
  for (struct proc *p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p && (p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING))
    {
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
      if (p->pid > 100 && p->priority != 5)
      {
        if (p->state == RUNNABLE)
          rq_remove(&cpus[p->cpu].rq, p);
        p->priority = 5;
        if (p->state == RUNNABLE)
          rq_add(&cpus[p->cpu].rq, p);
      }
      p->wait_ticks++;
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

void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    cli();
    update_priorities();
    acquire(&ptable_lock);
    struct proc *p = rq_select(&c->rq);
    if (!p)
    {
      release(&ptable_lock);
      sti();
      continue;
    }
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    p->waiting_time += ticks - p->last_runnable_tick;
    p->last_runnable_tick = ticks;
    if (!p->has_run)
    {
      p->first_run_time = ticks;
      p->has_run = 1;
    }
    if (log_index < LOG_SIZE)
    {
      log_schedule(ticks, p->pid, p->priority, context_switches + 1);
    }
    context_switches++; // Increment context switch counter
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;
    release(&ptable_lock);
    sti();
  }
}

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

void yield(void)
{
  struct proc *p = myproc();
  if (p->cpu < 0 || p->cpu >= ncpu)
    panic("yield: invalid CPU assignment");
  acquire(&ptable_lock);
  p->state = RUNNABLE;
  p->last_runnable_tick = ticks;
  sched();
  release(&ptable_lock);
}

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

void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  if (!p)
    panic("sleep");
  if (!lk)
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
  rq_remove(&cpus[p->cpu].rq, p);
  sched();
  p->chan = 0;
  if (lk != &ptable_lock)
  {
    release(&ptable_lock);
    acquire(lk);
  }
}

static void wakeup1(void *chan)
{
  struct proc *p;
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      p->last_runnable_tick = ticks;
      if (p->priority > 0 && p->priority != 5)
      {
        rq_remove(&cpus[p->cpu].rq, p);
        p->priority = 0;
        rq_add(&cpus[p->cpu].rq, p);
      }
      if (p->cpu < 0 || p->cpu >= ncpu)
        panic("wakeup1: invalid CPU assignment");
      rq_add(&cpus[p->cpu].rq, p);
    }
  }
}

void wakeup(void *chan)
{
  acquire(&ptable_lock);
  wakeup1(chan);
  release(&ptable_lock);
}

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

void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused", [EMBRYO] "embryo", [SLEEPING] "sleep ", [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
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
    cprintf("%d %s %s priority=%d\n", p->pid, state, p->name, p->priority);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (int i = 0; i < 10 && pc[i]; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}