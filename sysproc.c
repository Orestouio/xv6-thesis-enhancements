#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

// Define struct pinfo to match user.h
struct pinfo
{
  int pid;
  int tickets;
  int ticks_scheduled;
};

extern struct proc ptable[NPROC];
extern struct spinlock ptable_lock;

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0;
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

int sys_settickets(void)
{
  int tickets;
  struct proc *curproc = myproc();

  if (argint(0, &tickets) < 0 || tickets <= 0)
    return -1;

  acquire(&ptable_lock);
  curproc->tickets = tickets;
  // cprintf("sys_settickets: pid=%d, tickets set to %d\n", curproc->pid, tickets);
  release(&ptable_lock);

  return 0;
}

int sys_getpinfo(void)
{
  struct pinfo *info;
  if (argptr(0, (void *)&info, sizeof(*info) * NPROC) < 0)
  {
    cprintf("sys_getpinfo: argptr failed\n");
    return -1;
  }
  acquire(&ptable_lock);
  for (int i = 0; i < NPROC; i++)
  {
    struct proc *p = &ptable[i];
    // Include all processes that have been scheduled, even if now UNUSED
    if (p->pid > 0) // Process has been used at some point
    {
      info[i].pid = p->pid;
      info[i].tickets = p->tickets;
      info[i].ticks_scheduled = p->ticks_scheduled;
      /*cprintf("sys_getpinfo: slot %d, pid %d, state %d, tickets %d, scheduled %d\n",
              i, p->pid, p->state, p->tickets, p->ticks_scheduled);*/
    }
    else
    {
      info[i].pid = 0;
      info[i].tickets = 0;
      info[i].ticks_scheduled = 0;
    }
  }
  release(&ptable_lock);
  return 0;
}

int sys_yield(void)
{
  yield();
  return 0;
}

int sys_settickets_pid(void)
{
  int pid, tickets;
  struct proc *p;

  if (argint(0, &pid) < 0 || argint(1, &tickets) < 0 || tickets <= 0)
    return -1;

  acquire(&ptable_lock);
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->tickets = tickets;
      // cprintf("sys_settickets_pid: pid=%d, tickets set to %d\n", pid, tickets);
      release(&ptable_lock);
      return 0;
    }
  }
  release(&ptable_lock);
  // cprintf("sys_settickets_pid: pid=%d not found\n", pid);
  return -1;
}