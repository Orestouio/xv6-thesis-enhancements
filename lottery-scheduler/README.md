# xv6 Priority Scheduler

This directory contains a priority-based scheduler implementation for the xv6 operating system. Processes are assigned priorities (0-10, 0 highest), and the scheduler executes the highest-priority process first.

## Build and Run
- `make clean`: Remove compiled files.
- `make`: Compile the xv6 kernel and user programs.
- `make qemu CPUS=n`: Run xv6 in QEMU with `n` CPUs (e.g., `make qemu CPUS=2`).

## Testing
The `prioritytest.c` suite evaluates performance and prioritization across various workloads, measuring turnaround time and context switch overhead.