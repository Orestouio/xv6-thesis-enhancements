#include "types.h"
#include "stat.h"
#include "user.h"

// Generic test function for CPU-heavy, mixed, short tasks, and starvation check
void run_workload_test(int tickets1, int tickets2, int tickets3, int iterations1, int iterations2, int iterations3,
                       int sleep1, int sleep2, int sleep3, const char *test_name,
                       int *sched1_out, int *sched2_out, int *sched3_out)
{
    int pid1, pid2, pid3;
    int i;
    volatile int counter = 0;
    struct pinfo info[64];
    int sched1 = 0, sched2 = 0, sched3 = 0;
    int start_time, end_time;

    printf(1, "%s: Tickets=%d,%d,%d\n", test_name, tickets1, tickets2, tickets3);

    start_time = uptime();

    pid1 = fork();
    if (pid1 == 0)
    {
        settickets(tickets1);
        if (sleep1 > 0)
            sleep(sleep1);
        for (i = 0; i < iterations1; i++)
        {
            counter++;
            if (i % 5000 == 0) // Yield every 5K iterations
                yield();
        }
        printf(1, "Process A (pid=%d, %d tickets) done\n", getpid(), tickets1);
        exit();
    }

    pid2 = fork();
    if (pid2 == 0)
    {
        settickets(tickets2);
        if (sleep2 > 0)
            sleep(sleep2);
        for (i = 0; i < iterations2; i++)
        {
            counter++;
            if (i % 5000 == 0)
                yield();
        }
        printf(1, "Process B (pid=%d, %d tickets) done\n", getpid(), tickets2);
        exit();
    }

    pid3 = fork();
    if (pid3 == 0)
    {
        settickets(tickets3);
        if (sleep3 > 0)
            sleep(sleep3);
        for (i = 0; i < iterations3; i++)
        {
            counter++;
            if (i % 5000 == 0)
                yield();
        }
        printf(1, "Process C (pid=%d, %d tickets) done\n", getpid(), tickets3);
        exit();
    }

    sleep(5);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        printf(1, "Before wait, looking for PIDs: %d, %d, %d\n", pid1, pid2, pid3);
        for (i = 0; i < 64; i++)
        {
            if (info[i].pid > 0)
                printf(1, "info[%d]: pid=%d, tickets=%d, scheduled=%d\n",
                       i, info[i].pid, info[i].tickets, info[i].ticks_scheduled);
            if (info[i].pid == pid1)
                sched1 = info[i].ticks_scheduled;
            if (info[i].pid == pid2)
                sched2 = info[i].ticks_scheduled;
            if (info[i].pid == pid3)
                sched3 = info[i].ticks_scheduled;
        }
    }

    wait();
    wait();
    wait();

    end_time = uptime();
    printf(1, "Test runtime: %d ticks\n", end_time - start_time);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
        return;
    }

    int total_sched = sched1 + sched2 + sched3;
    if (total_sched > 0)
    {
        printf(1, "Results:\n");
        printf(1, "  Process A: %d tickets, %d schedules (%d%%)\n",
               tickets1, sched1, (sched1 * 100) / total_sched);
        printf(1, "  Process B: %d tickets, %d schedules (%d%%)\n",
               tickets2, sched2, (sched2 * 100) / total_sched);
        printf(1, "  Process C: %d tickets, %d schedules (%d%%)\n",
               tickets3, sched3, (sched3 * 100) / total_sched);
        printf(1, "  Expected: A=%d%%, B=%d%%, C=%d%%\n",
               (tickets1 * 100) / (tickets1 + tickets2 + tickets3),
               (tickets2 * 100) / (tickets1 + tickets2 + tickets3),
               (tickets3 * 100) / (tickets1 + tickets2 + tickets3));
    }
    else
    {
        printf(1, "No scheduling data collected (sched1=%d, sched2=%d, sched3=%d)\n",
               sched1, sched2, sched3);
    }

    *sched1_out = sched1;
    *sched2_out = sched2;
    *sched3_out = sched3;
}

void run_switch_test(int tickets1, int tickets2, int tickets3, int *sched1_out, int *sched2_out, int *sched3_out)
{
    int pids[50]; // Array to store PIDs of all 50 processes
    int sched1 = 0, sched2 = 0, sched3 = 0;
    struct pinfo info[64];
    int start_time, end_time;
    int i;

    printf(1, "%s: Tickets=%d,%d,%d\n", "Test 2: Switch Overhead", tickets1, tickets2, tickets3);

    start_time = uptime();

    // First 16 sets: C, A, B
    for (i = 0; i < 16; i++)
    {
        // Fork C
        pids[34 + i] = fork();
        if (pids[34 + i] == 0)
        {
            settickets(tickets3);
            for (volatile int j = 0; j < 100000000; j++)
            {
                if (j % 100000 == 0)
                    yield();
            }
            exit();
        }
        // Fork A
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(tickets1);
            for (volatile int j = 0; j < 100000000; j++)
            {
                if (j % 100000 == 0)
                    yield();
            }
            exit();
        }
        // Fork B
        pids[17 + i] = fork();
        if (pids[17 + i] == 0)
        {
            settickets(tickets2);
            for (volatile int j = 0; j < 100000000; j++)
            {
                if (j % 100000 == 0)
                    yield();
            }
            exit();
        }
    }
    // Fork the remaining process for A (17th process)
    pids[16] = fork();
    if (pids[16] == 0)
    {
        settickets(tickets1);
        for (volatile int j = 0; j < 100000000; j++)
        {
            if (j % 100000 == 0)
                yield();
        }
        exit();
    }
    // Fork the remaining process for B (17th process)
    pids[33] = fork();
    if (pids[33] == 0)
    {
        settickets(tickets2);
        for (volatile int j = 0; j < 100000000; j++)
        {
            if (j % 100000 == 0)
                yield();
        }
        exit();
    }

    sleep(50); // Keep at 50 ticks to capture scheduling events while processes are running

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        // printf(1, "Before wait, looking for processes:\n");
        for (int j = 0; j < 64; j++)
        {
            /*if (info[j].pid > 0)
                printf(1, "info[%d]: pid=%d, tickets=%d, scheduled=%d\n",
                       j, info[j].pid, info[j].tickets, info[j].ticks_scheduled);*/
            for (int k = 0; k < 17; k++) // Process A
                if (info[j].pid == pids[k])
                    sched1 += info[j].ticks_scheduled;
            for (int k = 17; k < 34; k++) // Process B
                if (info[j].pid == pids[k])
                    sched2 += info[j].ticks_scheduled;
            for (int k = 34; k < 50; k++) // Process C
                if (info[j].pid == pids[k])
                    sched3 += info[j].ticks_scheduled;
        }
    }

    for (i = 0; i < 50; i++)
    {
        wait();
    }

    end_time = uptime();
    printf(1, "Test runtime: %d ticks\n", end_time - start_time);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
        return;
    }

    printf(1, "After forks, looking for active processes:\n");
    for (i = 0; i < 64; i++)
    {
        /*if (info[i].pid > 0)
            printf(1, "info[%d]: pid=%d, tickets=%d, scheduled=%d\n",
                   i, info[i].pid, info[i].tickets, info[i].ticks_scheduled);*/
    }

    int total_sched = sched1 + sched2 + sched3;
    if (total_sched > 0)
    {
        printf(1, "Results:\n");
        printf(1, "  Process A: %d tickets, %d schedules (%d%%)\n",
               tickets1, sched1, (sched1 * 100) / total_sched);
        printf(1, "  Process B: %d tickets, %d schedules (%d%%)\n",
               tickets2, sched2, (sched2 * 100) / total_sched);
        printf(1, "  Process C: %d tickets, %d schedules (%d%%)\n",
               tickets3, sched3, (sched3 * 100) / total_sched);
        printf(1, "  Expected: A=%d%%, B=%d%%, C=%d%%\n",
               (tickets1 * 100) / (tickets1 + tickets2 + tickets3),
               (tickets2 * 100) / (tickets1 + tickets2 + tickets3),
               (tickets3 * 100) / (tickets1 + tickets2 + tickets3));
    }
    else
    {
        printf(1, "No scheduling data collected (sched1=%d, sched2=%d, sched3=%d)\n",
               sched1, sched2, sched3);
    }

    *sched1_out = sched1;
    *sched2_out = sched2;
    *sched3_out = sched3;
}

int main(int argc, char *argv[])
{
    printf(1, "Starting lottery scheduler tests\n");

    int num_runs = 5;
    int total_a, total_b, total_c;
    int total_schedules;

    // Test 1: CPU-heavy
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_a = 0, sched_b = 0, sched_c = 0;
        printf(1, "Run %d:\n", i + 1);
        run_workload_test(30, 20, 10, 500000000, 500000000, 500000000, 0, 0, 0, "Test 1: CPU-heavy",
                          &sched_a, &sched_b, &sched_c);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_schedules += (sched_a + sched_b + sched_c);

        printf(1, "\n");
        sleep(5);
    }

    printf(1, "\nAverage Results Over %d Runs for Test 1:\n", num_runs);
    int percent_a_whole = (total_a * 1000) / total_schedules;
    int percent_a_int = percent_a_whole / 10;
    int percent_a_dec = percent_a_whole % 10;

    int percent_b_whole = (total_b * 1000) / total_schedules;
    int percent_b_int = percent_b_whole / 10;
    int percent_b_dec = percent_b_whole % 10;

    int percent_c_whole = (total_c * 1000) / total_schedules;
    int percent_c_int = percent_c_whole / 10;
    int percent_c_dec = percent_c_whole % 10;

    printf(1, "  Process A: %d schedules (%d.%d%%), Expected: 50%%\n",
           total_a, percent_a_int, percent_a_dec);
    printf(1, "  Process B: %d schedules (%d.%d%%), Expected: 33%%\n",
           total_b, percent_b_int, percent_b_dec);
    printf(1, "  Process C: %d schedules (%d.%d%%), Expected: 16%%\n",
           total_c, percent_c_int, percent_c_dec);
    printf(1, "\n");

    // Test 2: Switch Overhead
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_a = 0, sched_b = 0, sched_c = 0;
        printf(1, "Run %d:\n", i + 1);
        run_switch_test(30, 20, 10, &sched_a, &sched_b, &sched_c);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_schedules += (sched_a + sched_b + sched_c);

        printf(1, "\n");
        sleep(5);
    }

    printf(1, "\nAverage Results Over %d Runs for Test 2:\n", num_runs);
    percent_a_whole = (total_a * 1000) / total_schedules;
    percent_a_int = percent_a_whole / 10;
    percent_a_dec = percent_a_whole % 10;

    percent_b_whole = (total_b * 1000) / total_schedules;
    percent_b_int = percent_b_whole / 10;
    percent_b_dec = percent_b_whole % 10;

    percent_c_whole = (total_c * 1000) / total_schedules;
    percent_c_int = percent_c_whole / 10;
    percent_c_dec = percent_c_whole % 10;

    printf(1, "  Process A: %d schedules (%d.%d%%), Expected: 50%%\n",
           total_a, percent_a_int, percent_a_dec);
    printf(1, "  Process B: %d schedules (%d.%d%%), Expected: 33%%\n",
           total_b, percent_b_int, percent_b_dec);
    printf(1, "  Process C: %d schedules (%d.%d%%), Expected: 16%%\n",
           total_c, percent_c_int, percent_c_dec);
    printf(1, "\n");

    // Test 3: Starvation Check (Modified)
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_a = 0, sched_b = 0, sched_c = 0;
        printf(1, "Run %d:\n", i + 1);
        run_workload_test(50, 10, 1, 10000000, 10000000, 10000000, 0, 0, 0, "Test 3: Starvation Check",
                          &sched_a, &sched_b, &sched_c);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_schedules += (sched_a + sched_b + sched_c);

        printf(1, "\n");
        sleep(5);
    }

    printf(1, "\nAverage Results Over %d Runs for Test 3:\n", num_runs);
    percent_a_whole = (total_a * 1000) / total_schedules;
    percent_a_int = percent_a_whole / 10;
    percent_a_dec = percent_a_whole % 10;

    percent_b_whole = (total_b * 1000) / total_schedules;
    percent_b_int = percent_b_whole / 10;
    percent_b_dec = percent_b_whole % 10;

    percent_c_whole = (total_c * 1000) / total_schedules;
    percent_c_int = percent_c_whole / 10;
    percent_c_dec = percent_c_whole % 10;

    printf(1, "  Process A: %d schedules (%d.%d%%), Expected: 82%%\n",
           total_a, percent_a_int, percent_a_dec);
    printf(1, "  Process B: %d schedules (%d.%d%%), Expected: 16%%\n",
           total_b, percent_b_int, percent_b_dec);
    printf(1, "  Process C: %d schedules (%d.%d%%), Expected: 1%%\n",
           total_c, percent_c_int, percent_c_dec);
    printf(1, "\n");

    printf(1, "\nAll tests complete\n");
    sleep(5);
    exit();
}