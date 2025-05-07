# xv6 Thesis Enhancements

This repository contains the implementations and thesis document for enhancing the xv6 operating system as part of Orestis Theodorou's dissertation project. The project includes four main components:

- **Priority Scheduler** (`priority-scheduler/`): A priority-based scheduler with levels 0–10, ensuring critical tasks are executed efficiently.
- **Lottery Scheduler** (`lottery-scheduler/`): A probabilistic scheduler using ticket counts for proportional fairness.
- **Shared Memory and Semaphores** (`shared-memory-semaphores/`): Mechanisms for IPC and synchronization, tested with a producer-consumer application.
- **Round Robin Scheduler** (`round-robin/`): A modified round-robin scheduler, enhancing the default xv6 scheduling mechanism.

The full thesis document is available at [thesis/thesis.pdf](thesis/thesis.pdf), detailing the design, implementation, testing, and results of each component.

## Repository Structure
- `priority-scheduler/`: Implementation of the priority scheduler. Originally developed in [Xv-6-Project](https://github.com/Orestouio/Xv-6-Project).
- `lottery-scheduler/`: Implementation of the lottery scheduler. Originally developed in [Xv6_LotteryExtension](https://github.com/Orestouio/Xv6_LotteryExtension).
- `shared-memory-semaphores/`: Implementation of shared memory and semaphores. Originally developed in [xv6-shared-memory](https://github.com/Orestouio/xv6-shared-memory).
- `round-robin/`: Implementation of the modified round-robin scheduler. Originally developed in the `round-robin-modified` branch of [Xv-6-Project](https://github.com/Orestouio/Xv-6-Project/tree/round-robin-modified).
- `thesis/`: Unified LaTeX document (`thesis.tex`) and compiled PDF (`thesis.pdf`).
EOF

git add README.md
git commit -m "Update README to include round-robin implementation"

If you only have thesis.tex and not thesis.pdf, adjust the README:
bash
cat > README.md << 'EOF'
# xv6 Thesis Enhancements

This repository contains the implementations and thesis document for enhancing the xv6 operating system as part of Orestis Theodorou's dissertation project. The project includes four main components:

- **Priority Scheduler** (`priority-scheduler/`): A priority-based scheduler with levels 0–10, ensuring critical tasks are executed efficiently.
- **Lottery Scheduler** (`lottery-scheduler/`): A probabilistic scheduler using ticket counts for proportional fairness.
- **Shared Memory and Semaphores** (`shared-memory-semaphores/`): Mechanisms for IPC and synchronization, tested with a producer-consumer application.
- **Round Robin Scheduler** (`round-robin/`): A modified round-robin scheduler, enhancing the default xv6 scheduling mechanism.

The thesis document is available as a LaTeX source file at [thesis/thesis.tex](thesis/thesis.tex). To view the document, compile `thesis.tex` using a LaTeX compiler (e.g., pdflatex) to generate `thesis.pdf`.

## Repository Structure
- `priority-scheduler/`: Implementation of the priority scheduler. Originally developed in [Xv-6-Project](https://github.com/Orestouio/Xv-6-Project).
- `lottery-scheduler/`: Implementation of the lottery scheduler. Originally developed in [Xv6_LotteryExtension](https://github.com/Orestouio/Xv6_LotteryExtension).
- `shared-memory-semaphores/`: Implementation of shared memory and semaphores. Originally developed in [xv6-shared-memory](https://github.com/Orestouio/xv6-shared-memory).
- `round-robin/`: Implementation of the modified round-robin scheduler. Originally developed in the `round-robin-modified` branch of [Xv-6-Project](https://github.com/Orestouio/Xv-6-Project/tree/round-robin-modified).
- `thesis/`: Unified LaTeX document (`thesis.tex`).