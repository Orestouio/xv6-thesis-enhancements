// sysproc.c
#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"     // For struct proc
#include "spinlock.h" // For struct spinlock in ptable

// Declare ptable - matches proc.c
extern struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
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

int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sysproc.c
int sys_settickets(void)
{
  int pid, tickets;
  struct proc *p;

  if (argint(0, &pid) < 0 || argint(1, &tickets) < 0)
    return -1;
  if (tickets < 1 || tickets > 100)
    return -1;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->tickets = tickets;
      cprintf("pid %d tickets set to %d\n", pid, p->tickets);
      release(&ptable.lock);
      return 0;
    }
  }
  cprintf("pid %d not found\n", pid);
  release(&ptable.lock);
  return -1;
}
