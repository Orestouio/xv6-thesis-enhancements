#include "types.h"
#include "stat.h"
#include "user.h"

void run_test(int tickets1, int tickets2, int tickets3, int test_num)
{
    int pid1, pid2, pid3;
    int i;
    volatile int counter = 0;
    struct pinfo info[64];

    printf(1, "Test %d: Tickets=%d,%d,%d\n", test_num, tickets1, tickets2, tickets3);

    pid1 = fork();
    if (pid1 == 0)
    {
        settickets(tickets1);
        for (i = 0; i < 50000000; i++)
            counter++;
        printf(1, "Process A (pid=%d, %d tickets) done\n", getpid(), tickets1);
        exit();
    }

    pid2 = fork();
    if (pid2 == 0)
    {
        settickets(tickets2);
        for (i = 0; i < 50000000; i++)
            counter++;
        printf(1, "Process B (pid=%d, %d tickets) done\n", getpid(), tickets2);
        exit();
    }

    pid3 = fork();
    if (pid3 == 0)
    {
        settickets(tickets3);
        for (i = 0; i < 50000000; i++)
            counter++;
        printf(1, "Process C (pid=%d, %d tickets) done\n", getpid(), tickets3);
        exit();
    }

    // Try getpinfo before wait()
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
        }
    }

    wait();
    wait();
    wait();

    // Try getpinfo after wait()
    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
        return;
    }

    printf(1, "After wait, looking for PIDs: %d, %d, %d\n", pid1, pid2, pid3);
    for (i = 0; i < 64; i++)
    {
        if (info[i].pid > 0)
            printf(1, "info[%d]: pid=%d, tickets=%d, scheduled=%d\n",
                   i, info[i].pid, info[i].tickets, info[i].ticks_scheduled);
    }

    int sched1 = 0, sched2 = 0, sched3 = 0;
    for (i = 0; i < 64; i++)
    {
        if (info[i].pid == pid1)
        {
            sched1 = info[i].ticks_scheduled;
            printf(1, "Found pid1=%d, sched1=%d\n", pid1, sched1);
        }
        if (info[i].pid == pid2)
        {
            sched2 = info[i].ticks_scheduled;
            printf(1, "Found pid2=%d, sched2=%d\n", pid2, sched2);
        }
        if (info[i].pid == pid3)
        {
            sched3 = info[i].ticks_scheduled;
            printf(1, "Found pid3=%d, sched3=%d\n", pid3, sched3);
        }
    }

    /*int total_sched = sched1 + sched2 + sched3;
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
    }*/
}

int main(int argc, char *argv[])
{
    printf(1, "Starting lottery scheduler tests\n");

    // Test 1: Balanced (30-20-10)
    run_test(30, 20, 10, 1);
    printf(1, "\n");

    // Test 2: Skewed (50-10-5)
    run_test(50, 10, 5, 2);
    printf(1, "\n");

    // Test 3: High total (100-50-25)
    run_test(100, 50, 25, 3);

    printf(1, "\nAll tests complete\n");
    exit();
}