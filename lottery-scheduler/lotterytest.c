#include "types.h"
#include "stat.h"
#include "user.h"

// Test 1: Low Process Count (3 processes, tickets 30, 20, 10)
void run_low_process_test(int tickets1, int tickets2, int tickets3, int *sched1_out, int *sched2_out, int *sched3_out)
{
    int pid1, pid2, pid3;
    int i;
    volatile int counter = 0;
    struct pinfo info[64];
    int sched1 = 0, sched2 = 0, sched3 = 0;

    printf(1, "Test 1: Low Process Count: Tickets=%d,%d,%d\n", tickets1, tickets2, tickets3);

    pid1 = fork();
    if (pid1 == 0)
    {
        settickets(tickets1);
        for (i = 0; i < 500000000; i++)
        {
            counter++;
            if (i % 5000 == 0)
                yield();
        }
        exit();
    }

    pid2 = fork();
    if (pid2 == 0)
    {
        settickets(tickets2);
        for (i = 0; i < 500000000; i++)
        {
            counter++;
            if (i % 5000 == 0)
                yield();
        }
        exit();
    }

    pid3 = fork();
    if (pid3 == 0)
    {
        settickets(tickets3);
        for (i = 0; i < 500000000; i++)
        {
            counter++;
            if (i % 5000 == 0)
                yield();
        }
        exit();
    }

    sleep(50);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (i = 0; i < 64; i++)
        {
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

    int total_sched = sched1 + sched2 + sched3;
    if (total_sched > 0)
    {
        printf(1, "  A: %d tickets, %d schedules (%d%%)\n",
               tickets1, sched1, (sched1 * 100) / total_sched);
        printf(1, "  B: %d tickets, %d schedules (%d%%)\n",
               tickets2, sched2, (sched2 * 100) / total_sched);
        printf(1, "  C: %d tickets, %d schedules (%d%%)\n",
               tickets3, sched3, (sched3 * 100) / total_sched);
        printf(1, "  Expected: A=50%%, B=33%%, C=16%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched1_out = sched1;
    *sched2_out = sched2;
    *sched3_out = sched3;
}

// Test 2: Basic Fairness (8 processes, tickets 30, 30, 20, 20, 10, 10, 5, 5)
void run_basic_fairness_test(int *sched1_out, int *sched2_out, int *sched3_out, int *sched4_out,
                             int *sched5_out, int *sched6_out, int *sched7_out, int *sched8_out)
{
    int pids[8];
    int sched1 = 0, sched2 = 0, sched3 = 0, sched4 = 0, sched5 = 0, sched6 = 0, sched7 = 0, sched8 = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 2: Basic Fairness: Tickets=30,30,20,20,10,10,5,5\n");

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
            {
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    sleep(50);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            if (info[j].pid == pids[0])
                sched1 = info[j].ticks_scheduled;
            if (info[j].pid == pids[1])
                sched2 = info[j].ticks_scheduled;
            if (info[j].pid == pids[2])
                sched3 = info[j].ticks_scheduled;
            if (info[j].pid == pids[3])
                sched4 = info[j].ticks_scheduled;
            if (info[j].pid == pids[4])
                sched5 = info[j].ticks_scheduled;
            if (info[j].pid == pids[5])
                sched6 = info[j].ticks_scheduled;
            if (info[j].pid == pids[6])
                sched7 = info[j].ticks_scheduled;
            if (info[j].pid == pids[7])
                sched8 = info[j].ticks_scheduled;
        }
    }

    for (i = 0; i < 8; i++)
    {
        wait();
    }

    int total_sched = sched1 + sched2 + sched3 + sched4 + sched5 + sched6 + sched7 + sched8;
    if (total_sched > 0)
    {
        printf(1, "  A: 30 tickets, %d schedules (%d%%)\n", sched1, (sched1 * 100) / total_sched);
        printf(1, "  B: 30 tickets, %d schedules (%d%%)\n", sched2, (sched2 * 100) / total_sched);
        printf(1, "  C: 20 tickets, %d schedules (%d%%)\n", sched3, (sched3 * 100) / total_sched);
        printf(1, "  D: 20 tickets, %d schedules (%d%%)\n", sched4, (sched4 * 100) / total_sched);
        printf(1, "  E: 10 tickets, %d schedules (%d%%)\n", sched5, (sched5 * 100) / total_sched);
        printf(1, "  F: 10 tickets, %d schedules (%d%%)\n", sched6, (sched6 * 100) / total_sched);
        printf(1, "  G: 5 tickets, %d schedules (%d%%)\n", sched7, (sched7 * 100) / total_sched);
        printf(1, "  H: 5 tickets, %d schedules (%d%%)\n", sched8, (sched8 * 100) / total_sched);
        printf(1, "  Expected: A+B=46%%, C+D=31%%, E+F=15%%, G+H=8%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched1_out = sched1;
    *sched2_out = sched2;
    *sched3_out = sched3;
    *sched4_out = sched4;
    *sched5_out = sched5;
    *sched6_out = sched6;
    *sched7_out = sched7;
    *sched8_out = sched8;
}

// Test 3: Switch Overhead (50 processes)
void run_switch_test(int tickets1, int tickets2, int tickets3, int *sched1_out, int *sched2_out, int *sched3_out)
{
    int pids[50];
    int sched1 = 0, sched2 = 0, sched3 = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 3: Switch Overhead: Tickets=%d,%d,%d\n", tickets1, tickets2, tickets3);

    for (i = 0; i < 16; i++)
    {
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

    sleep(50);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            for (int k = 0; k < 17; k++)
                if (info[j].pid == pids[k])
                    sched1 += info[j].ticks_scheduled;
            for (int k = 17; k < 34; k++)
                if (info[j].pid == pids[k])
                    sched2 += info[j].ticks_scheduled;
            for (int k = 34; k < 50; k++)
                if (info[j].pid == pids[k])
                    sched3 += info[j].ticks_scheduled;
        }
    }

    for (i = 0; i < 50; i++)
    {
        wait();
    }

    int total_sched = sched1 + sched2 + sched3;
    if (total_sched > 0)
    {
        printf(1, "  A: %d tickets, %d schedules (%d%%)\n",
               tickets1, sched1, (sched1 * 100) / total_sched);
        printf(1, "  B: %d tickets, %d schedules (%d%%)\n",
               tickets2, sched2, (sched2 * 100) / total_sched);
        printf(1, "  C: %d tickets, %d schedules (%d%%)\n",
               tickets3, sched3, (sched3 * 100) / total_sched);
        printf(1, "  Expected: A=50%%, B=33%%, C=16%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched1_out = sched1;
    *sched2_out = sched2;
    *sched3_out = sched3;
}

// Test 4: Starvation Check (8 processes)
void run_starvation_test(int *sched1_out, int *sched2_out, int *sched3_out, int *sched4_out,
                         int *sched5_out, int *sched6_out, int *sched7_out, int *sched8_out)
{
    int pids[8];
    int sched1 = 0, sched2 = 0, sched3 = 0, sched4 = 0, sched5 = 0, sched6 = 0, sched7 = 0, sched8 = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 4: Starvation Check: Tickets=50,50,10,10,1,1,1,1\n");

    for (i = 0; i < 8; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            int tickets = (i < 2) ? 50 : (i < 4) ? 10
                                                 : 1;
            settickets(tickets);
            for (volatile int j = 0; j < 10000000; j++)
            {
                if (j % 5000 == 0)
                    yield();
            }
            exit();
        }
    }

    sleep(50);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            if (info[j].pid == pids[0])
                sched1 = info[j].ticks_scheduled;
            if (info[j].pid == pids[1])
                sched2 = info[j].ticks_scheduled;
            if (info[j].pid == pids[2])
                sched3 = info[j].ticks_scheduled;
            if (info[j].pid == pids[3])
                sched4 = info[j].ticks_scheduled;
            if (info[j].pid == pids[4])
                sched5 = info[j].ticks_scheduled;
            if (info[j].pid == pids[5])
                sched6 = info[j].ticks_scheduled;
            if (info[j].pid == pids[6])
                sched7 = info[j].ticks_scheduled;
            if (info[j].pid == pids[7])
                sched8 = info[j].ticks_scheduled;
        }
    }

    for (i = 0; i < 8; i++)
    {
        wait();
    }

    int total_sched = sched1 + sched2 + sched3 + sched4 + sched5 + sched6 + sched7 + sched8;
    if (total_sched > 0)
    {
        printf(1, "  A: 50 tickets, %d schedules (%d%%)\n", sched1, (sched1 * 100) / total_sched);
        printf(1, "  B: 50 tickets, %d schedules (%d%%)\n", sched2, (sched2 * 100) / total_sched);
        printf(1, "  C: 10 tickets, %d schedules (%d%%)\n", sched3, (sched3 * 100) / total_sched);
        printf(1, "  D: 10 tickets, %d schedules (%d%%)\n", sched4, (sched4 * 100) / total_sched);
        printf(1, "  E: 1 tickets, %d schedules (%d%%)\n", sched5, (sched5 * 100) / total_sched);
        printf(1, "  F: 1 tickets, %d schedules (%d%%)\n", sched6, (sched6 * 100) / total_sched);
        printf(1, "  G: 1 tickets, %d schedules (%d%%)\n", sched7, (sched7 * 100) / total_sched);
        printf(1, "  H: 1 tickets, %d schedules (%d%%)\n", sched8, (sched8 * 100) / total_sched);
        printf(1, "  Expected: A+B=81%%, C+D=16%%, E+F+G+H=3%%\n");
    }
    else
    {
        printf(1, "No scheduling data collected\n");
    }

    *sched1_out = sched1;
    *sched2_out = sched2;
    *sched3_out = sched3;
    *sched4_out = sched4;
    *sched5_out = sched5;
    *sched6_out = sched6;
    *sched7_out = sched7;
    *sched8_out = sched8;
}

// Test 5: Grouped Ticket Levels (30 processes)
void run_grouped_ticket_test(int *sched_group1_out, int *sched_group2_out, int *sched_group3_out)
{
    int pids[30];
    int sched_group1 = 0, sched_group2 = 0, sched_group3 = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 5: Grouped Ticket Levels: 10 procs with 1 ticket, 10 with 5 tickets, 10 with 10 tickets\n");

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

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            for (int k = 0; k < 10; k++)
                if (info[j].pid == pids[k])
                    sched_group1 += info[j].ticks_scheduled;
            for (int k = 10; k < 20; k++)
                if (info[j].pid == pids[k])
                    sched_group2 += info[j].ticks_scheduled;
            for (int k = 20; k < 30; k++)
                if (info[j].pid == pids[k])
                    sched_group3 += info[j].ticks_scheduled;
        }
    }

    for (i = 0; i < 30; i++)
    {
        wait();
    }

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

// Test 6: Mixed Workload (20 processes: 5 CPU-heavy, 5 I/O-bound, 5 short-lived, 5 mixed)
void run_mixed_workload_test(int *sched_group1_out, int *sched_group2_out, int *sched_group3_out, int *sched_group4_out)
{
    int pids[20];
    int sched_group1 = 0, sched_group2 = 0, sched_group3 = 0, sched_group4 = 0;
    struct pinfo info[64];
    int i;

    printf(1, "Test 6: Mixed Workload: 5 CPU-heavy (20 tickets), 5 I/O-bound (15 tickets), 5 short-lived (10 tickets), 5 mixed (5 tickets)\n");

    // Group 1: CPU-heavy (20 tickets each)
    for (i = 0; i < 5; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            settickets(20);
            for (volatile int j = 0; j < 500000000; j++)
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
                sleep(1);
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
                ;
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
                    ;
                sleep(1);
            }
            exit();
        }
    }

    sleep(50);

    if (getpinfo(info) < 0)
    {
        printf(1, "getpinfo failed\n");
    }
    else
    {
        for (int j = 0; j < 64; j++)
        {
            for (int k = 0; k < 5; k++)
                if (info[j].pid == pids[k])
                    sched_group1 += info[j].ticks_scheduled;
            for (int k = 5; k < 10; k++)
                if (info[j].pid == pids[k])
                    sched_group2 += info[j].ticks_scheduled;
            for (int k = 10; k < 15; k++)
                if (info[j].pid == pids[k])
                    sched_group3 += info[j].ticks_scheduled;
            for (int k = 15; k < 20; k++)
                if (info[j].pid == pids[k])
                    sched_group4 += info[j].ticks_scheduled;
        }
    }

    for (i = 0; i < 20; i++)
    {
        wait();
    }

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

int main(int argc, char *argv[])
{
    printf(1, "Starting lottery scheduler tests\n");

    int num_runs = 5;
    int total_a, total_b, total_c, total_d, total_e, total_f, total_g, total_h;
    int total_schedules;

    // Test 1: Low Process Count (3 processes, tickets 30, 20, 10)
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

    // Test 2: Basic Fairness (8 processes)
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
        int sched_a = 0, sched_b = 0, sched_c = 0, sched_d = 0, sched_e = 0, sched_f = 0, sched_g = 0, sched_h = 0;
        printf(1, "Run %d:\n", i + 1);
        run_basic_fairness_test(&sched_a, &sched_b, &sched_c, &sched_d, &sched_e, &sched_f, &sched_g, &sched_h);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_d += sched_d;
        total_e += sched_e;
        total_f += sched_f;
        total_g += sched_g;
        total_h += sched_h;
        total_schedules += (sched_a + sched_b + sched_c + sched_d + sched_e + sched_f + sched_g + sched_h);

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

    // Test 3: Switch Overhead (50 processes)
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

    // Test 4: Starvation Check (8 processes)
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
        int sched_a = 0, sched_b = 0, sched_c = 0, sched_d = 0, sched_e = 0, sched_f = 0, sched_g = 0, sched_h = 0;
        printf(1, "Run %d:\n", i + 1);
        run_starvation_test(&sched_a, &sched_b, &sched_c, &sched_d, &sched_e, &sched_f, &sched_g, &sched_h);

        total_a += sched_a;
        total_b += sched_b;
        total_c += sched_c;
        total_d += sched_d;
        total_e += sched_e;
        total_f += sched_f;
        total_g += sched_g;
        total_h += sched_h;
        total_schedules += (sched_a + sched_b + sched_c + sched_d + sched_e + sched_f + sched_g + sched_h);

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

    // Test 5: Grouped Ticket Levels (30 processes)
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

    // Test 6: Mixed Workload (20 processes)
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