#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct ptable ptable;
static struct proc *initproc;

int nextpid = 1;
int context_switches = 0;

// Priority queue: Array of linked lists for each priority level (0-10)
struct
{
  struct proc *head[11]; // Head of the linked list for each priority
  struct proc *tail[11]; // Tail of the linked list for each priority
} priority_queue;

// FIFO queue for short-lived processes
struct
{
  struct proc *head;
  struct proc *tail;
} short_lived_queue;

// Function prototypes
static void wakeup1(void *chan);
void update_priorities(void);
extern void trapret(void);

void init_priority_queue(void)
{
  for (int i = 0; i < 11; i++)
  {
    priority_queue.head[i] = 0;
    priority_queue.tail[i] = 0;
  }
  short_lived_queue.head = 0;
  short_lived_queue.tail = 0;
}

// Add a process to the priority queue based on its priority
void add_to_priority_queue(struct proc *p)
{
  int prio = p->priority;
  p->next = 0; // Clear next pointer
  if (priority_queue.head[prio] == 0)
  {
    priority_queue.head[prio] = p;
    priority_queue.tail[prio] = p;
  }
  else
  {
    priority_queue.tail[prio]->next = p;
    priority_queue.tail[prio] = p;
  }
}

// Remove a process from the priority queue
void remove_from_priority_queue(struct proc *p)
{
  int prio = p->priority;
  struct proc *curr = priority_queue.head[prio];
  struct proc *prev = 0;

  while (curr != 0)
  {
    if (curr == p)
    {
      if (prev == 0)
      {
        priority_queue.head[prio] = curr->next;
      }
      else
      {
        prev->next = curr->next;
      }
      if (curr->next == 0)
      {
        priority_queue.tail[prio] = prev;
      }
      p->next = 0;
      break;
    }
    prev = curr;
    curr = curr->next;
  }
}

// Add a process to the short-lived FIFO queue
void add_to_short_lived_queue(struct proc *p)
{
  p->next = 0;
  if (short_lived_queue.head == 0)
  {
    short_lived_queue.head = p;
    short_lived_queue.tail = p;
  }
  else
  {
    short_lived_queue.tail->next = p;
    short_lived_queue.tail = p;
  }
}

// Remove a process from the short-lived FIFO queue
void remove_from_short_lived_queue(struct proc *p)
{
  struct proc *curr = short_lived_queue.head;
  struct proc *prev = 0;

  while (curr != 0)
  {
    if (curr == p)
    {
      if (prev == 0)
      {
        short_lived_queue.head = curr->next;
      }
      else
      {
        prev->next = curr->next;
      }
      if (curr->next == 0)
      {
        short_lived_queue.tail = prev;
      }
      p->next = 0;
      break;
    }
    prev = curr;
    curr = curr->next;
  }
}

#define LOG_SIZE 100

struct
{
  int tick;
  int pid;
  int priority;
  int cs_count;
} sched_log[LOG_SIZE];
int log_index = 0;

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

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
  for (int i = 0; i < NPROC; i++)
    ptable.proc[i] = 0; // Initialize all pointers to null
  init_priority_queue();
}

int cpuid()
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
  {
    if (cpus[i].apicid == apicid) // Match only on APIC ID
    {
      return &cpus[i];
    }
  }
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
  int i;

  acquire(&ptable.lock);
  for (i = 0; i < NPROC; i++)
    if (ptable.proc[i] == 0 || ptable.proc[i]->state == UNUSED)
      break;
  if (i == NPROC)
  {
    release(&ptable.lock);
    return 0;
  }

  p = (struct proc *)kalloc();
  if (p == 0)
  {
    release(&ptable.lock);
    return 0;
  }
  memset(p, 0, sizeof(*p));
  ptable.proc[i] = p;

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

  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    ptable.proc[i] = 0;
    kfree((char *)p);
    release(&ptable.lock);
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

  release(&ptable.lock);
  return p;
}

void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p; // Temporarily set to PID 1
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

  acquire(&ptable.lock);
  p->state = RUNNABLE;
  p->last_runnable_tick = ticks;
  add_to_priority_queue(p);
  release(&ptable.lock);
}

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
    for (i = 0; i < NPROC; i++)
    {
      if (ptable.proc[i] == np)
      {
        ptable.proc[i] = 0;
        break;
      }
    }
    kfree((char *)np);
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

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  np->last_runnable_tick = ticks;
  if (np->priority == 5)
  {
    add_to_short_lived_queue(np);
  }
  else
  {
    add_to_priority_queue(np);
  }
  release(&ptable.lock);
  return pid;
}

void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;
  int i;

  if (curproc->pid == 1)
  {
    acquire(&ptable.lock);
    for (i = 0; i < NPROC; i++)
    {
      p = ptable.proc[i];
      if (p && p->pid == 2)
      {
        char *expected_name = "init";
        int match = 1;
        for (int j = 0; j < sizeof(p->name); j++)
        {
          if (expected_name[j] == '\0' && p->name[j] == '\0')
            break;
          if (p->name[j] != expected_name[j])
          {
            match = 0;
            break;
          }
          if (expected_name[j] == '\0')
            break;
        }
        if (match)
        {
          initproc = p;
          break;
        }
      }
    }
    release(&ptable.lock);
  }

  if (curproc == initproc)
  {
    cprintf("exit: init (pid=%d) exiting, killed=%d\n", curproc->pid, curproc->killed);
    panic("init exiting");
  }

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

  acquire(&ptable.lock);

  if (curproc->state == RUNNABLE)
  {
    if (curproc->priority == 5)
    {
      remove_from_short_lived_queue(curproc);
    }
    else
    {
      remove_from_priority_queue(curproc);
    }
  }

  wakeup1(curproc->parent);

  for (i = 0; i < NPROC; i++)
  {
    p = ptable.proc[i];
    if (p && p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  curproc->completion_time = ticks;
  curproc->state = ZOMBIE;
  sched();

  for (i = 0; i < NPROC; i++)
  {
    if (ptable.proc[i] == curproc)
    {
      kfree(curproc->kstack);
      curproc->kstack = 0;
      freevm(curproc->pgdir);
      curproc->pid = 0;
      curproc->parent = 0;
      curproc->name[0] = '\0';
      curproc->killed = 0;
      curproc->state = UNUSED;
      ptable.proc[i] = 0;
      kfree((char *)curproc);
      break;
    }
  }
  release(&ptable.lock);
}

int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  int i;

  acquire(&ptable.lock);
  for (;;)
  {
    havekids = 0;
    for (i = 0; i < NPROC; i++)
    {
      p = ptable.proc[i];
      if (p == 0 || p->parent != curproc)
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
        ptable.proc[i] = 0;
        kfree((char *)p);
        release(&ptable.lock);
        return pid;
      }
    }

    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock);
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

void scheduler(void)
{
  struct cpu *c;
  // cprintf("scheduler: before mycpu\n");
  c = mycpu();
  // cprintf("scheduler: after mycpu, c->apicid=%d\n", c->apicid);
  c->proc = 0;

  for (;;)
  {
    update_priorities();
    acquire(&ptable.lock);

    struct proc *selected = short_lived_queue.head;
    if (selected != 0)
    {
      remove_from_short_lived_queue(selected);
    }
    else
    {
      int highest_priority = 11;
      for (int i = 0; i < 11; i++)
      {
        if (priority_queue.head[i] != 0)
        {
          highest_priority = i;
          break;
        }
      }

      if (highest_priority == 11)
      {
        release(&ptable.lock);
        sti();
        continue;
      }

      selected = priority_queue.head[highest_priority];
      remove_from_priority_queue(selected);
    }

    selected->waiting_time += ticks - selected->last_runnable_tick;
    selected->last_runnable_tick = ticks;
    if (selected->has_run == 0)
    {
      selected->first_run_time = ticks;
      selected->has_run = 1;
    }

    c->proc = selected;
    switchuvm(selected);
    selected->state = RUNNING;
    // cprintf("scheduler: scheduling pid=%d, name=%s, priority=%d\n", selected->pid, selected->name, selected->priority);
    log_schedule(ticks, selected->pid, selected->priority, context_switches + 1);
    context_switches++;
    // cprintf("scheduler: before swtch, mycpu()->apicid=%d\n", mycpu()->apicid);
    swtch(&(c->scheduler), selected->context);
    // cprintf("scheduler: after swtch, mycpu()->apicid=%d\n", c->apicid);
    switchkvm();
    c->proc = 0;

    release(&ptable.lock);
    sti();
  }
}

void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
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

void yield(void)
{
  // cprintf("yield: acquiring ptable.lock on cpu%d\n", mycpu()->apicid);
  acquire(&ptable.lock);
  myproc()->state = RUNNABLE;
  myproc()->last_runnable_tick = ticks;
  if (myproc()->priority == 5)
  {
    add_to_short_lived_queue(myproc());
  }
  else
  {
    add_to_priority_queue(myproc());
  }
  sched();
  // cprintf("yield: releasing ptable.lock on cpu%d\n", mycpu()->apicid);
  release(&ptable.lock);
}

void forkret(void)
{
  static int first = 1;
  release(&ptable.lock);

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

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  if (lk != &ptable.lock)
  {
    acquire(&ptable.lock);
    release(lk);
  }
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = 0;

  if (lk != &ptable.lock)
  {
    release(&ptable.lock);
    acquire(lk);
  }
}

static void wakeup1(void *chan)
{
  struct proc *p;
  int i;
  for (i = 0; i < NPROC; i++)
  {
    p = ptable.proc[i];
    if (p && p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      p->last_runnable_tick = ticks;
      if (p->priority > 0 && p->priority != 5)
      {
        remove_from_priority_queue(p);
        p->priority = 0;
        add_to_priority_queue(p);
      }
      if (p->priority == 5)
      {
        add_to_short_lived_queue(p);
      }
      else
      {
        add_to_priority_queue(p);
      }
    }
  }
}

void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

int kill(int pid)
{
  struct proc *p;
  int i;

  acquire(&ptable.lock);
  for (i = 0; i < NPROC; i++)
  {
    p = ptable.proc[i];
    if (p && p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        p->state = RUNNABLE;
        p->last_runnable_tick = ticks;
        if (p->priority == 5)
        {
          add_to_short_lived_queue(p);
        }
        else
        {
          add_to_priority_queue(p);
        }
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i, j;
  struct proc *p;
  char *state;
  uint pc[10];

  for (j = 0; j < NPROC; j++)
  {
    p = ptable.proc[j];
    if (p == 0 || p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void update_priorities(void)
{
  struct proc *p;
  int i;
  // cprintf("update_priorities: starting\n");
  for (i = 0; i < NPROC; i++)
  {
    p = ptable.proc[i];
    if (p && (p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING))
    {
      if (ticks - p->creation_time > 10000 && p->pid != 1 && p->pid != 2)
      {
        p->killed = 1;
        if (p->state == SLEEPING)
        {
          p->state = RUNNABLE;
          p->last_runnable_tick = ticks;
          if (p->priority == 5)
          {
            add_to_short_lived_queue(p);
          }
          else
          {
            add_to_priority_queue(p);
          }
        }
        continue;
      }

      if (p->pid > 100 && p->priority != 5)
      {
        if (p->state == RUNNABLE)
        {
          remove_from_priority_queue(p);
        }
        p->priority = 5;
        if (p->state == RUNNABLE)
        {
          add_to_short_lived_queue(p);
        }
      }

      p->wait_ticks++;
      if (p->wait_ticks >= 50)
      {
        if (p->priority > 0 && p->pid <= 100)
        {
          if (p->state == RUNNABLE)
          {
            if (p->priority == 5)
            {
              remove_from_short_lived_queue(p);
            }
            else
            {
              remove_from_priority_queue(p);
            }
          }
          p->priority--;
          if (p->state == RUNNABLE)
          {
            if (p->priority == 5)
            {
              add_to_short_lived_queue(p);
            }
            else
            {
              add_to_priority_queue(p);
            }
          }
        }
        p->wait_ticks = 0;
      }
    }
  }
  // cprintf("update_priorities: done\n");
}