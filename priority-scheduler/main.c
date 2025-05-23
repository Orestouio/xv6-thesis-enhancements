#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void) __attribute__((noreturn));
extern pde_t *kpgdir;
struct spinlock uartlock;
extern char end[];

int main(void)
{
  initlock(&uartlock, "uart");
  kinit1(end, P2V(4 * 1024 * 1024));
  kvmalloc();
  mpinit();

  lapicinit();
  seginit();
  picinit();
  ioapicinit();
  consoleinit();
  uartinit();

  // cprintf("After mpinit: ncpu=%d\n", ncpu);
  // for (int i = 0; i < ncpu; i++)
  //   cprintf("cpus[%d].apicid=%d\n", i, cpus[i].apicid);

  // cprintf("Debug: ncpu=%d, CPUS=%d\n", ncpu, CPUS);
  pinit();
  tvinit();
  // cprintf("Before binit\n");
  binit();
  // cprintf("Before fileinit\n");
  fileinit();
  // cprintf("Before ideinit\n");
  ideinit();
  userinit();
  startothers();
  kinit2(P2V(4 * 1024 * 1024), P2V(PHYSTOP));
  mpmain();
}

static void mpenter(void)
{
  switchkvm();
  seginit();
  lapicinit();
  mpmain();
}

static void mpmain(void)
{
  // cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  //  cprintf("mpmain: ncpu=%d\n", ncpu);
  //  for (int i = 0; i < ncpu; i++)
  //    cprintf("mpmain: cpus[%d].apicid=%d\n", i, cpus[i].apicid);
  //  cprintf("mpmain: before idtinit\n");
  idtinit();
  // cprintf("mpmain: before xchg, mycpu()->apicid=%d\n", mycpu()->apicid);
  xchg(&(mycpu()->started), 1);
  // cprintf("mpmain: before scheduler\n");
  scheduler();
}

pde_t entrypgdir[]; // For entry.S

// Start the non-boot (AP) processors.
static void startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  code = P2V(0x7000);
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for (c = cpus; c < cpus + ncpu; c++)
  {
    if (c == mycpu())
      continue;

    stack = kalloc();
    *(void **)(code - 4) = stack + KSTACKSIZE;
    *(void (**)(void))(code - 8) = mpenter;
    *(int **)(code - 12) = (void *)V2P(entrypgdir);

    lapicstartap(c->apicid, V2P(code));

    while (c->started == 0)
      ;
  }
}

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
    [0] = (0) | PTE_P | PTE_W | PTE_PS, // VA 0 -> PA 0 (4MB)
    [KERNBASE >>
        PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS, // VA KERNBASE -> PA 0 (4MB)
    [(KERNBASE + 0xFEE00000) >>
        PDXSHIFT] = (0xFEE00000) | PTE_P | PTE_W | PTE_PS, // VA KERNBASE+0xFEE00000 -> PA 0xFEE00000 (4MB)
};