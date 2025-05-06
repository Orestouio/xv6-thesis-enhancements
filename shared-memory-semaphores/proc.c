#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// Shared memory table
struct shm shmtable[NSHM];

// Semaphore table
struct sem semtable[NSEM];

// Initialize shared memory table
void shminit(void)
{
  for (int i = 0; i < NSHM; i++)
  {
    shmtable[i].in_use = 0;
    initlock(&shmtable[i].lock, "shm");
  }
}

// Initialize semaphore table
void seminit(void)
{
  for (int i = 0; i < NSEM; i++)
  {
    semtable[i].in_use = 0;
    initlock(&semtable[i].lock, "sem");
    semtable[i].value = 0;
    semtable[i].queue_head = 0;
    semtable[i].queue_tail = 0;
    for (int j = 0; j < NPROC; j++)
    {
      semtable[i].queue[j] = 0;
    }
  }
}

// System call: sem_init(value)
// Allocates a semaphore, initializes it with the given value, and returns its ID
// Returns the semaphore ID (index in semtable) on success, -1 on failure
int sys_sem_init(void)
{
  int value;
  struct proc *curproc = myproc();

  // Get argument
  if (argint(0, &value) < 0 || value < 0)
    return -1;

  // Check if the process can open more semaphores
  if (curproc->sem_count >= MAX_SEM)
    return -1;

  acquire(&ptable.lock);

  // Find a free semaphore slot
  int sem_id = -1;
  for (int i = 0; i < NSEM; i++)
  {
    if (!semtable[i].in_use)
    {
      sem_id = i;
      break;
    }
  }

  if (sem_id == -1)
  {
    release(&ptable.lock);
    return -1; // No free semaphore slots
  }

  acquire(&semtable[sem_id].lock);

  // Initialize the semaphore
  semtable[sem_id].in_use = 1;
  semtable[sem_id].value = value;
  semtable[sem_id].queue_head = 0;
  semtable[sem_id].queue_tail = 0;
  for (int i = 0; i < NPROC; i++)
  {
    semtable[sem_id].queue[i] = 0;
  }

  // Add the semaphore ID to the process's list
  curproc->sem_ids[curproc->sem_count] = sem_id;
  curproc->sem_count++;

  release(&semtable[sem_id].lock);
  release(&ptable.lock);

  return sem_id;
}

// System call: sem_wait(sem_id)
// Decrements the semaphore's value; if the value becomes negative, the process blocks
// Returns 0 on success, -1 on failure
int sys_sem_wait(void)
{
  int sem_id;
  struct proc *curproc = myproc();

  // Get argument
  if (argint(0, &sem_id) < 0 || sem_id < 0 || sem_id >= NSEM)
    return -1;

  // Check if the semaphore is in use
  acquire(&ptable.lock);
  if (!semtable[sem_id].in_use)
  {
    release(&ptable.lock);
    return -1;
  }
  release(&ptable.lock);

  acquire(&semtable[sem_id].lock);

  semtable[sem_id].value--;

  if (semtable[sem_id].value < 0)
  {
    // Add the process to the wait queue
    if ((semtable[sem_id].queue_tail + 1) % NPROC == semtable[sem_id].queue_head)
    {
      // Queue is full
      release(&semtable[sem_id].lock);
      return -1;
    }

    semtable[sem_id].queue[semtable[sem_id].queue_tail] = curproc;
    semtable[sem_id].queue_tail = (semtable[sem_id].queue_tail + 1) % NPROC;

    // Block the process
    sleep(curproc, &semtable[sem_id].lock);
  }

  release(&semtable[sem_id].lock);
  return 0;
}

// System call: sem_post(sem_id)
// Increments the semaphore's value; if there are waiting processes, wakes one up
// Returns 0 on success, -1 on failure
int sys_sem_post(void)
{
  int sem_id;
  struct proc *p = 0;

  // Get argument
  if (argint(0, &sem_id) < 0 || sem_id < 0 || sem_id >= NSEM)
    return -1;

  // Check if the semaphore is in use
  acquire(&ptable.lock);
  if (!semtable[sem_id].in_use)
  {
    release(&ptable.lock);
    return -1;
  }
  release(&ptable.lock);

  acquire(&semtable[sem_id].lock);

  semtable[sem_id].value++;

  if (semtable[sem_id].value <= 0)
  {
    // There are waiting processes; wake one up
    if (semtable[sem_id].queue_head != semtable[sem_id].queue_tail)
    {
      p = semtable[sem_id].queue[semtable[sem_id].queue_head];
      semtable[sem_id].queue[semtable[sem_id].queue_head] = 0;
      semtable[sem_id].queue_head = (semtable[sem_id].queue_head + 1) % NPROC;
    }
  }

  release(&semtable[sem_id].lock);

  // Wake up the process after releasing the semaphore lock
  if (p)
  {
    wakeup(p);
  }

  return 0;
}

// System call: shm_open(name, size)
// Creates or opens a shared memory object and maps it into the process's address space
// Returns the virtual address where the shared memory is mapped, or -1 on error
int sys_shm_open(void)
{
  char *name;
  int size;
  struct proc *curproc = myproc();
  void *va = (void *)(0x60000000 + curproc->shm_count * PGSIZE); // Increment VA for each mapping

  // Get arguments
  if (argstr(0, &name) < 0 || argint(1, &size) < 0)
    return -1;

  // Validate size
  if (size <= 0 || size > PGSIZE) // Limit to one page for simplicity
    return -1;

  acquire(&ptable.lock);

  // Search for an existing shared memory object with the given name
  int shm_idx = -1;
  for (int i = 0; i < NSHM; i++)
  {
    if (shmtable[i].in_use && strncmp(shmtable[i].name, name, sizeof(shmtable[i].name)) == 0)
    {
      shm_idx = i;
      break;
    }
  }

  // If not found, create a new shared memory object
  if (shm_idx == -1)
  {
    for (int i = 0; i < NSHM; i++)
    {
      if (!shmtable[i].in_use)
      {
        shm_idx = i;
        break;
      }
    }
    if (shm_idx == -1)
    { // No free slots
      release(&ptable.lock);
      return -1;
    }

    // Initialize the new shared memory object
    acquire(&shmtable[shm_idx].lock);
    shmtable[shm_idx].in_use = 1;
    strncpy(shmtable[shm_idx].name, name, sizeof(shmtable[shm_idx].name) - 1);
    shmtable[shm_idx].name[sizeof(shmtable[shm_idx].name) - 1] = '\0';
    shmtable[shm_idx].size = size;
    shmtable[shm_idx].ref_count = 0;
    shmtable[shm_idx].phys_addr = kalloc();
    if (shmtable[shm_idx].phys_addr == 0)
    {
      shmtable[shm_idx].in_use = 0;
      release(&shmtable[shm_idx].lock);
      release(&ptable.lock);
      return -1;
    }
    memset(shmtable[shm_idx].phys_addr, 0, PGSIZE); // Zero the memory
    release(&shmtable[shm_idx].lock);
  }

  // Check if the process has room for more shared memory mappings
  if (curproc->shm_count >= MAX_SHM_MAPPINGS)
  {
    release(&ptable.lock);
    return -1;
  }

  // Map the shared memory into the process's address space
  acquire(&shmtable[shm_idx].lock);
  if (mappages(curproc->pgdir, va, PGSIZE, V2P(shmtable[shm_idx].phys_addr), PTE_W | PTE_U) < 0)
  {
    if (shmtable[shm_idx].ref_count == 0)
    {
      kfree(shmtable[shm_idx].phys_addr);
      shmtable[shm_idx].in_use = 0;
    }
    release(&shmtable[shm_idx].lock);
    release(&ptable.lock);
    return -1;
  }

  // Update process's shared memory mappings
  curproc->shm_mappings[curproc->shm_count] = va;
  curproc->shm_objects[curproc->shm_count] = &shmtable[shm_idx];
  curproc->shm_count++;
  shmtable[shm_idx].ref_count++;

  release(&shmtable[shm_idx].lock);
  release(&ptable.lock);
  return (int)va;
}

// System call: shm_close(addr)
// Unmaps the shared memory region from the process's address space
// Returns 0 on success, -1 on error
int sys_shm_close(void)
{
  int addr;
  struct proc *curproc = myproc();

  // Get argument
  if (argint(0, &addr) < 0)
    return -1;

  acquire(&ptable.lock);

  // Find the shared memory mapping
  int mapping_idx = -1;
  for (int i = 0; i < curproc->shm_count; i++)
  {
    if ((int)curproc->shm_mappings[i] == addr)
    {
      mapping_idx = i;
      break;
    }
  }

  if (mapping_idx == -1)
  {
    release(&ptable.lock);
    return -1;
  }

  struct shm *shm = curproc->shm_objects[mapping_idx];
  acquire(&shm->lock);

  // Unmap the shared memory
  pde_t *pde = &curproc->pgdir[PDX(addr)];
  pte_t *pte = (pte_t *)P2V(PTE_ADDR(*pde));
  pte[PTX(addr)] = 0; // Clear the page table entry

  // Update process's mappings
  for (int i = mapping_idx; i < curproc->shm_count - 1; i++)
  {
    curproc->shm_mappings[i] = curproc->shm_mappings[i + 1];
    curproc->shm_objects[i] = curproc->shm_objects[i + 1];
  }
  curproc->shm_count--;

  // Decrement reference count and free if necessary
  shm->ref_count--;
  if (shm->ref_count == 0)
  {
    cprintf("[Kernel Debug] Shared memory %s freed\n", shm->name);
    kfree(shm->phys_addr);
    shm->in_use = 0;
  }

  release(&shm->lock);
  release(&ptable.lock);
  return 0;
}

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
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
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

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

  // Initialize shared memory mappings
  p->shm_count = 0;

  // Initialize semaphore tracking
  p->sem_count = 0;

  return p;
}

// PAGEBREAK: 32
//  Set up first user process.
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

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);

  // Initialize shared memory table
  shminit();

  // Initialize semaphore table
  seminit();
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

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
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

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  // Copy shared memory mappings to the child
  np->shm_count = curproc->shm_count;
  for (i = 0; i < curproc->shm_count; i++)
  {
    np->shm_mappings[i] = curproc->shm_mappings[i];
    np->shm_objects[i] = curproc->shm_objects[i];
    // Map the shared memory into the child's address space
    void *va = curproc->shm_mappings[i];
    struct shm *shm = curproc->shm_objects[i];
    acquire(&shm->lock);
    if (mappages(np->pgdir, va, PGSIZE, V2P(shm->phys_addr), PTE_W | PTE_U) < 0)
    {
      // If mapping fails, clean up and fail the fork
      for (int j = 0; j < i; j++)
      {
        pde_t *pde = &np->pgdir[PDX(np->shm_mappings[j])];
        pte_t *pte = (pte_t *)P2V(PTE_ADDR(*pde));
        pte[PTX(np->shm_mappings[j])] = 0; // Unmap on failure
      }
      release(&shm->lock);
      kfree(np->kstack);
      np->kstack = 0;
      freevm(np->pgdir);
      np->state = UNUSED;
      return -1;
    }
    shm->ref_count++; // Increment reference count for the child
    release(&shm->lock);
  }

  // Copy semaphore IDs to the child
  np->sem_count = curproc->sem_count;
  for (i = 0; i < curproc->sem_count; i++)
  {
    np->sem_ids[i] = curproc->sem_ids[i];
  }

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

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

  // Clean up semaphores
  acquire(&ptable.lock);
  for (int i = 0; i < curproc->sem_count; i++)
  {
    int sem_id = curproc->sem_ids[i];
    if (sem_id < 0 || sem_id >= NSEM || !semtable[sem_id].in_use)
      continue;

    acquire(&semtable[sem_id].lock);

    // Clear the wait queue
    while (semtable[sem_id].queue_head != semtable[sem_id].queue_tail)
    {
      struct proc *wp = semtable[sem_id].queue[semtable[sem_id].queue_head];
      semtable[sem_id].queue[semtable[sem_id].queue_head] = 0;
      semtable[sem_id].queue_head = (semtable[sem_id].queue_head + 1) % NPROC;
      if (wp != curproc)
      {
        wakeup1(wp); // Wake up other waiting processes
      }
    }

    // Free the semaphore
    semtable[sem_id].in_use = 0;
    semtable[sem_id].value = 0;
    cprintf("[Kernel Debug] Semaphore %d freed\n", sem_id);

    release(&semtable[sem_id].lock);
  }
  curproc->sem_count = 0;
  release(&ptable.lock);

  // Close shared memory mappings
  acquire(&ptable.lock);
  while (curproc->shm_count > 0)
  {
    int mapping_idx = 0; // Always process the first mapping
    int addr = (int)curproc->shm_mappings[mapping_idx];
    struct shm *shm = curproc->shm_objects[mapping_idx];

    acquire(&shm->lock);

    // Unmap the shared memory
    pde_t *pde = &curproc->pgdir[PDX(addr)];
    pte_t *pte = (pte_t *)P2V(PTE_ADDR(*pde));
    pte[PTX(addr)] = 0; // Clear the page table entry

    // Update process's mappings
    for (int i = mapping_idx; i < curproc->shm_count - 1; i++)
    {
      curproc->shm_mappings[i] = curproc->shm_mappings[i + 1];
      curproc->shm_objects[i] = curproc->shm_objects[i + 1];
    }
    curproc->shm_count--;

    // Decrement reference count and free if necessary
    shm->ref_count--;
    if (shm->ref_count == 0)
    {
      cprintf("[Kernel Debug] Shared memory %s freed\n", shm->name);
      kfree(shm->phys_addr);
      shm->in_use = 0;
    }

    release(&shm->lock);
  }
  release(&ptable.lock);

  // Close all open files.
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

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
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

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); // DOC: wait-sleep
  }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
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

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); // DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
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

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        // DOC: sleeplock0
    acquire(&ptable.lock); // DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
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

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
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