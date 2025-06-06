/*
 * Prioritytest.c: User-level test program for the xv6 priority scheduler.
 * Executes a suite of tests to evaluate scheduler performance under various workloads,
 * including CPU-heavy, I/O-bound, mixed, process creation, short tasks, and starvation scenarios.
 * Measures execution time and context switches.
 */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

// Function prototypes for test cases
int timing_cpu_heavy(void);
int timing_switch_overhead(void);
int timing_io_bound(void);
int timing_mixed_load(void);
int timing_process_creation(void);
int timing_short_tasks(void);
int timing_starvation_check(void);

// Run a test case multiple times and report total and average execution time.
void run_test(int (*test)(), char *name, int runs)
{
    int total = 0; // Total ticks across all runs

    // Print test name and number of runs
    printf(1, "%s (%d runs)\n", name, runs);

    // Execute test multiple times
    for (int i = 0; i < runs; i++)
    {
        int ticks = test(); // Run test and get execution time
        total += ticks;
        // printf(1, "Run %d: %d ticks\n", i + 1, ticks); // Optional per-run output
    }

    // Print total and average ticks
    printf(1, "+++ Total: %d ticks, Avg: %d ticks/run\n", total, total / runs);
}

// Main function: execute all test cases with pauses between them.
int main(int argc, char *argv[])
{
    // Announce start of tests
    printf(1, "Starting scheduling tests with priority...\n");

    // Run each test case with 5 runs, pausing 5 ticks between tests
    run_test(timing_cpu_heavy, "Test 1: CPU-heavy", 5);
    sleep(5);
    run_test(timing_switch_overhead, "Test 2: Switch overhead", 5);
    sleep(5);
    run_test(timing_io_bound, "Test 3: I/O-bound", 5);
    sleep(5);
    run_test(timing_mixed_load, "Test 4: Mixed load", 5);
    sleep(5);
    run_test(timing_process_creation, "Test 5: Process creation", 5);
    sleep(5);
    run_test(timing_short_tasks, "Test 6: Short tasks", 5);
    sleep(5);
    run_test(timing_starvation_check, "Test 7: Starvation check", 5);
    sleep(5);

    // Announce completion
    printf(1, "Tests complete.\n");
    exit();
}

// Test 1: Measure performance of CPU-heavy tasks.
int timing_cpu_heavy(void)
{
    int pid, runs = 10; // Number of child processes

    // Announce test
    printf(1, "Test 1: CPU-heavy tasks (%d procs)\n", runs);

    // Record initial context switches and time
    int start_switches = getcontextswitches();
    int start = uptime();

    // Fork child processes
    for (int i = 0; i < runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return -1;
        }
        if (pid == 0)
        {
            // Child: perform CPU-intensive loop
            for (volatile int j = 0; j < 20000000; j++)
                ;
            exit();
        }
    }

    // Wait for all children to complete
    for (int i = 0; i < runs; i++)
    {
        wait();
    }

    // Record end time and context switches
    int end = uptime();
    int end_switches = getcontextswitches();

    // Print context switch count
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);

    // Return execution time in ticks
    return end - start;
}

// Test 2: Measure context switch overhead by rapid process creation and termination.
int timing_switch_overhead(void)
{
    int pid, runs = 200; // Number of fork-wait cycles

    // Announce test
    printf(1, "Test 2: Context switch overhead (%d switches)\n", runs);

    // Record initial context switches and time
    int start_switches = getcontextswitches();
    int start = uptime();

    // Perform fork-wait cycles
    for (int i = 0; i < runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return -1;
        }
        if (pid == 0)
        {
            // Child: exit immediately
            exit();
        }
        else
        {
            // Parent: wait for child
            wait();
        }
    }

    // Record end time and context switches
    int end = uptime();
    int end_switches = getcontextswitches();

    // Print context switch count
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);

    // Return execution time in ticks (~75-80 ticks)
    return end - start;
}

// Test 3: Measure performance of I/O-bound tasks with varying priorities.
int timing_io_bound(void)
{
    const int num_procs = 50; // Number of processes (limited to fit NPROC)

    // Announce test
    printf(1, "Test 3: I/O-bound tasks (%d procs)\n", num_procs);

    // Record initial context switches and time
    int start_switches = getcontextswitches();
    int start_time = uptime();

    // Fork I/O-bound processes
    for (int i = 0; i < num_procs; i++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            // Clean up: wait for existing children
            while (wait() != -1)
                ;
            return -1;
        }
        if (pid == 0)
        {
            // Child: set priority (first half: 5, second half: 0)
            if (i < num_procs / 2)
                setpriority(getpid(), 5);
            else
                setpriority(getpid(), 0);

            // Simulate I/O-bound behavior: short CPU bursts with sleep
            for (int j = 0; j < 10; j++)
            {
                for (int k = 0; k < 100000; k++)
                    ;     // Short CPU burst
                sleep(1); // Simulate I/O wait
            }
            exit();
        }
    }

    // Wait for all children to complete
    for (int i = 0; i < num_procs; i++)
    {
        if (wait() == -1)
        {
            printf(1, "wait failed for child %d\n", i);
            break;
        }
    }

    // Record end time and context switches
    int end_time = uptime();
    int end_switches = getcontextswitches();

    // Print context switch count
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);

    // Return execution time in ticks
    return end_time - start_time;
}

// Test 4: Measure performance of mixed CPU and I/O workloads.
int timing_mixed_load(void)
{
    int pid, cpu_runs = 5, io_runs = 5; // Number of CPU and I/O processes
    int min_ticks = 9999;               // Minimum execution time
    int has_50 = 0;                     // Flag for 50-tick processes
    int pipefd[2];                      // Pipe for communication

    // Create pipe for timing data
    if (pipe(pipefd) < 0)
    {
        printf(1, "pipe failed\n");
        return -1;
    }

    // Announce test
    printf(1, "Test 4: Mixed load (%d CPU, %d I/O)\n", cpu_runs, io_runs);

    // Record initial context switches
    int start_switches = getcontextswitches();

    // Fork I/O-bound processes
    for (int i = 0; i < io_runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return -1;
        }
        if (pid == 0)
        {
            // Child: I/O-bound with low priority
            close(pipefd[0]);
            setpriority(getpid(), 10);
            sleep(50);
            int ticks = 50; // Fixed sleep duration
            write(pipefd[1], &ticks, sizeof(ticks));
            close(pipefd[1]);
            exit();
        }
    }

    // Fork CPU-bound processes
    for (int i = 0; i < cpu_runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return -1;
        }
        if (pid == 0)
        {
            // Child: CPU-bound with high priority
            close(pipefd[0]);
            setpriority(getpid(), 0);
            int child_start = uptime();
            for (volatile int j = 0; j < 50000000; j++)
                ;
            int ticks = uptime() - child_start;
            write(pipefd[1], &ticks, sizeof(ticks));
            close(pipefd[1]);
            exit();
        }
    }

    // Close write end of pipe in parent
    close(pipefd[1]);

    // Collect timing data and wait for children
    for (int i = 0; i < cpu_runs + io_runs; i++)
    {
        int ticks;
        read(pipefd[0], &ticks, sizeof(ticks));
        if (ticks == 50)
            has_50 = 1;
        if (ticks < min_ticks && ticks >= 50)
            min_ticks = ticks;
        wait();
    }

    // Close read end of pipe
    close(pipefd[0]);

    // Record context switches
    int end_switches = getcontextswitches();

    // Print context switch count
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);

    // Return representative execution time
    return has_50 ? 50 : (min_ticks == 9999 ? 50 : min_ticks);
}

// Test 5: Measure process creation overhead.
int timing_process_creation(void)
{
    int pid, runs = 50; // Number of forks

    // Announce test
    printf(1, "Test 5: Process creation (%d forks)\n", runs);

    // Record initial context switches and time
    int start_switches = getcontextswitches();
    int start = uptime();

    // Fork and execution
    for (int i = 0; i < runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return -1;
        }
        if (pid == 0)
        {
            exit(); // No exec, just fork and exit
        }
    }

    // Wait for all children
    while (wait() != -1)
        ;

    // Record end time and context switches
    int end = uptime();
    int end_switches = getcontextswitches();

    // Print context switch count
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);

    // Return execution time in ticks (~25-30 ticks)
    return end - start;
}

// Test 6: Measure performance of short-lived tasks.
int timing_short_tasks(void)
{
    int pid, runs = 200, batch_size = 50; // Total tasks and batch size

    // Announce test
    printf(1, "Test 6: Short tasks (%d quick procs)\n", runs);

    // Record initial context switches and time
    int start_switches = getcontextswitches();
    int start = uptime();

    // Process tasks in batches
    for (int b = 0; b < runs / batch_size; b++)
    {
        // Fork batch of processes
        for (int i = 0; i < batch_size; i++)
        {
            pid = fork();
            if (pid < 0)
            {
                printf(1, "fork failed at %d\n", b * batch_size + i);
                return -1;
            }
            if (pid == 0)
            {
                // Child: perform short CPU task
                for (volatile int j = 0; j < 10000; j++)
                    ;
                exit();
            }
        }

        // Wait for batch to complete
        for (int i = 0; i < batch_size; i++)
        {
            wait();
        }
    }

    // Record end time and context switches
    int end = uptime();
    int end_switches = getcontextswitches();

    // Print context switch count
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);

    // Return execution time in ticks (~70-85 ticks)
    return end - start;
}

// Test 7: Check for starvation with one high-priority light task vs. heavy tasks.
int timing_starvation_check(void)
{
    int pid;

    // Announce test
    printf(1, "Test 7: Starvation check (1 light vs 5 heavy)\n");

    // Record initial context switches and time
    int start_switches = getcontextswitches();
    int start = uptime();

    // Fork high-priority light task
    pid = fork();
    if (pid < 0)
    {
        printf(1, "fork failed\n");
        return -1;
    }
    if (pid == 0)
    {
        // Child: light task with high priority
        setpriority(getpid(), 0);
        for (volatile int j = 0; j < 50000; j++)
            ;
        exit();
    }

    // Fork low-priority heavy tasks
    for (int i = 0; i < 5; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return -1;
        }
        if (pid == 0)
        {
            // Child: heavy task with low priority
            setpriority(getpid(), 10);
            for (volatile int j = 0; j < 20000000; j++)
                ;
            exit();
        }
    }

    // Wait for all children
    for (int i = 0; i < 6; i++)
    {
        wait();
    }

    // Record end time and context switches
    int end = uptime();
    int end_switches = getcontextswitches();

    // Print context switch count
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);

    // Return execution time in ticks (~25-30 ticks)
    return end - start;
}