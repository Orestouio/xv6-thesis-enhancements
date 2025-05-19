#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int timing_cpu_heavy(void);
int timing_switch_overhead(void);
int timing_io_bound(void);
int timing_mixed_load(void);
int timing_process_creation(void);
int timing_short_tasks(void);
int timing_starvation_check(void);

void run_test(int (*test)(), char *name, int runs)
{
    int total = 0;
    printf(1, "%s (%d runs)\n", name, runs);
    for (int i = 0; i < runs; i++)
    {
        int ticks = test();
        total += ticks;
        printf(1, "Run %d: %d ticks\n", i + 1, ticks);
    }
    printf(1, "Total: %d ticks, Avg: %d ticks/run\n", total, total / runs);
}

int main(int argc, char *argv[])
{
    printf(1, "Starting round-robin scheduling tests...\n");
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

int timing_cpu_heavy(void)
{
    int pid, runs = 10;
    printf(1, "Test 1: CPU-heavy tasks (%d procs)\n", runs);
    int start_switches = getcontextswitches();
    int start = uptime();
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
            for (volatile int j = 0; j < 20000000; j++)
                ;
            exit();
        }
    }
    for (int i = 0; i < runs; i++)
    {
        wait();
    }
    int end = uptime();
    int end_switches = getcontextswitches();
    printf(1, "Context switches during test: %d\n", end_switches - start_switches);
    return end - start;
}

int timing_switch_overhead(void)
{
    int pid, runs = 200;
    printf(1, "Test 2: Context switch overhead (%d switches)\n", runs);
    int start = uptime();
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
            exit(); // Match priority scheduler: immediate exit
        }
        else
        {
            wait();
        }
    }
    int end = uptime();
    return end - start;
}

int timing_io_bound(void)
{
    int pid, runs = 100, batch_size = 50;
    printf(1, "Test 3: I/O-bound tasks (%d procs)\n", runs);
    int start = uptime();
    for (int b = 0; b < runs / batch_size; b++)
    {
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
                sleep(10); // Match priority scheduler: sleep 10 ticks
                exit();
            }
        }
        for (int i = 0; i < batch_size; i++)
        {
            wait();
        }
    }
    int end = uptime();
    return end - start;
}

int timing_mixed_load(void)
{
    int pid, cpu_runs = 5, io_runs = 5;
    int min_ticks = 9999;
    int has_50 = 0;
    int pipefd[2];

    if (pipe(pipefd) < 0)
    {
        printf(1, "pipe failed\n");
        return -1;
    }

    printf(1, "Test 4: Mixed load (%d CPU, %d I/O)\n", cpu_runs, io_runs);

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
            close(pipefd[0]);
            int start = uptime();
            sleep(50); // Match priority scheduler: sleep 50 ticks
            int end = uptime();
            int ticks = end - start;
            write(pipefd[1], &ticks, sizeof(ticks));
            close(pipefd[1]);
            exit();
        }
    }

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
            close(pipefd[0]);
            int start = uptime();
            for (volatile int j = 0; j < 50000000; j++)
                ; // Match priority scheduler: 50M iterations
            int end = uptime();
            int ticks = end - start;
            write(pipefd[1], &ticks, sizeof(ticks));
            close(pipefd[1]);
            exit();
        }
    }

    close(pipefd[1]);
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
    close(pipefd[0]);
    return has_50 ? 50 : (min_ticks == 9999 ? 50 : min_ticks);
}

int timing_process_creation(void)
{
    int pid, runs = 50;
    printf(1, "Test 5: Process creation (%d forks)\n", runs);
    int start = uptime();
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
            exit(); // Match priority scheduler: immediate exit
        }
    }
    for (int i = 0; i < runs; i++)
    {
        if (wait() == -1)
        {
            printf(1, "wait failed at %d\n", i);
            break;
        }
    }
    int end = uptime();
    return end - start;
}

int timing_short_tasks(void)
{
    int pid, runs = 200, batch_size = 50;
    printf(1, "Test 6: Short tasks (%d quick procs)\n", runs);
    int start = uptime();
    for (int b = 0; b < runs / batch_size; b++)
    {
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
                for (volatile int j = 0; j < 10000; j++)
                    ; // Match priority scheduler: 10K iterations
                exit();
            }
        }
        for (int i = 0; i < batch_size; i++)
        {
            wait();
        }
    }
    int end = uptime();
    return end - start;
}

int timing_starvation_check(void)
{
    int pid;
    printf(1, "Test 7: Starvation check (1 light vs 5 heavy)\n");
    int start = uptime();
    pid = fork();
    if (pid < 0)
    {
        printf(1, "fork failed\n");
        return -1;
    }
    if (pid == 0)
    {
        for (volatile int j = 0; j < 50000; j++)
            ; // Match priority scheduler: 50K iterations
        exit();
    }
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
            for (volatile int j = 0; j < 20000000; j++)
                ; // Match priority scheduler: 20M iterations
            exit();
        }
    }
    for (int i = 0; i < 6; i++)
    {
        wait();
    }
    int end = uptime();
    return end - start;
}