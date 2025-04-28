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
        if (sleep1 > 0)
            sleep(sleep1);
        for (i = 0; i < iterations1; i++)
        {
            counter++;
            if (i % 100000 == 0) // Yield every 100K iterations
                yield();
        }
        printf(1, "Process A (pid=%d, %d tickets) done\n", getpid(), tickets1);
        exit();
    }
    settickets_pid(pid1, tickets1);

    pid2 = fork();
    if (pid2 == 0)
    {
        if (sleep2 > 0)
            sleep(sleep2);
        for (i = 0; i < iterations2; i++)
        {
            counter++;
            if (i % 100000 == 0)
                yield();
        }
        printf(1, "Process B (pid=%d, %d tickets) done\n", getpid(), tickets2);
        exit();
    }
    settickets_pid(pid2, tickets2);

    pid3 = fork();
    if (pid3 == 0)
    {
        if (sleep3 > 0)
            sleep(sleep3);
        for (i = 0; i < iterations3; i++)
        {
            counter++;
            if (i % 100000 == 0)
                yield();
        }
        printf(1, "Process C (pid=%d, %d tickets) done\n", getpid(), tickets3);
        exit();
    }
    settickets_pid(pid3, tickets3);

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

    printf(1, "After wait, looking for PIDs: %d, %d, %d\n", pid1, pid2, pid3);
    for (i = 0; i < 64; i++)
    {
        if (info[i].pid > 0)
            printf(1, "info[%d]: pid=%d, tickets=%d, scheduled=%d\n",
                   i, info[i].pid, info[i].tickets, info[i].ticks_scheduled);
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

// Test 2: Switch Overhead (fork-and-exit)
void run_switch_test(int tickets1, int tickets2, int tickets3, int *sched1_out, int *sched2_out, int *sched3_out)
{
    int pid1, pid2, pid3;
    int i;
    struct pinfo info[64];
    int sched1 = 0, sched2 = 0, sched3 = 0;
    int start_time, end_time;

    printf(1, "%s: Tickets=%d,%d,%d\n", "Test 2: Switch Overhead", tickets1, tickets2, tickets3);

    start_time = uptime();

    // Process A: 17 forks
    for (i = 0; i < 17; i++)
    {
        pid1 = fork();
        if (pid1 == 0)
        {
            settickets(tickets1);
            // Perform a larger workload to ensure scheduling
            for (volatile int j = 0; j < 10000000; j++)
                ; // Increased to 10M iterations
            // Sleep to keep the process RUNNABLE for a while
            sleep(10); // Sleep for 10 ticks
            exit();
        }

        // Collect scheduling data while the child is still alive
        int found = 0;
        for (int k = 0; k < 50; k++) // Retry up to 50 times
        {
            if (getpinfo(info) < 0)
            {
                printf(1, "getpinfo failed\n");
                break;
            }
            for (int j = 0; j < 64; j++)
            {
                if (info[j].pid == pid1)
                {
                    sched1 += info[j].ticks_scheduled;
                    found = 1;
                    if (info[j].ticks_scheduled > 0) // Only print if non-zero
                        /*printf(1, "Captured pid=%d, ticks_scheduled=%d (total sched1=%d)\n",
                               pid1, info[j].ticks_scheduled, sched1);*/
                        break;
                }
            }
            yield(); // Yield to give the child a chance to run
        }
        if (!found)
        {
            printf(1, "Failed to capture scheduling data for pid=%d\n", pid1);
        }

        wait();
        yield();
    }

    // Process B: 17 forks
    for (i = 0; i < 17; i++)
    {
        pid2 = fork();
        if (pid2 == 0)
        {
            settickets(tickets2);
            for (volatile int j = 0; j < 10000000; j++)
                ;
            sleep(10);
            exit();
        }

        int found = 0;
        for (int k = 0; k < 50; k++)
        {
            if (getpinfo(info) < 0)
            {
                printf(1, "getpinfo failed\n");
                break;
            }
            for (int j = 0; j < 64; j++)
            {
                if (info[j].pid == pid2)
                {
                    sched2 += info[j].ticks_scheduled;
                    found = 1;
                    if (info[j].ticks_scheduled > 0)
                        /*printf(1, "Captured pid=%d, ticks_scheduled=%d (total sched2=%d)\n",
                               pid2, info[j].ticks_scheduled, sched2);*/
                        break;
                }
            }
            yield();
        }
        if (!found)
        {
            printf(1, "Failed to capture scheduling data for pid=%d\n", pid2);
        }

        wait();
        yield();
    }

    // Process C: 16 forks
    for (i = 0; i < 16; i++)
    {
        pid3 = fork();
        if (pid3 == 0)
        {
            settickets(tickets3);
            for (volatile int j = 0; j < 10000000; j++)
                ;
            sleep(10);
            exit();
        }

        int found = 0;
        for (int k = 0; k < 50; k++)
        {
            if (getpinfo(info) < 0)
            {
                printf(1, "getpinfo failed\n");
                break;
            }
            for (int j = 0; j < 64; j++)
            {
                if (info[j].pid == pid3)
                {
                    sched3 += info[j].ticks_scheduled;
                    found = 1;
                    if (info[j].ticks_scheduled > 0)
                        /*printf(1, "Captured pid=%d, ticks_scheduled=%d (total sched3=%d)\n",
                               pid3, info[j].ticks_scheduled, sched3);*/
                        break;
                }
            }
            yield();
        }
        if (!found)
        {
            printf(1, "Failed to capture scheduling data for pid=%d\n", pid3);
        }

        wait();
        yield();
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
        if (info[i].pid > 0)
        {
        }
        /*{printf(1, "info[%d]: pid=%d, tickets=%d, scheduled=%d\n",
               i, info[i].pid, info[i].tickets, info[i].ticks_scheduled);}*/
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

    int num_runs = 10;
    int total_a, total_b, total_c; // To accumulate schedules for each process
    int total_schedules;           // To accumulate total schedules across all runs

    // Test 1: CPU-heavy
    total_a = 0;
    total_b = 0;
    total_c = 0;
    total_schedules = 0;
    for (int i = 0; i < num_runs; i++)
    {
        int sched_a = 0, sched_b = 0, sched_c = 0;
        printf(1, "Run %d:\n", i + 1);
        run_workload_test(30, 20, 10, 10000000, 10000000, 10000000, 0, 0, 0, "Test 1: CPU-heavy",
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
    /*
        // Test 3: I/O-bound
        total_a = 0;
        total_b = 0;
        total_c = 0;
        total_schedules = 0;
        for (int i = 0; i < num_runs; i++)
        {
            int sched_a = 0, sched_b = 0, sched_c = 0;
            printf(1, "Run %d:\n", i + 1);
            run_workload_test(30, 20, 10, 0, 0, 0, 100, 90, 80, "Test 3: I/O-bound",
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

        printf(1, "  Process A: %d schedules (%d.%d%%), Expected: 50%%\n",
               total_a, percent_a_int, percent_a_dec);
        printf(1, "  Process B: %d schedules (%d.%d%%), Expected: 33%%\n",
               total_b, percent_b_int, percent_b_dec);
        printf(1, "  Process C: %d schedules (%d.%d%%), Expected: 16%%\n",
               total_c, percent_c_int, percent_c_dec);
        printf(1, "\n");

            // Test 4: Mixed Load
            total_a = 0;
            total_b = 0;
            total_c = 0;
            total_schedules = 0;
            for (int i = 0; i < num_runs; i++)
            {
                int sched_a = 0, sched_b = 0, sched_c = 0;
                printf(1, "Run %d:\n", i + 1);
                run_workload_test(30, 20, 10, 10000000, 10000000, 0, 0, 0, 100, "Test 4: Mixed Load",
                                  &sched_a, &sched_b, &sched_c);

                total_a += sched_a;
                total_b += sched_b;
                total_c += sched_c;
                total_schedules += (sched_a + sched_b + sched_c);

                printf(1, "\n");
                sleep(5);
            }

            printf(1, "\nAverage Results Over %d Runs for Test 4:\n", num_runs);
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

            // Test 5: Process Creation
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

            printf(1, "  Process A: %d schedules (%d.%d%%), Expected: 50%%\n",
                   total_a, percent_a_int, percent_a_dec);
            printf(1, "  Process B: %d schedules (%d.%d%%), Expected: 33%%\n",
                   total_b, percent_b_int, percent_b_dec);
            printf(1, "  Process C: %d schedules (%d.%d%%), Expected: 16%%\n",
                   total_c, percent_c_int, percent_c_dec);
            printf(1, "\n");

            // Test 6: Short Tasks
            total_a = 0;
            total_b = 0;
            total_c = 0;
            total_schedules = 0;
            for (int i = 0; i < num_runs; i++)
            {
                int sched_a = 0, sched_b = 0, sched_c = 0;
                printf(1, "Run %d:\n", i + 1);
                run_workload_test(30, 20, 10, 10000000, 10000000, 10000000, 0, 0, 0, "Test 6: Short Tasks",
                                  &sched_a, &sched_b, &sched_c);

                total_a += sched_a;
                total_b += sched_b;
                total_c += sched_c;
                total_schedules += (sched_a + sched_b + sched_c);

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

            printf(1, "  Process A: %d schedules (%d.%d%%), Expected: 50%%\n",
                   total_a, percent_a_int, percent_a_dec);
            printf(1, "  Process B: %d schedules (%d.%d%%), Expected: 33%%\n",
                   total_b, percent_b_int, percent_b_dec);
            printf(1, "  Process C: %d schedules (%d.%d%%), Expected: 16%%\n",
                   total_c, percent_c_int, percent_c_dec);
            printf(1, "\n");

            // Test 7: Starvation Check
            total_a = 0;
            total_b = 0;
            total_c = 0;
            total_schedules = 0;
            for (int i = 0; i < num_runs; i++)
            {
                int sched_a = 0, sched_b = 0, sched_c = 0;
                printf(1, "Run %d:\n", i + 1);
                run_workload_test(30, 20, 10, 500000, 10000000, 10000000, 0, 0, 0, "Test 7: Starvation Check",
                                  &sched_a, &sched_b, &sched_c);

                total_a += sched_a;
                total_b += sched_b;
                total_c += sched_c;
                total_schedules += (sched_a + sched_b + sched_c);

                printf(1, "\n");
                sleep(5);
            }

            printf(1, "\nAverage Results Over %d Runs for Test 7:\n", num_runs);
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
            printf(1, "\n");*/

    printf(1, "\nAll tests complete\n");
    sleep(5);
    exit();
}