#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define NUM_PROCS 5
#define LOOPS 1000000
#define NUM_TESTS 4
#define STATS_FILE "stats.txt"

// Define O_APPEND if not provided by fcntl.h
#ifndef O_APPEND
#define O_APPEND 0x0008
#endif

// Convert integer to string
int itoa(char *buf, int n)
{
    int i = 0;
    if (n == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (n > 0)
        {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
    }
    for (int j = 0; j < i / 2; j++)
    {
        char temp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = temp;
    }
    buf[i] = '\0';
    return i;
}

// Format stats into a buffer
void format_stats(char *buf, int pid, int priority, int start, int first_run, int end, int cs)
{
    char *p = buf;
    p += itoa(p, pid);
    *p++ = ' ';
    p += itoa(p, priority);
    *p++ = ' ';
    p += itoa(p, start);
    *p++ = ' ';
    p += itoa(p, first_run);
    *p++ = ' ';
    p += itoa(p, end);
    *p++ = ' ';
    p += itoa(p, cs);
    *p++ = '\n';
    *p = '\0';
}

// Parse stats from a line
void parse_stats(char *line, int *pid, int *priority, int *start, int *first_run, int *end, int *cs)
{
    *pid = atoi(line);
    line = strchr(line, ' ') + 1;
    *priority = atoi(line);
    line = strchr(line, ' ') + 1;
    *start = atoi(line);
    line = strchr(line, ' ') + 1;
    *first_run = atoi(line);
    line = strchr(line, ' ') + 1;
    *end = atoi(line);
    line = strchr(line, ' ') + 1;
    *cs = atoi(line);
}

void cpu_bound(int priority, int test_num)
{
    int pid = getpid();
    setpriority(pid, priority);

    int start_ticks = uptime();
    int first_run_ticks = uptime();
    int context_switches = 0;

    int i, j;
    volatile int sum = 0;
    for (i = 0; i < LOOPS; i++)
    {
        for (j = 0; j < 100; j++)
        {
            sum += i * j;
        }
        if (i % (LOOPS / 10) == 0)
        {
            context_switches++;
            sleep(0);
        }
    }
    int end_ticks = uptime();

    int fd = open(STATS_FILE, O_WRONLY | O_CREATE | O_APPEND);
    if (fd < 0)
    {
        printf(1, "Error opening stats file\n");
        exit();
    }
    char buf[128];
    format_stats(buf, pid, priority, start_ticks, first_run_ticks, end_ticks, context_switches);
    write(fd, buf, strlen(buf));
    close(fd);
}

void print_stats(int test_num)
{
    int fd = open(STATS_FILE, O_RDONLY);
    if (fd < 0)
    {
        printf(1, "Error opening stats file\n");
        return;
    }

    char buf[512];
    int n;
    printf(1, "Test %d: Statistics\n", test_num);
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        char *p = buf;
        while (p < buf + n)
        {
            int pid, priority, start, first_run, end, cs;
            parse_stats(p, &pid, &priority, &start, &first_run, &end, &cs);
            int turnaround = end - start;
            int response = first_run - start;
            int waiting = turnaround - (LOOPS / 1000); // Rough estimate
            printf(1, "Process %d (priority %d): turnaround=%d ticks, response=%d ticks, waiting=%d ticks, context_switches=%d\n",
                   pid, priority, turnaround, response, waiting, cs);
            while (p < buf + n && *p != '\n')
                p++;
            if (p < buf + n)
                p++;
        }
    }
    close(fd);
}

void run_test(int test_num, int priorities[])
{
    int pids[NUM_PROCS];
    int i;

    unlink(STATS_FILE);

    printf(1, "\nTest %d: Starting priority scheduler test (priorities: ", test_num);
    for (i = 0; i < NUM_PROCS; i++)
    {
        printf(1, "%d ", priorities[i]);
    }
    printf(1, ")\n");

    for (i = 0; i < NUM_PROCS; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            cpu_bound(priorities[i], test_num);
            exit();
        }
        sleep(1);
    }

    for (i = 0; i < NUM_PROCS; i++)
    {
        wait();
    }

    print_stats(test_num);
    printf(1, "Test %d: All processes completed\n", test_num);
}

int main(int argc, char *argv[])
{
    int test_priorities[NUM_TESTS][NUM_PROCS] = {
        {3, 5, 7, 8, 9},
        {1, 2, 3, 4, 5},
        {5, 4, 3, 2, 1},
        {3, 3, 5, 5, 1}};

    printf(1, "Starting priority scheduler tests\n");

    for (int i = 0; i < NUM_TESTS; i++)
    {
        run_test(i + 1, test_priorities[i]);
    }

    printf(1, "\nAll tests completed\n");
    exit();
}