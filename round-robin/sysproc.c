#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_fork(void) { return fork(); }

int sys_exit(void)
{
    exit();
    return 0;
}

int sys_wait(void) { return wait(); }

int sys_kill(void)
{
    int pid;
    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

int sys_getpid(void) { return myproc()->pid; }

int sys_sbrk(void)
{
    int addr, n;
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

int sys_getpinfo(void)
{
    int pid;
    struct proc_stat *ps;
    if (argint(0, &pid) < 0 || argptr(1, (void *)&ps, sizeof(*ps)) < 0)
    {
        cprintf("sys_getpinfo: invalid arguments, pid=%d\n", pid);
        return -1;
    }
    struct proc *p;
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->pid == pid && (p->state != UNUSED || p->end_ticks > 0))
        {
            ps->pid = p->pid;
            ps->turnaround = p->end_ticks > 0 ? p->end_ticks - p->start_ticks : 0;
            ps->response = p->first_run_ticks > 0 ? p->first_run_ticks - p->start_ticks : 0;
            ps->waiting = p->wait_ticks;
            ps->cpu = p->run_ticks;
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    cprintf("sys_getpinfo: pid %d not found\n", pid);
    return -1;
}

int sys_getticks(void)
{
    return ticks;
}

int sys_yield(void)
{
    yield();
    return 0;
}

int sys_getcontextswitches(void)
{
    return context_switches;
}