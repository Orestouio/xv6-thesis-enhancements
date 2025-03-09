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
    printf(1, "Starting scheduling baseline tests...\n");
    run_test(timing_cpu_heavy, "Test 1: CPU-heavy", 5);
    run_test(timing_switch_overhead, "Test 2: Switch overhead", 5);
    run_test(timing_io_bound, "Test 3: I/O-bound", 5);
    run_test(timing_mixed_load, "Test 4: Mixed load", 5);
    run_test(timing_process_creation, "Test 5: Process creation", 5);
    run_test(timing_short_tasks, "Test 6: Short tasks", 5);
    run_test(timing_starvation_check, "Test 7: Starvation check", 5);
    printf(1, "Baseline tests complete.\n");
    exit();
}

void timing_cpu_heavy(void)
{
    int start, end, pid, total = 0;
    int runs = 10;

    printf(1, "Test 1: CPU-heavy tasks (%d procs)\n", runs);
    start = uptime();
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
                ; // CPU burn
            exit();
        }
    }
    for (int i = 0; i < runs; i++)
    {
        wait();
    }
    end = uptime();
    total = end - start;
    printf(1, "%d CPU-heavy procs took %d ticks\n", runs, total);
}

void timing_switch_overhead(void)
{
    int start, end, pid, total = 0;
    int runs = 500;

    printf(1, "Test 2: Context switch overhead (%d switches)\n", runs);
    start = uptime();
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
    end = uptime();
    total = end - start;
    printf(1, "%d switches took %d ticks (avg: %d ticks/switch)\n", runs, total, total / runs);
}

void timing_io_bound(void)
{
    int start, end, pid, total = 0;
    int runs = 100;

    printf(1, "Test 3: I/O-bound tasks (%d cycles)\n", runs);
    start = uptime();
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
            sleep(10); // 100ms sleep
            exit();
        }
        else
        {
            wait();
        }
    }
    end = uptime();
    total = end - start;
    printf(1, "%d I/O cycles took %d ticks (avg: %d ticks/cycle)\n", runs, total, total / runs);
}

void timing_mixed_load(void)
{
    int start, end, pid, total = 0;
    int cpu_runs = 5, io_runs = 5;

    printf(1, "Test 4: Mixed load (%d CPU, %d I/O)\n", cpu_runs, io_runs);
    start = uptime();
    // Fork I/O tasks only
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
            sleep(50); // 500ms sleep
            exit();
        }
    }
    // Wait for I/O
    for (int i = 0; i < io_runs; i++)
    {
        wait();
    }
    printf(1, "I/O done at %d ticks\n", uptime() - start); // Debug
    // Fork CPU tasks
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
            for (volatile int j = 0; j < 50000000; j++)
                ; // 50M iterations
            exit();
        }
    }
    for (int i = 0; i < cpu_runs; i++)
    {
        wait();
    }
    end = uptime();
    total = end - start;
    printf(1, "Mixed load took %d ticks\n", total);
}

void timing_process_creation(void)
{
    int start, end, pid, total = 0;
    int runs = 50;

    printf(1, "Test 5: Process creation (%d forks + exec)\n", runs);
    start = uptime();
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
            char *argv[] = {"echo", "hi", 0}; // Minimal args
            exec("echo", argv);
            printf(1, "exec failed at %d\n", i); // Debug failure
            exit();
        }
        else
        {
            wait();
        }
    }
    end = uptime();
    total = end - start;
    printf(1, "%d fork+exec took %d ticks (avg: %d ticks/run)\n", runs, total, total / runs);
}

void timing_short_tasks(void)
{
    int start, end, pid, total = 0;
    int runs = 200;

    printf(1, "Test 6: Short tasks (%d quick procs)\n", runs);
    start = uptime();
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
            for (volatile int j = 0; j < 10000; j++)
                ; // Light CPU
            exit();
        }
        else
        {
            wait();
        }
    }
    end = uptime();
    total = end - start;
    printf(1, "%d short tasks took %d ticks (avg: %d ticks/task)\n", runs, total, total / runs);
}

void timing_starvation_check(void)
{
    int start, end, pid, total = 0;

    printf(1, "Test 7: Starvation check (1 light vs 5 heavy)\n");
    start = uptime();
    // Light process
    pid = fork();
    if (pid < 0)
    {
        printf(1, "fork failed\n");
        return;
    }
    if (pid == 0)
    {
        for (volatile int j = 0; j < 50000; j++)
            ; // Quick task
        exit();
    }
    // Heavy processes
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
            for (volatile int j = 0; j < 20000000; j++)
                ; // Heavy CPU
            exit();
        }
    }
    for (int i = 0; i < 6; i++)
    {
        wait();
    }
    end = uptime();
    total = end - start;
    printf(1, "1 light + 5 heavy took %d ticks\n", total);
}