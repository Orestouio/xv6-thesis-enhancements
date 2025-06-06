# xv6 Round-Robin Scheduler

This directory contains the default round-robin scheduler implementation for the xv6 operating system. The scheduler allocates equal time slices to all runnable processes in a cyclic order.

## Build and Run
- `make clean`: Remove compiled files.
- `make`: Compile the xv6 kernel and user programs.
- `make qemu CPUS=n`: Run xv6 in QEMU with `n` CPUs (e.g., `make qemu CPUS=2`).

## Testing
The `timingtests.c` suite evaluates performance across various workloads, measuring turnaround time and context switch overhead.