#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

struct gatedesc idt[256];
extern uint vectors[];
struct spinlock tickslock;
uint ticks;

// Removed fixed TIME_SLICE definition; we'll compute it dynamically

void tvinit(void)
{
  int i;
  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);
  initlock(&tickslock, "time");
}

void idtinit(void)
{
  lidt(idt, sizeof(idt));
}

void trap(struct trapframe *tf)
{
  if (tf->trapno == T_SYSCALL)
  {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno)
  {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0)
    {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    if (myproc() && myproc()->state == RUNNING)
    {
      myproc()->cpu_time++;
      acquire(&ptable.lock);
      int has_runnable = 0;
      int highest_priority = 11; // Higher than any valid priority
      struct proc *p;
      for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      {
        if (p != myproc() && p->state == RUNNABLE)
        {
          has_runnable = 1;
          if (p->priority < highest_priority)
            highest_priority = p->priority; // Lower numerical value = higher priority
        }
      }
      // Dynamic time slice based on priority
      int time_slice = (myproc()->priority <= 2) ? 5 : 2; // Priority 0-2: 5 ticks, 3-10: 2 ticks
      // Force preemption if time slice is up OR a higher-priority process is runnable
      if (has_runnable && (myproc()->cpu_time % time_slice == 0 || highest_priority < myproc()->priority))
      {
        release(&ptable.lock);
        yield();
      }
      else
      {
        release(&ptable.lock);
      }
    }
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE + 1:
    break;

  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;

  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  default:
    if (myproc() == 0 || (tf->cs & 3) == 0)
    {
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}