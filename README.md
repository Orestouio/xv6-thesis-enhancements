# xv6 Scheduling Implementations

This project contains three implementations of process schedulers for the xv6 operating system: round-robin, priority, and lottery scheduling. Each scheduler is implemented in a separate subdirectory.

## Directory Structure
- `round-robin/`: Default xv6 round-robin scheduler.
- `priority/`: Priority-based scheduler with deterministic process prioritization.
- `lottery/`: Lottery scheduler with probabilistic CPU allocation based on tickets.

## Build Instructions
Each subdirectory supports the following commands:
- `make clean`: Remove compiled files.
- `make`: Compile the xv6 kernel and user programs.
- `make qemu CPUS=n`: Run xv6 in QEMU with `n` CPUs (e.g., `make qemu CPUS=2`).

Navigate to the desired subdirectory and follow its README for specific details.