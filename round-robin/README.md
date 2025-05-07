# xv6 Thesis Enhancements

This repository contains the implementations and thesis document for enhancing the xv6 operating system as part of Orestis Theodorou's dissertation project. The project includes three main components:

- **Priority Scheduler** (`priority-scheduler/`): A priority-based scheduler with levels 0–10, ensuring critical tasks are executed efficiently.
- **Lottery Scheduler** (`lottery-scheduler/`): A probabilistic scheduler using ticket counts for proportional fairness.
- **Shared Memory and Semaphores** (`shared-memory-semaphores/`): Mechanisms for IPC and synchronization, tested with a producer-consumer application.

The full thesis document is available at [thesis/thesis.pdf](thesis/thesis.pdf), detailing the design, implementation, testing, and results of each component.

## Repository Structure
- `priority-scheduler/`: Implementation of the priority scheduler (also available in the `priority-scheduler` branch).
- `lottery-scheduler/`: Implementation of the lottery scheduler (also available in the `lottery-scheduler` branch).
- `shared-memory-semaphores/`: Implementation of shared memory and semaphores (also available in the `shared-memory-semaphores` branch).
- `thesis/`: Unified LaTeX document (`thesis.tex`) and compiled PDF (`thesis.pdf`).

## Branches
- `main`: Contains all implementations in subdirectories and the thesis document.
- `priority-scheduler`: Original branch for the priority scheduler implementation.
- `lottery-scheduler`: Original branch for the lottery scheduler implementation.
- `shared-memory-semaphores`: Original branch for the shared memory and semaphores implementation.