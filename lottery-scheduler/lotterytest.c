/*
 * lotterytest.c - Lottery Scheduler Test Suite for xv6
 *
 * This program runs a series of tests to evaluate the performance and fairness
 * of a lottery scheduler implemented in the xv6 operating system. Each test
 * creates multiple processes with varying ticket counts and workloads to measure
 * scheduling behavior under different conditions.
 *
 * Tests:
 * 1. Low Process Count: 3 processes with tickets 30, 20, 10.
 * 2. Basic Fairness: 8 processes with tickets 30, 30, 20, 20, 10, 10, 5, 5.
 * 3. Switch Overhead: 50 processes to test context switch performance.
 * 4. Starvation Check: 8 processes with tickets 50, 50, 10, 10, 1, 1, 1, 1.
 * 5. Grouped Ticket Levels: 30 processes in three groups (10 with 1 ticket, 10 with 5 tickets, 10 with 10 tickets).
 * 6. Mixed Workload: 20 processes with different behaviors (CPU-heavy, I/O-bound, short-lived, mixed).
 */

#include "types.h"
#include "stat.h"
#include "user.h"

// Test 1: Evaluate scheduling with a low number of processes (3 processes)
// Tickets: A=30, B=20, C=10
// Expected Proportions: A=50%, B=33%, C=16%
void run_low_process_test(int tickets_a, int tickets_b, int tickets_c,
                          int *sched_a_out, int *sched_b_out, int *sched_c_out)
{
    int pid_a, pid_b, pid_c; // Process IDs for the three child processes
    int i;
    volatile int counter = 0;                  // Dummy counter to simulate CPU work
    struct pinfo info[64];                     // Array to store process information from getpinfo
    int sched_a = 0, sched_b = 0, sched_c = 0; // Scheduling counts for each process

    printf(1, "Test 1: Low Process Count: Tickets=%d,%d,%d\n", tickets_a, tickets_b, tickets_c);

    // Create first child process (A)
    pid_a = fork();
    if (pid_a == 0)
    {
        settickets(tickets_a); // Set tickets for process A
        for (i = 0; i < 500000000; i++)
        { // Simulate CPU-intensive work
            counter++;
            if (i % 5000 == 0)
                yield(); // Yield every 5000 iterations to allow scheduling
        }
        exit();
    }

    // Create second child process (B)
    pid_b = fork();
    if (pid_b == 0)
    {
        settickets(tickets_b); // Set tickets for process B
        for (i = 0; i < 500000000; i++)
        {
            counter++;
            if (i % 5000 == 0)
                yield();
        }
        exit();
    }

    // Create third child process (C)
    pid_c = fork();
    if (pid_c == 0)
    {
        settickets(tickets_c); // Set tickets for process C
        for (i = 0; i < 500000000; i++)
        {
            counter++;
            if (i % 5000 == 0)
                yield();
        }
        exit();
    }

    sleep(50); // Allow child processes to run for a short time

    // Collect scheduling statistics using getpinfo system call
    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (i = 0; i < 64; i++)
        {
            if (info[i].pid == pid_a)
                sched_a = info[i].ticks_scheduled;
            if (info[i].pid == pid_b)
                sched_b = info[i].ticks_scheduled;
            if (info[i].pid == pid_c)
                sched_c = info[i].ticks_scheduled;
        }
    }

    // Wait for all child processes to complete
    wait();
    wait();
    wait();

    // Calculate and print scheduling proportions
    int total_sched = sched_a + sched_b + sched_c;
    if (total_sched > 0)
    {
        printf(1, "  A: %d tickets, %d schedules (%d%%)\n",
               tickets_a, sched_a, (sched_a * 100) / total_sched);
        printf(1, "  B: %d tickets, %d schedules (%d%%)\n",
               tickets_b, sched_b, (sched_b * 100) / total_sched);
        printf(1, "  C: %d tickets, %d schedules (%d%%)\n",
               tickets_c, sched_c, (sched_c * 100) / total_sched);
        printf(1, "  Expected: A=50%%, B=33%%, C=16%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    // Return scheduling counts to caller
    *sched_a_out = sched_a;
    *sched_b_out = sched_b;
    *sched_c_out = sched_c;
}

// Test 2: Evaluate basic fairness with 8 processes
// Tickets: A=30, B=30, C=20, D=20, E=10, F=10, G=5, H=5
// Expected Proportions: A+B=46%, C+D=31%, E+F=15%, G+H=8%
void run_basic_fairness_test(int *sched_a_out, int *sched_b_out, int *sched_c_out, int *sched_d_out,
                             int *sched_e_out, int *sched_f_out, int *sched_g_out, int *sched_h_out)
{
    int pids[8]; // Array to store process IDs
    int sched_a = 0, sched_b = 0, sched_c = 0, sched_d = 0,
        sched_e = 0, sched_f = 0, sched_g = 0, sched_h = 0; // Scheduling counts
    struct pinfo info[64];
    int i;

    printf(1, "Test 2: Basic Fairness: Tickets=30,30,20,20,10,10,5,5\n");

    // Create 8 child processes with varying ticket counts
    for (i = 0; i < 8; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            int tickets = (i < 2) ? 30 : (i < 4) ? 20
                                     : (i < 6)   ? 10
                                                 : 5;
            settickets(tickets);
            for (volatile int j = 0; j < 500000000; j++)
            { // Simulate CPU-intensive work
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    sleep(50);

    // Collect scheduling statistics
    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            if (info[j].pid == pids[0])
                sched_a = info[j].ticks_scheduled;
            if (info[j].pid == pids[1])
                sched_b = info[j].ticks_scheduled;
            if (info[j].pid == pids[2])
                sched_c = info[j].ticks_scheduled;
            if (info[j].pid == pids[3])
                sched_d = info[j].ticks_scheduled;
            if (info[j].pid == pids[4])
                sched_e = info[j].ticks_scheduled;
            if (info[j].pid == pids[5])
                sched_f = info[j].ticks_scheduled;
            if (info[j].pid == pids[6])
                sched_g = info[j].ticks_scheduled;
            if (info[j].pid == pids[7])
                sched_h = info[j].ticks_scheduled;
        }
    }

    // Wait for all child processes to complete
    for (i = 0; i < 8; i++)
    {
        wait();
    }

    // Calculate and print scheduling proportions
    int total_sched = sched_a + sched_b + sched_c + sched_d + sched_e + sched_f + sched_g + sched_h;
    if (total_sched > 0)
    {
        printf(1, "  A: 30 tickets, %d schedules (%d%%)\n", sched_a, (sched_a * 100) / total_sched);
        printf(1, "  B: 30 tickets, %d schedules (%d%%)\n", sched_b, (sched_b * 100) / total_sched);
        printf(1, "  C: 20 tickets, %d schedules (%d%%)\n", sched_c, (sched_c * 100) / total_sched);
        printf(1, "  D: 20 tickets, %d schedules (%d%%)\n", sched_d, (sched_d * 100) / total_sched);
        printf(1, "  E: 10 tickets, %d schedules (%d%%)\n", sched_e, (sched_e * 100) / total_sched);
        printf(1, "  F: 10 tickets, %d schedules (%d%%)\n", sched_f, (sched_f * 100) / total_sched);
        printf(1, "  G: 5 tickets, %d schedules (%d%%)\n", sched_g, (sched_g * 100) / total_sched);
        printf(1, "  H: 5 tickets, %d schedules (%d%%)\n", sched_h, (sched_h * 100) / total_sched);
        printf(1, "  Expected: A+B=46%%, C+D=31%%, E+F=15%%, G+H=8%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    // Return scheduling counts
    *sched_a_out = sched_a;
    *sched_b_out = sched_b;
    *sched_c_out = sched_c;
    *sched_d_out = sched_d;
    *sched_e_out = sched_e;
    *sched_f_out = sched_f;
    *sched_g_out = sched_g;
    *sched_h_out = sched_h;
}

// Test 3: Evaluate context switch overhead with 50 processes
// Tickets: Group A=30 (17 procs), Group B=20 (17 procs), Group C=10 (16 procs)
// Expected Proportions: A=50%, B=33%, C=16%
void run_switch_test(int tickets_a, int tickets_b, int tickets_c,
                     int *sched_group_a_out, int *sched_group_b_out, int *sched_group_c_out)
{
    int pids[50];
    int sched_group_a = 0, sched_group_b = 0, sched_group_c = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 3: Switch Overhead: Tickets=%d,%d,%d\n", tickets_a, tickets_b, tickets_c);

    // Create 50 child processes in three groups
    for (i = 0; i < 16; i++)
    {
        // Group C: 16 processes with tickets_c
        pids[34 + i] = fork();
        if (pids[34 + i] == 0)
        {
            settickets(tickets_c);
            for (volatile int j = 0; j < 100000000; j++)
            {
                if (j % 100000 == 0)
                    yield();
            }
            exit();
        }
        // Group A: 16 processes with tickets_a
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(tickets_a);
            for (volatile int j = 0; j < 100000000; j++)
            {
                if (j % 100000 == 0)
                    yield();
            }
            exit();
        }
        // Group B: 16 processes with tickets_b
        pids[17 + i] = fork();
        if (pids[17 + i] == 0)
        {
            settickets(tickets_b);
            for (volatile int j = 0; j < 100000000; j++)
            {
                if (j % 100000 == 0)
                    yield();
            }
            exit();
        }
    }
    // 17th process for Group A
    pids[16] = fork();
    if (pids[16] == 0)
    {
        settickets(tickets_a);
        for (volatile int j = 0; j < 100000000; j++)
        {
            if (j % 100000 == 0)
                yield();
        }
        exit();
    }
    // 17th process for Group B
    pids[33] = fork();
    if (pids[33] == 0)
    {
        settickets(tickets_b);
        for (volatile int j = 0; j < 100000000; j++)
        {
            if (j % 100000 == 0)
                yield();
        }
        exit();
    }

    sleep(50);

    // Collect scheduling statistics
    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            for (int k = 0; k < 17; k++) // Group A: pids[0] to pids[16]
                if (info[j].pid == pids[k])
                    sched_group_a += info[j].ticks_scheduled;
            for (int k = 17; k < 34; k++) // Group B: pids[17] to pids[33]
                if (info[j].pid == pids[k])
                    sched_group_b += info[j].ticks_scheduled;
            for (int k = 34; k < 50; k++) // Group C: pids[34] to pids[49]
                if (info[j].pid == pids[k])
                    sched_group_c += info[j].ticks_scheduled;
        }
    }

    // Wait for all child processes
    for (i = 0; i < 50; i++)
    {
        wait();
    }

    // Calculate and print scheduling proportions
    int total_sched = sched_group_a + sched_group_b + sched_group_c;
    if (total_sched > 0)
    {
        printf(1, "  A: %d tickets, %d schedules (%d%%)\n",
               tickets_a, sched_group_a, (sched_group_a * 100) / total_sched);
        printf(1, "  B: %d tickets, %d schedules (%d%%)\n",
               tickets_b, sched_group_b, (sched_group_b * 100) / total_sched);
        printf(1, "  C: %d tickets, %d schedules (%d%%)\n",
               tickets_c, sched_group_c, (sched_group_c * 100) / total_sched);
        printf(1, "  Expected: A=50%%, B=33%%, C=16%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched_group_a_out = sched_group_a;
    *sched_group_b_out = sched_group_b;
    *sched_group_c_out = sched_group_c;
}

// Test 4: Check for starvation with 8 processes
// Tickets: A=50, B=50, C=10, D=10, E=1, F=1, G=1, H=1
// Expected Proportions: A+B=81%, C+D=16%, E+F+G+H=3%
void run_starvation_test(int *sched_a_out, int *sched_b_out, int *sched_c_out, int *sched_d_out,
                         int *sched_e_out, int *sched_f_out, int *sched_g_out, int *sched_h_out)
{
    int pids[8];
    int sched_a = 0, sched_b = 0, sched_c = 0, sched_d = 0,
        sched_e = 0, sched_f = 0, sched_g = 0, sched_h = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 4: Starvation Check: Tickets=50,50,10,10,1,1,1,1\n");

    // Create 8 child processes with varying ticket counts
    for (i = 0; i < 8; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            int tickets = (i < 2) ? 50 : (i < 4) ? 10
                                                 : 1;
            settickets(tickets);
            for (volatile int j = 0; j < 10000000; j++)
            { // Shorter workload to focus on starvation
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    sleep(50);

    // Collect scheduling statistics
    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            if (info[j].pid == pids[0])
                sched_a = info[j].ticks_scheduled;
            if (info[j].pid == pids[1])
                sched_b = info[j].ticks_scheduled;
            if (info[j].pid == pids[2])
                sched_c = info[j].ticks_scheduled;
            if (info[j].pid == pids[3])
                sched_d = info[j].ticks_scheduled;
            if (info[j].pid == pids[4])
                sched_e = info[j].ticks_scheduled;
            if (info[j].pid == pids[5])
                sched_f = info[j].ticks_scheduled;
            if (info[j].pid == pids[6])
                sched_g = info[j].ticks_scheduled;
            if (info[j].pid == pids[7])
                sched_h = info[j].ticks_scheduled;
        }
    }

    // Wait for all child processes
    for (i = 0; i < 8; i++)
    {
        wait();
    }

    // Calculate and print scheduling proportions
    int total_sched = sched_a + sched_b + sched_c + sched_d + sched_e + sched_f + sched_g + sched_h;
    if (total_sched > 0)
    {
        printf(1, "  A: 50 tickets, %d schedules (%d%%)\n", sched_a, (sched_a * 100) / total_sched);
        printf(1, "  B: 50 tickets, %d schedules (%d%%)\n", sched_b, (sched_b * 100) / total_sched);
        printf(1, "  C: 10 tickets, %d schedules (%d%%)\n", sched_c, (sched_c * 100) / total_sched);
        printf(1, "  D: 10 tickets, %d schedules (%d%%)\n", sched_d, (sched_d * 100) / total_sched);
        printf(1, "  E: 1 tickets, %d schedules (%d%%)\n", sched_e, (sched_e * 100) / total_sched);
        printf(1, "  F: 1 tickets, %d schedules (%d%%)\n", sched_f, (sched_f * 100) / total_sched);
        printf(1, "  G: 1 tickets, %d schedules (%d%%)\n", sched_g, (sched_g * 100) / total_sched);
        printf(1, "  H: 1 tickets, %d schedules (%d%%)\n", sched_h, (sched_h * 100) / total_sched);
        printf(1, "  Expected: A+B=81%%, C+D=16%%, E+F+G+H=3%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched_a_out = sched_a;
    *sched_b_out = sched_b;
    *sched_c_out = sched_c;
    *sched_d_out = sched_d;
    *sched_e_out = sched_e;
    *sched_f_out = sched_f;
    *sched_g_out = sched_g;
    *sched_h_out = sched_h;
}

// Test 5: Evaluate scheduling with grouped ticket levels (30 processes)
// Group 1: 10 processes with 1 ticket each (total 10 tickets)
// Group 2: 10 processes with 5 tickets each (total 50 tickets)
// Group 3: 10 processes with 10 tickets each (total 100 tickets)
// Expected Proportions: Group 1=6%, Group 2=31%, Group 3=62%
void run_grouped_ticket_test(int *sched_group1_out, int *sched_group2_out, int *sched_group3_out)
{
    int pids[30];
    int sched_group1 = 0, sched_group2 = 0, sched_group3 = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 5: Grouped Ticket Levels: 10 procs with 1 ticket, 10 with 5 tickets, 10 with 10 tickets\n");

    // Group 1: 10 processes with 1 ticket each
    for (i = 0; i < 10; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(1);
            for (volatile int j = 0; j < 500000000; j++)
            {
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    // Group 2: 10 processes with 5 tickets each
    for (i = 10; i < 20; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(5);
            for (volatile int j = 0; j < 500000000; j++)
            {
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    // Group 3: 10 processes with 10 tickets each
    for (i = 20; i < 30; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(10);
            for (volatile int j = 0; j < 500000000; j++)
            {
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    sleep(50);

    // Collect scheduling statistics
    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            for (int k = 0; k < 10; k++) // Group 1: pids[0] to pids[9]
                if (info[j].pid == pids[k])
                    sched_group1 += info[j].ticks_scheduled;
            for (int k = 10; k < 20; k++) // Group 2: pids[10] to pids[19]
                if (info[j].pid == pids[k])
                    sched_group2 += info[j].ticks_scheduled;
            for (int k = 20; k < 30; k++) // Group 3: pids[20] to pids[29]
                if (info[j].pid == pids[k])
                    sched_group3 += info[j].ticks_scheduled;
        }
    }

    // Wait for all child processes
    for (i = 0; i < 30; i++)
    {
        wait();
    }

    // Calculate and print scheduling proportions
    int total_sched = sched_group1 + sched_group2 + sched_group3;
    if (total_sched > 0)
    {
        printf(1, "  Group 1: Total 10 tickets, %d schedules (%d%%)\n",
               sched_group1, (sched_group1 * 100) / total_sched);
        printf(1, "  Group 2: Total 50 tickets, %d schedules (%d%%)\n",
               sched_group2, (sched_group2 * 100) / total_sched);
        printf(1, "  Group 3: Total 100 tickets, %d schedules (%d%%)\n",
               sched_group3, (sched_group3 * 100) / total_sched);
        printf(1, "  Expected: Group 1=6%%, Group 2=31%%, Group 3=62%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched_group1_out = sched_group1;
    *sched_group2_out = sched_group2;
    *sched_group3_out = sched_group3;
}

// Test 6: Evaluate scheduling with mixed workloads (20 processes)
// Group 1: 5 CPU-heavy processes (20 tickets each, total 100 tickets)
// Group 2: 5 I/O-bound processes (15 tickets each, total 75 tickets)
// Group 3: 5 short-lived processes (10 tickets each, total 50 tickets)
// Group 4: 5 mixed processes (5 tickets each, total 25 tickets)
// Expected Proportions: CPU-heavy=40%, I/O-bound=30%, Short-lived=20%, Mixed=10%
void run_mixed_workload_test(int *sched_group1_out, int *sched_group2_out,
                             int *sched_group3_out, int *sched_group4_out)
{
    int pids[20];
    int sched_group1 = 0, sched_group2 = 0, sched_group3 = 0, sched_group4 = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 6: Mixed Workload: 5 CPU-heavy (20 tickets), 5 I/O-bound (15 tickets), "
              "5 short-lived (10 tickets), 5 mixed (5 tickets)\n");

    // Group 1: CPU-heavy (20 tickets each)
    for (i = 0; i < 5; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(20);
            for (volatile int j = 0; j < 1000000; j++)
            {
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    // Group 2: I/O-bound (15 tickets each)
    for (i = 5; i < 10; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(15);
            for (int j = 0; j < 1000; j++)
            {
                sleep(1); // Simulate I/O operations
            }
            exit();
        }
    }

    // Group 3: Short-lived (10 tickets each)
    for (i = 10; i < 15; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(10);
            for (volatile int j = 0; j < 100000; j++)
                ; // Short CPU burst
            exit();
        }
    }

    // Group 4: Mixed (5 tickets each)
    for (i = 15; i < 20; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(5);
            for (int j = 0; j < 1000; j++)
            {
                for (volatile int k = 0; k < 10000; k++)
                    ;     // CPU burst
                sleep(1); // I/O operation
            }
            exit();
        }
    }

    sleep(50);

    // Collect scheduling statistics
    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            for (int k = 0; k < 5; k++) // Group 1: pids[0] to pids[4]
                if (info[j].pid == pids[k])
                    sched_group1 += info[j].ticks_scheduled;
            for (int k = 5; k < 10; k++) // Group 2: pids[5] to pids[9]
                if (info[j].pid == pids[k])
                    sched_group2 += info[j].ticks_scheduled;
            for (int k = 10; k < 15; k++) // Group 3: pids[10] to pids[14]
                if (info[j].pid == pids[k])
                    sched_group3 += info[j].ticks_scheduled;
            for (int k = 15; k < 20; k++) // Group 4: pids[15] to pids[19]
                if (info[j].pid == pids[k])
                    sched_group4 += info[j].ticks_scheduled;
        }
    }

    // Wait for all child processes
    for (i = 0; i < 20; i++)
    {
        wait();
    }

    // Calculate and print scheduling proportions
    int total_sched = sched_group1 + sched_group2 + sched_group3 + sched_group4;
    if (total_sched > 0)
    {
        printf(1, "  CPU-heavy: Total 100 tickets, %d schedules (%d%%)\n",
               sched_group1, (sched_group1 * 100) / total_sched);
        printf(1, "  I/O-bound: Total 75 tickets, %d schedules (%d%%)\n",
               sched_group2, (sched_group2 * 100) / total_sched);
        printf(1, "  Short-lived: Total 50 tickets, %d schedules (%d%%)\n",
               sched_group3, (sched_group3 * 100) / total_sched);
        printf(1, "  Mixed: Total 25 tickets, %d schedules (%d%%)\n",
               sched_group4, (sched_group4 * 100) / total_sched);
        printf(1, "  Expected: CPU-heavy=40%%, I/O-bound=30%%, Short-lived=20%%, Mixed=10%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched_group1_out = sched_group1;
    *sched_group2_out = sched_group2;
    *sched_group3_out = sched_group3;
    *sched_group4_out = sched_group4;
}

// Main function to run all lottery scheduler tests
int main(int argc, char *argv[])
{
    printf(1, "Starting lottery scheduler tests\n");

    const int num_runs = 5; // Number of runs for each test to average results
    int total_a, total_b, total_c, total_d, total_e, total_f, total_g, total_h;
    int total_schedules;

    // Test 1: Low Process Count
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_a = 0, sched_b = 0, sched_c = 0;
        printf(1, "Run %d:\n", i + 1);
        run_low_process_test(30, 20, 10, &sched_a, &sched_b, &sched_c);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_schedules += (sched_a + sched_b + sched_c);

        printf(1, "\n");
        sleep(5);
    }

    // Print average results for Test 1 with decimal precision
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

    printf(1, "  A: %d schedules (%d.%d%%), Expected: 50%%\n",
           total_a, percent_a_int, percent_a_dec);
    printf(1, "  B: %d schedules (%d.%d%%), Expected: 33%%\n",
           total_b, percent_b_int, percent_b_dec);
    printf(1, "  C: %d schedules (%d.%d%%), Expected: 16%%\n",
           total_c, percent_c_int, percent_c_dec);
    printf(1, "\n");

    // Test 2: Basic Fairness
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_d = 0;
    total_e = 0;
    total_f = 0;
    total_g = 0;
    total_h = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_a = 0, sched_b = 0, sched_c = 0, sched_d = 0,
            sched_e = 0, sched_f = 0, sched_g = 0, sched_h = 0;
        printf(1, "Run %d:\n", i + 1);
        run_basic_fairness_test(&sched_a, &sched_b, &sched_c, &sched_d,
                                &sched_e, &sched_f, &sched_g, &sched_h);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_d += sched_d;
        total_e += sched_e;
        total_f += sched_f;
        total_g += sched_g;
        total_h += sched_h;
        total_schedules += (sched_a + sched_b + sched_c + sched_d +
                            sched_e + sched_f + sched_g + sched_h);

        printf(1, "\n");
        sleep(5);
    }

    printf(1, "\nAverage Results Over %d Runs for Test 2:\n", num_runs);
    int percent_ab_whole = ((total_a + total_b) * 1000) / total_schedules;
    int percent_ab_int = percent_ab_whole / 10;
    int percent_ab_dec = percent_ab_whole % 10;

    int percent_cd_whole = ((total_c + total_d) * 1000) / total_schedules;
    int percent_cd_int = percent_cd_whole / 10;
    int percent_cd_dec = percent_cd_whole % 10;

    int percent_ef_whole = ((total_e + total_f) * 1000) / total_schedules;
    int percent_ef_int = percent_ef_whole / 10;
    int percent_ef_dec = percent_ef_whole % 10;

    int percent_gh_whole = ((total_g + total_h) * 1000) / total_schedules;
    int percent_gh_int = percent_gh_whole / 10;
    int percent_gh_dec = percent_gh_whole % 10;

    printf(1, "  A+B: %d schedules (%d.%d%%), Expected: 46%%\n",
           total_a + total_b, percent_ab_int, percent_ab_dec);
    printf(1, "  C+D: %d schedules (%d.%d%%), Expected: 31%%\n",
           total_c + total_d, percent_cd_int, percent_cd_dec);
    printf(1, "  E+F: %d schedules (%d.%d%%), Expected: 15%%\n",
           total_e + total_f, percent_ef_int, percent_ef_dec);
    printf(1, "  G+H: %d schedules (%d.%d%%), Expected: 8%%\n",
           total_g + total_h, percent_gh_int, percent_gh_dec);
    printf(1, "\n");

    // Test 3: Switch Overhead
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

    printf(1, "  A: %d schedules (%d.%d%%), Expected: 50%%\n",
           total_a, percent_a_int, percent_a_dec);
    printf(1, "  B: %d schedules (%d.%d%%), Expected: 33%%\n",
           total_b, percent_b_int, percent_b_dec);
    printf(1, "  C: %d schedules (%d.%d%%), Expected: 16%%\n",
           total_c, percent_c_int, percent_c_dec);
    printf(1, "\n");

    // Test 4: Starvation Check
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_d = 0;
    total_e = 0;
    total_f = 0;
    total_g = 0;
    total_h = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_a = 0, sched_b = 0, sched_c = 0, sched_d = 0,
            sched_e = 0, sched_f = 0, sched_g = 0, sched_h = 0;
        printf(1, "Run %d:\n", i + 1);
        run_starvation_test(&sched_a, &sched_b, &sched_c, &sched_d,
                            &sched_e, &sched_f, &sched_g, &sched_h);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_d += sched_d;
        total_e += sched_e;
        total_f += sched_f;
        total_g += sched_g;
        total_h += sched_h;
        total_schedules += (sched_a + sched_b + sched_c + sched_d +
                            sched_e + sched_f + sched_g + sched_h);

        printf(1, "\n");
        sleep(5);
    }

    printf(1, "\nAverage Results Over %d Runs for Test 4:\n", num_runs);
    percent_ab_whole = ((total_a + total_b) * 1000) / total_schedules;
    percent_ab_int = percent_ab_whole / 10;
    percent_ab_dec = percent_ab_whole % 10;

    percent_cd_whole = ((total_c + total_d) * 1000) / total_schedules;
    percent_cd_int = percent_cd_whole / 10;
    percent_cd_dec = percent_cd_whole % 10;

    int percent_efgh_whole = ((total_e + total_f + total_g + total_h) * 1000) / total_schedules;
    int percent_efgh_int = percent_efgh_whole / 10;
    int percent_efgh_dec = percent_efgh_whole % 10;

    printf(1, "  A+B: %d schedules (%d.%d%%), Expected: 81%%\n",
           total_a + total_b, percent_ab_int, percent_ab_dec);
    printf(1, "  C+D: %d schedules (%d.%d%%), Expected: 16%%\n",
           total_c + total_d, percent_cd_int, percent_cd_dec);
    printf(1, "  E+F+G+H: %d schedules (%d.%d%%), Expected: 3%%\n",
           total_e + total_f + total_g + total_h, percent_efgh_int, percent_efgh_dec);
    printf(1, "\n");

    // Test 5: Grouped Ticket Levels
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_group1 = 0, sched_group2 = 0, sched_group3 = 0;
        printf(1, "Run %d:\n", i + 1);
        run_grouped_ticket_test(&sched_group1, &sched_group2, &sched_group3);

        total_a += sched_group1;
        total_b += sched_group2;
        total_c += sched_group3;
        total_schedules += (sched_group1 + sched_group2 + sched_group3);

        printf(1, "\n");
        sleep(5);
    }

    printf(1, "\nAverage Results Over %d Runs for Test 5:\n", num_runs);
    percent_a_whole = (total_a * 1000) / total_schedules;
    percent_a_int = percent_a_whole / 10;
    percent_a_dec = percent_a_whole % 10;

    percent_b_whole = (total_b * 1000) / total_schedules;
    percent_b_int = percent_b_whole / 10;
    percent_b_dec = percent_b_whole % 10;

    percent_c_whole = (total_c * 1000) / total_schedules;
    percent_c_int = percent_c_whole / 10;
    percent_c_dec = percent_c_whole % 10;

    printf(1, "  Group 1: %d schedules (%d.%d%%), Expected: 6%%\n",
           total_a, percent_a_int, percent_a_dec);
    printf(1, "  Group 2: %d schedules (%d.%d%%), Expected: 31%%\n",
           total_b, percent_b_int, percent_b_dec);
    printf(1, "  Group 3: %d schedules (%d.%d%%), Expected: 62%%\n",
           total_c, percent_c_int, percent_c_dec);
    printf(1, "\n");

    // Test 6: Mixed Workload
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_d = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_group1 = 0, sched_group2 = 0, sched_group3 = 0, sched_group4 = 0;
        printf(1, "Run %d:\n", i + 1);
        run_mixed_workload_test(&sched_group1, &sched_group2, &sched_group3, &sched_group4);

        total_a += sched_group1;
        total_b += sched_group2;
        total_c += sched_group3;
        total_d += sched_group4;
        total_schedules += (sched_group1 + sched_group2 + sched_group3 + sched_group4);

        printf(1, "\n");
        sleep(5);
    }

    printf(1, "\nAverage Results Over %d Runs for Test 6:\n", num_runs);
    percent_a_whole = (total_a * 1000) / total_schedules;
    percent_a_int = percent_a_whole / 10;
    percent_a_dec = percent_a_whole % 10;

    percent_b_whole = (total_b * 1000) / total_schedules;
    percent_b_int = percent_b_whole / 10;
    percent_b_dec = percent_b_whole % 10;

    percent_c_whole = (total_c * 1000) / total_schedules;
    percent_c_int = percent_c_whole / 10;
    percent_c_dec = percent_c_whole % 10;

    int percent_d_whole = (total_d * 1000) / total_schedules;
    int percent_d_int = percent_d_whole / 10;
    int percent_d_dec = percent_d_whole % 10;

    printf(1, "  CPU-heavy: %d schedules (%d.%d%%), Expected: 40%%\n",
           total_a, percent_a_int, percent_a_dec);
    printf(1, "  I/O-bound: %d schedules (%d.%d%%), Expected: 30%%\n",
           total_b, percent_b_int, percent_b_dec);
    printf(1, "  Short-lived: %d schedules (%d.%d%%), Expected: 20%%\n",
           total_c, percent_c_int, percent_c_dec);
    printf(1, "  Mixed: %d schedules (%d.%d%%), Expected: 10%%\n",
           total_d, percent_d_int, percent_d_dec);
    printf(1, "\n");

    printf(1, "\nAll tests complete\n");
    sleep(5);
    exit();
}