// lotterytest.c
#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
    int pid, i;
    int child_wins = 0, parent_wins = 0;
    int trials = 10;
    int child_finish_time = 0; // Shared via pipe

    printf(1, "Running %d trials of lottery scheduling...\n", trials);

    for (i = 0; i < trials; i++)
    {
        int fd[2];
        pipe(fd); // Pipe to pass child's finish time
        pid = fork();
        if (pid < 0)
        {
            printf(1, "fork failed\n");
            exit();
        }

        if (pid == 0)
        {
            close(fd[0]); // Child writes
            settickets(getpid(), 100);
            for (int j = 0; j < 1000000; j++)
                ; // Busy work
            int finish = uptime();
            printf(1, "Child (100 tickets) done in trial %d at %d\n", i + 1, finish);
            write(fd[1], &finish, sizeof(finish));
            close(fd[1]);
            exit();
        }
        else
        {
            close(fd[1]); // Parent reads
            printf(1, "Parent PID: %d\n", getpid());
            settickets(getpid(), 10);
            for (int j = 0; j < 1000000; j++)
                ; // Same busy work
            int parent_time = uptime();
            wait();
            read(fd[0], &child_finish_time, sizeof(child_finish_time));
            close(fd[0]);
            printf(1, "Parent (10 tickets) done in trial %d at %d\n", i + 1, parent_time);
            if (child_finish_time <= parent_time) // Child wins if it finishes first
                child_wins++;
            else
                parent_wins++;
        }
    }

    printf(1, "\nResults after %d trials:\n", trials);
    printf(1, "Child (100 tickets) won %d times (~%d%%)\n", child_wins, (child_wins * 100) / trials);
    printf(1, "Parent (10 tickets) won %d times (~%d%%)\n", parent_wins, (parent_wins * 100) / trials);
    exit();
}