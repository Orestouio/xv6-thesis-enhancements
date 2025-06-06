/*
 * sysproc.c - System Call Implementations for xv6 Process Management
 *
 * This file implements system calls related to process management, memory allocation,
 * and scheduling for the xv6 operating system with a lottery scheduler. It provides
 * interfaces for process creation, termination, sleeping, and lottery scheduling
 * operations, such as setting tickets and retrieving process information.
 *
 * System Calls:
 * - sys_fork: Creates a new child process.
 * - sys_exit: Terminates the current process.
 * - sys_wait: Waits for a child process to terminate.
 * - sys_kill: Kills a process by PID.
 * - sys_getpid: Returns the current process's PID.
 * - sys_sbrk: Adjusts the process's memory size.
 * - sys_sleep: Puts the process to sleep for a specified number of ticks.
 * - sys_uptime: Returns the system uptime in ticks.
 * - sys_settickets: Sets the ticket count for the current process (lottery scheduling).
 * - sys_getpinfo: Retrieves scheduling statistics for all processes.
 * - sys_yield: Yields the CPU to another process.
 * - sys_settickets_pid: Sets the ticket count for a process by PID.
 */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

// Process information structure matching user.h for getpinfo system call
struct pinfo
{
  int pid;             // Process ID
  int tickets;         // Number of lottery tickets
  int ticks_scheduled; // Number of times scheduled
};

// External references to the process table and its lock
extern struct proc ptable[NPROC];
extern struct spinlock ptable_lock;

/*
 * sys_fork - Create a new child process
 *
 * Calls fork() to duplicate the current process and returns the child's PID to the parent.
 * Returns: Child's PID (parent), 0 (child), or -1 (error).
 */
int sys_fork(void)
{
  return fork();
}

/*
 * sys_exit - Terminate the current process
 *
 * Calls exit() to terminate the current process, cleaning up resources.
 * Never returns as the process is terminated.
 * Returns: 0 (never reached).
 */
int sys_exit(void)
{
  exit();
  return 0; // Unreachable
}

/*
 * sys_wait - Wait for a child process to terminate
 *
 * Calls wait() to block until a child process exits.
 * Returns: PID of the terminated child, or -1 if no children exist.
 */
int sys_wait(void)
{
  return wait();
}

/*
 * sys_kill - Kill a process by PID
 *
 * Parameters:
 * - pid (via argint): The process ID to kill.
 * Returns: 0 on success, -1 if the PID is invalid or not found.
 */
int sys_kill(void)
{
  int pid;

  // Retrieve the PID argument from the system call
  if (argint(0, &pid) < 0)
  {
    return -1; // Invalid argument
  }

  return kill(pid);
}

/*
 * sys_getpid - Get the current process's PID
 *
 * Returns: The PID of the current process.
 */
int sys_getpid(void)
{
  return myproc()->pid;
}

/*
 * sys_sbrk - Adjust the process's memory size
 *
 * Parameters:
 * - n (via argint): Number of bytes to increase (positive) or decrease (negative) memory.
 * Returns: Old memory size on success, -1 on failure.
 */
int sys_sbrk(void)
{
  int addr;
  int n;

  // Retrieve the size argument
  if (argint(0, &n) < 0)
  {
    return -1; // Invalid argument
  }

  // Get current memory size
  addr = myproc()->sz;

  // Adjust memory size
  if (growproc(n) < 0)
  {
    return -1; // Memory allocation failed
  }

  return addr;
}

/*
 * sys_sleep - Sleep for a specified number of ticks
 *
 * Parameters:
 * - n (via argint): Number of ticks to sleep.
 * Returns: 0 on success, -1 if interrupted or invalid argument.
 */
int sys_sleep(void)
{
  int n;
  uint ticks0;

  // Retrieve the sleep duration
  if (argint(0, &n) < 0)
  {
    return -1; // Invalid argument
  }

  // Acquire the ticks lock to safely access the global tick counter
  acquire(&tickslock);
  ticks0 = ticks;

  // Sleep until the specified number of ticks has passed
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1; // Process was killed
    }
    sleep(&ticks, &tickslock); // Sleep on the ticks channel
  }

  release(&tickslock);
  return 0;
}

/*
 * sys_uptime - Get the system uptime
 *
 * Returns: The number of ticks since system boot.
 */
int sys_uptime(void)
{
  uint xticks;

  // Acquire the ticks lock to safely read the tick counter
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);

  return xticks;
}

/*
 * sys_settickets - Set the ticket count for the current process
 *
 * Parameters:
 * - tickets (via argint): Number of lottery tickets to assign.
 * Returns: 0 on success, -1 if tickets is non-positive or invalid.
 */
int sys_settickets(void)
{
  int tickets;
  struct proc *curproc = myproc();

  // Validate the ticket count
  if (argint(0, &tickets) < 0 || tickets <= 0)
  {
    return -1; // Invalid or non-positive tickets
  }

  // Update the process's ticket count under lock
  acquire(&ptable_lock);
  curproc->tickets = tickets;
  release(&ptable_lock);

  return 0;
}

/*
 * sys_getpinfo - Retrieve scheduling statistics for all processes
 *
 * Parameters:
 * - info (via argptr): Pointer to an array of struct pinfo for NPROC processes.
 * Returns: 0 on success, -1 if the pointer is invalid.
 */
int sys_getpinfo(void)
{
  struct pinfo *info;

  // Validate the user-provided pointer
  if (argptr(0, (void *)&info, sizeof(*info) * NPROC) < 0)
  {
    cprintf("sys_getpinfo: argptr failed\n");
    return -1; // Invalid pointer
  }

  // Copy process information under lock
  acquire(&ptable_lock);
  for (int i = 0; i < NPROC; i++)
  {
    struct proc *p = &ptable[i];
    if (p->pid > 0)
    { // Process has been used
      info[i].pid = p->pid;
      info[i].tickets = p->tickets;
      info[i].ticks_scheduled = p->ticks_scheduled;
    }
    else
    { // Unused process slot
      info[i].pid = 0;
      info[i].tickets = 0;
      info[i].ticks_scheduled = 0;
    }
  }
  release(&ptable_lock);

  return 0;
}

/*
 * sys_yield - Yield the CPU to another process
 *
 * Calls yield() to voluntarily relinquish the CPU.
 * Returns: 0 on success.
 */
int sys_yield(void)
{
  yield();
  return 0;
}

/*
 * sys_settickets_pid - Set the ticket count for a process by PID
 *
 * Parameters:
 * - pid (via argint): Process ID to modify.
 * - tickets (via argint): Number of lottery tickets to assign.
 * Returns: 0 on success, -1 if PID is not found or tickets is invalid.
 */
int sys_settickets_pid(void)
{
  int pid, tickets;
  struct proc *p;

  // Validate PID and ticket count
  if (argint(0, &pid) < 0 || argint(1, &tickets) < 0 || tickets <= 0)
  {
    return -1; // Invalid arguments
  }

  // Search for the process under lock
  acquire(&ptable_lock);
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->tickets = tickets;
      release(&ptable_lock);
      return 0; // Success
    }
  }
  release(&ptable_lock);

  return -1; // PID not found
}