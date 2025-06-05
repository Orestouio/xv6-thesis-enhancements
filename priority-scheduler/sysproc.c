#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

extern int context_switches;       // Defined in proc.c
extern void print_sched_log(void); // Defined in proc.c
extern struct proc ptable[NPROC];

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

int sys_yield(void)
{
  yield();
  return 0;
}

int sys_setpriority(void)
{
  int pid, priority;
  if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
    return -1;
  if (priority < 0 || priority > 10)
    return -1;
  struct proc *p;
  acquire(&ptable_lock);
  for (p = ptable; p < &ptable[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      if (p->state == RUNNABLE)
        rq_remove(&cpus[p->cpu].rq, p);
      p->priority = priority;
      if (p->state == RUNNABLE)
        rq_add(&cpus[p->cpu].rq, p);
      release(&ptable_lock);
      return 0;
    }
  }
  release(&ptable_lock);
  return -1;
}

int sys_getcontextswitches(void)
{
  return context_switches;
}

int sys_print_sched_log(void)
{
  print_sched_log();
  return 0;
}