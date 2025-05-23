// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include "param.h"
#include "types.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "traps.h"
#include "mmu.h"
#include "x86.h"

// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID (0x0020 / 4)    // ID
#define VER (0x0030 / 4)   // Version
#define TPR (0x0080 / 4)   // Task Priority
#define EOI (0x00B0 / 4)   // End of Interrupt
#define SVR (0x00F0 / 4)   // Spurious Interrupt Vector
#define ENABLE 0x00000100  // Unit Enable
#define ESR (0x0280 / 4)   // Error Status
#define ICRLO (0x0300 / 4) // Interrupt Command (low 32 bits)
#define INIT 0x00000500    // INIT/RESET
#define STARTUP 0x00000600 // Startup IPI
#define DELIVS 0x00001000  // Delivery status
#define ASSERT 0x00004000  // Assert interrupt (vs deassert)
#define DEASSERT 0x00000000
#define LEVEL 0x00008000 // Level triggered
#define BCAST 0x00080000 // Send to all APICs, including self
#define BUSY 0x00001000
#define FIXED 0x00000000
#define ICRHI (0x0310 / 4)  // Interrupt Command (high 32 bits)
#define TIMER (0x0320 / 4)  // Local Vector Table 0 (TIMER)
#define X1 0x0000000B       // Divide counts by 1
#define PERIODIC 0x00020000 // Periodic
#define PCINT (0x0340 / 4)  // Performance Counter LVT
#define LINT0 (0x0350 / 4)  // Local Vector Table 1 (LINT0)
#define LINT1 (0x0360 / 4)  // Local Vector Table 2 (LINT1)
#define ERROR (0x0370 / 4)  // Local Vector Table 3 (ERROR)
#define MASKED 0x00010000   // Interrupt masked
#define TICR (0x0380 / 4)   // Timer Initial Count
#define TCCR (0x0390 / 4)   // Timer Current Count
#define TDCR (0x03E0 / 4)   // Timer Divide Configuration

volatile uint *lapic; // Initialized in mp.c

// Write a value to a LAPIC register and ensure the write completes.
static void
lapicw(int index, int value)
{
  lapic[index] = value;
  lapic[ID]; // Read ID register to ensure write completes
}

// Initialize the Local APIC.
void lapicinit(void)
{
  if (!lapic)
    return;

  // Enable local APIC; set spurious interrupt vector.
  lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  // Configure the timer to count down at bus frequency and issue interrupts.
  // TICR sets the initial count; in QEMU, this is roughly 10ms per interrupt.
  lapicw(TDCR, X1);
  lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapicw(TICR, 10000000);

  // Disable logical interrupt lines.
  lapicw(LINT0, MASKED);
  lapicw(LINT1, MASKED);

  // Disable performance counter overflow interrupts on supported machines.
  if (((lapic[VER] >> 16) & 0xFF) >= 4)
    lapicw(PCINT, MASKED);

  // Map error interrupt to IRQ_ERROR.
  lapicw(ERROR, T_IRQ0 + IRQ_ERROR);

  // Clear error status register (requires back-to-back writes).
  lapicw(ESR, 0);
  lapicw(ESR, 0);

  // Acknowledge any outstanding interrupts.
  lapicw(EOI, 0);

  // Send an Init Level De-Assert to synchronize arbitration IDs.
  lapicw(ICRHI, 0);
  lapicw(ICRLO, BCAST | INIT | LEVEL);
  while (lapic[ICRLO] & DELIVS)
    ;

  // Enable interrupts on the APIC (but not on the processor).
  lapicw(TPR, 0);
}

// Get the APIC ID of the current CPU.
int lapicid(void)
{
  if (!lapic)
    return 0;
  return lapic[ID] >> 24;
}

// Acknowledge an interrupt.
void lapiceoi(void)
{
  if (lapic)
    lapicw(EOI, 0);
}

// Spin for a given number of microseconds.
// Currently empty for QEMU; on real hardware, this should be tuned dynamically.
void microdelay(int us)
{
}

// CMOS/RTC registers for reading the system time.
#define CMOS_PORT 0x70
#define CMOS_RETURN 0x71

// Start an additional processor (AP) running entry code at the given address.
// See Appendix B of the MultiProcessor Specification.
void lapicstartap(uchar apicid, uint addr)
{
  int i;
  ushort *wrv;

  // Initialize CMOS shutdown code and warm reset vector for AP startup.
  outb(CMOS_PORT, 0xF);
  outb(CMOS_PORT + 1, 0x0A);
  wrv = (ushort *)P2V((0x40 << 4 | 0x67)); // Warm reset vector
  wrv[0] = 0;
  wrv[1] = addr >> 4;

  // Universal startup algorithm: Send INIT interrupt to reset the AP.
  lapicw(ICRHI, apicid << 24);
  lapicw(ICRLO, INIT | LEVEL | ASSERT);
  microdelay(200);
  lapicw(ICRLO, INIT | LEVEL);
  microdelay(100); // Should be 10ms, but adjusted for QEMU.

  // Send Startup IPI (twice) to start the AP at the specified address.
  for (i = 0; i < 2; i++)
  {
    lapicw(ICRHI, apicid << 24);
    lapicw(ICRLO, STARTUP | (addr >> 12));
    microdelay(200);
  }
}

// CMOS/RTC status and data registers.
#define CMOS_STATA 0x0a
#define CMOS_STATB 0x0b
#define CMOS_UIP (1 << 7) // RTC update in progress

#define SECS 0x00
#define MINS 0x02
#define HOURS 0x04
#define DAY 0x07
#define MONTH 0x08
#define YEAR 0x09

// Read a value from the CMOS register.
static uint
cmos_read(uint reg)
{
  outb(CMOS_PORT, reg);
  microdelay(200);
  return inb(CMOS_RETURN);
}

// Fill an rtcdate structure with CMOS values.
static void
fill_rtcdate(struct rtcdate *r)
{
  r->second = cmos_read(SECS);
  r->minute = cmos_read(MINS);
  r->hour = cmos_read(HOURS);
  r->day = cmos_read(DAY);
  r->month = cmos_read(MONTH);
  r->year = cmos_read(YEAR);
}

// Read the current time from the RTC and convert to a usable format.
void cmostime(struct rtcdate *r)
{
  struct rtcdate t1, t2;
  int sb, bcd;

  sb = cmos_read(CMOS_STATB);
  bcd = (sb & (1 << 2)) == 0;

  // Ensure the RTC doesn't modify the time while reading.
  for (;;)
  {
    fill_rtcdate(&t1);
    if (cmos_read(CMOS_STATA) & CMOS_UIP)
      continue;
    fill_rtcdate(&t2);
    if (memcmp(&t1, &t2, sizeof(t1)) == 0)
      break;
  }

  // Convert BCD to binary if necessary.
  if (bcd)
  {
#define CONV(x) (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))
    CONV(second);
    CONV(minute);
    CONV(hour);
    CONV(day);
    CONV(month);
    CONV(year);
#undef CONV
  }

  *r = t1;
  r->year += 2000;
}