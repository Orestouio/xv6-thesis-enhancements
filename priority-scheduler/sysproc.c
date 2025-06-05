/*
 * sysproc.c: Implements system call handlers for process management in xv6.
 * Provides functions for process creation, termination, priority setting,
 * context switch tracking, and scheduling log printing.
 */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

// External declarations from proc.c
extern int context_switches;       // Global context switch counter
extern void print_sched_log(void); // Function to print scheduling log
extern struct proc ptable[NPROC];  // Global process table

// Create a new process by duplicating the calling process.
int sys_fork(void)
{
  return fork();
}

// Terminate the calling process.
int sys_exit(void)
{
  exit();
  return 0; // Not reached
}

// Wait for a child process to exit and return its PID.
int sys_wait(void)
{
  return wait();
}

// Terminate a process with the given PID.
int sys_kill(void)
{
  int pid;
  // Fetch PID from system call argument
  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// Return the PID of the calling process.
int sys_getpid(void)
{
  return myproc()->pid;
}

// Adjust the calling process's memory size by n bytes.
int sys_sbrk(void)
{
  int addr, n;
  // Fetch size increment from argument
  if (argint(0, &n) < 0)
    return -1;

  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;

  return addr;
}

// Suspend the calling process for n ticks.
int sys_sleep(void)
{
  int n;
  uint ticks0;

  // Fetch sleep duration from argument
  if (argint(0, &n) < 0)
    return -1;

  // Acquire ticks lock to safely access global ticks
  acquire(&tickslock);
  ticks0 = ticks;

  // Loop until requested sleep time has elapsed
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }

  release(&tickslock);
  return 0;
}

// Return the current system uptime in ticks.
int sys_uptime(void)
{
  uint xticks;

  // Acquire ticks lock to safely read global ticks
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  return xticks;
}

// Yield the CPU to other processes.
int sys_yield(void)
{
  yield();
  return 0;
}

// Set the priority of a process identified by PID.
int sys_setpriority(void)
{
  int pid, priority;

  // Fetch PID and priority from arguments
  if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;

  // Validate priority range (0-10, 0 highest)
  if (priority < 0 || priority > 10)
    return -1;

  // Acquire process table lock for thread safety
  acquire(&ptable_lock);

  // Search for process with matching PID
  struct proc *p;
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      // Update runqueue if process is runnable
      if (p->state == RUNNABLE)
        rq_remove(&cpus[p->cpu].rq, p);

      // Set new priority
      p->priority = priority;

      // Re-add to runqueue if still runnable
      if (p->state == RUNNABLE)
        rq_add(&cpus[p->cpu].rq, p);

      release(&ptable_lock);
      return 0;
    }
  }

  // Process not found
  release(&ptable_lock);
  return -1;
}

// Return the total number of context switches.
int sys_getcontextswitches(void)
{
  return context_switches;
}

// Print the scheduling log.
int sys_print_sched_log(void)
{
  print_sched_log();
  return 0;
}