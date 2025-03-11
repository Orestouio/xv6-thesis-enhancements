#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

void timing_cpu_heavy(void);
void timing_switch_overhead(void);
void timing_io_bound(void);
void timing_mixed_load(void);
void timing_process_creation(void);
void timing_short_tasks(void);
void timing_starvation_check(void);

void run_test(void (*test)(), char *name, int runs)
{
    int total = 0;
    printf(1, "%s (%d runs)\n", name, runs);
    for (int i = 0; i < runs; i++)
    {
        int start = uptime();
        test();
        int end = uptime();
        total += (end - start);
    }
    printf(1, "Total: %d ticks, Avg: %d ticks/run\n", total, total / runs);
}

int main(int argc, char *argv[])
{
    printf(1, "Starting scheduling tests with priority...\n");
    run_test(timing_cpu_heavy, "Test 1: CPU-heavy", 5);
    run_test(timing_switch_overhead, "Test 2: Switch overhead", 5);
    run_test(timing_io_bound, "Test 3: I/O-bound", 5);
    run_test(timing_mixed_load, "Test 4: Mixed load", 5);
    run_test(timing_process_creation, "Test 5: Process creation", 5);
    run_test(timing_short_tasks, "Test 6: Short tasks", 5);
    run_test(timing_starvation_check, "Test 7: Starvation check", 5);
    printf(1, "Tests complete.\n");
    exit();
}

void timing_cpu_heavy(void)
{
    int pid, runs = 10;
    printf(1, "Test 1: CPU-heavy tasks (%d procs)\n", runs);
    for (int i = 0; i < runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return;
        }
        if (pid == 0)
        {
            for (volatile int j = 0; j < 20000000; j++)
                ; // 20M iterations
            exit();
        }
    }
    for (int i = 0; i < runs; i++)
    {
        wait();
    }
}

void timing_switch_overhead(void)
{
    int pid, runs = 500;
    printf(1, "Test 2: Context switch overhead (%d switches)\n", runs);
    for (int i = 0; i < runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return;
        }
        if (pid == 0)
        {
            exit();
        }
        else
        {
            wait();
        }
    }
}

void timing_io_bound(void)
{
    int pid, runs = 100, batch_size = 50;
    printf(1, "Test 3: I/O-bound tasks (%d procs)\n", runs);
    for (int b = 0; b < runs / batch_size; b++)
    {
        for (int i = 0; i < batch_size; i++)
        {
            pid = fork();
            if (pid < 0)
            {
                printf(1, "fork failed at %d\n", b * batch_size + i);
                return;
            }
            if (pid == 0)
            {
                sleep(10); // 100ms = 10 ticks
                exit();
            }
        }
        for (int i = 0; i < batch_size; i++)
        {
            wait();
        }
    }
}

void timing_mixed_load(void)
{
    int pid, cpu_runs = 5, io_runs = 5;
    printf(1, "Test 4: Mixed load (%d CPU, %d I/O)\n", cpu_runs, io_runs);
    // Fork I/O tasks (low priority)
    for (int i = 0; i < io_runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return;
        }
        if (pid == 0)
        {
            setpriority(getpid(), 10); // Lowest priority
            sleep(50);                 // 500ms = 50 ticks
            exit();
        }
    }
    // Fork CPU tasks (high priority)
    for (int i = 0; i < cpu_runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return;
        }
        if (pid == 0)
        {
            setpriority(getpid(), 0); // Highest priority
            for (volatile int j = 0; j < 50000000; j++)
                ; // 50M iterations
            exit();
        }
    }
    for (int i = 0; i < cpu_runs + io_runs; i++)
    {
        wait();
    }
}

void timing_process_creation(void)
{
    int pid, runs = 50;
    printf(1, "Test 5: Process creation (%d forks + exec)\n", runs);
    for (int i = 0; i < runs; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return;
        }
        if (pid == 0)
        {
            char *argv[] = {"echo", "hi", 0};
            exec("echo", argv);
            printf(1, "exec failed at %d\n", i);
            exit();
        }
        else
        {
            wait();
        }
    }
}

void timing_short_tasks(void)
{
    int pid, runs = 200, batch_size = 50;
    printf(1, "Test 6: Short tasks (%d quick procs)\n", runs);
    for (int b = 0; b < runs / batch_size; b++)
    {
        for (int i = 0; i < batch_size; i++)
        {
            pid = fork();
            if (pid < 0)
            {
                printf(1, "fork failed at %d\n", b * batch_size + i);
                return;
            }
            if (pid == 0)
            {
                for (volatile int j = 0; j < 10000; j++)
                    ; // 10K iterations
                exit();
            }
        }
        for (int i = 0; i < batch_size; i++)
        {
            wait();
        }
    }
}

void timing_starvation_check(void)
{
    int pid;
    printf(1, "Test 7: Starvation check (1 light vs 5 heavy)\n");
    // Light process (high priority)
    pid = fork();
    if (pid < 0)
    {
        printf(1, "fork failed\n");
        return;
    }
    if (pid == 0)
    {
        setpriority(getpid(), 0); // Highest priority
        for (volatile int j = 0; j < 50000; j++)
            ; // 50K iterations
        exit();
    }
    // Heavy processes (low priority)
    for (int i = 0; i < 5; i++)
    {
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed at %d\n", i);
            return;
        }
        if (pid == 0)
        {
            setpriority(getpid(), 10); // Lowest priority
            for (volatile int j = 0; j < 20000000; j++)
                ; // 20M iterations
            exit();
        }
    }
    for (int i = 0; i < 6; i++)
    {
        wait();
    }
}