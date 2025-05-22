#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

void acquire(struct spinlock *lk)
{
  pushcli();
  if (holding(lk))
    panic("acquire");
  while (xchg(&lk->locked, 1) != 0)
    ;
  __sync_synchronize();
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

void release(struct spinlock *lk)
{
  if (!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;
  __sync_synchronize();
  asm volatile("movl $0, %0" : "+m"(lk->locked) :);
  popcli();
}

void getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint *)v - 2;
  for (i = 0; i < 10; i++)
  {
    if (ebp == 0 || ebp < (uint *)KERNBASE || ebp == (uint *)0xffffffff)
      break;
    pcs[i] = ebp[1];
    ebp = (uint *)ebp[0];
  }
  for (; i < 10; i++)
    pcs[i] = 0;
}

int holding(struct spinlock *lock)
{
  int r;
  pushcli();
  r = lock->locked && lock->cpu == mycpu();
  popcli();
  return r;
}

void pushcli(void)
{
  int eflags;

  eflags = readeflags();
  cli();
  if (mycpu()->ncli == 0)
    mycpu()->intena = eflags & FL_IF;
  mycpu()->ncli += 1;
}

void popcli(void)
{
  if (readeflags() & FL_IF)
    panic("popcli - interruptible");
  if (--mycpu()->ncli < 0)
    panic("popcli");
  if (mycpu()->ncli == 0 && mycpu()->intena)
    sti();
}