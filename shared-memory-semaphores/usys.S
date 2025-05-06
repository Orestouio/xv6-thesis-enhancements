#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
<<<<<<< HEAD:lottery-scheduler/usys.S
<<<<<<< HEAD:priority-scheduler/usys.S
SYSCALL(setpriority)
SYSCALL(getcontextswitches)
SYSCALL(print_sched_log)
=======
SYSCALL(settickets)
SYSCALL(getpinfo)
SYSCALL(yield)
SYSCALL(settickets_pid)
>>>>>>> lottery-scheduler:lottery-scheduler/usys.S
=======
SYSCALL(shm_open)
SYSCALL(shm_close)
SYSCALL(sem_init)
SYSCALL(sem_wait)
SYSCALL(sem_post)
>>>>>>> shared-memory-semaphores:shared-memory-semaphores/usys.S
