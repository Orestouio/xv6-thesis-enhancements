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
    if (!mycpu()->started)
    {
      lapiceoi();
      break;
    }
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
    if (myproc() && myproc()->state == RUNNING)
    {
      myproc()->cpu_time++;
      // Only call yield() if ptable.lock is not held
      if (!holding(&ptable.lock))
      {
        int has_runnable = 0;
        int highest_priority = 11;
        struct proc *p;
        int i;
        acquire(&ptable.lock);
        for (i = 0; i < NPROC; i++)
        {
          p = ptable.proc[i];
          if (p && p != myproc() && p->state == RUNNABLE)
          {
            has_runnable = 1;
            if (p->priority < highest_priority)
              highest_priority = p->priority;
          }
        }
        int time_slice = (myproc()->priority <= 2) ? 5 : 2;
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
    }
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE:
    if (!mycpu()->started)
    {
      lapiceoi();
      break;
    }
    ideintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE + 1:
    break;

  case T_IRQ0 + IRQ_KBD:
    if (!mycpu()->started)
    {
      lapiceoi();
      break;
    }
    kbdintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_COM1:
    if (!mycpu()->started)
    {
      lapiceoi();
      break;
    }
    uartintr();
    lapiceoi();
    break;

  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    if (!mycpu()->started)
    {
      lapiceoi();
      break;
    }
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  default:
    if (!mycpu()->started)
    {
      cprintf("Unexpected trap %d on uninitialized CPU\n", tf->trapno);
      break;
    }
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