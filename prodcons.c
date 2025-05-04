#include "types.h"
#include "user.h"

#define BUFSIZE 5 // Size of the circular buffer
#define NITEMS 10 // Number of items to produce/consume

struct buffer
{
    int data[BUFSIZE]; // Circular buffer to store items
    int in;            // Index where the producer will write
    int out;           // Index where the consumer will read
};

int main(int argc, char *argv[])
{
    struct buffer *buf;
    int empty, full, print_sem;
    int pid;

    // Open shared memory for the buffer
    buf = (struct buffer *)shm_open("/buffer", sizeof(struct buffer));
    if (buf == (struct buffer *)-1)
    {
        printf(1, "prodcons: shm_open failed\n");
        exit();
    }

    // Initialize the buffer
    buf->in = 0;
    buf->out = 0;

    // Initialize semaphores
    empty = sem_init(BUFSIZE); // Number of empty slots
    full = sem_init(0);        // Number of filled slots
    print_sem = sem_init(1);   // Semaphore for synchronized printing
    if (empty < 0 || full < 0 || print_sem < 0)
    {
        printf(1, "prodcons: sem_init failed\n");
        shm_close((int)buf);
        exit();
    }

    pid = fork();
    if (pid < 0)
    {
        printf(1, "prodcons: fork failed\n");
        shm_close((int)buf);
        exit();
    }

    if (pid == 0)
    {
        // Producer (child process)
        for (int i = 0; i < NITEMS; i++)
        {
            sem_wait(empty); // Wait for an empty slot
            buf->data[buf->in] = i;
            sem_wait(print_sem); // Acquire print semaphore
            printf(1, "Producer: produced %d at index %d\n", i, buf->in);
            sem_post(print_sem); // Release print semaphore
            buf->in = (buf->in + 1) % BUFSIZE;
            sem_post(full); // Signal a filled slot
            sleep(1);       // Small delay to allow consumer to catch up
        }
        exit();
    }
    else
    {
        // Consumer (parent process)
        for (int i = 0; i < NITEMS; i++)
        {
            sem_wait(full); // Wait for a filled slot
            int item = buf->data[buf->out];
            sem_wait(print_sem); // Acquire print semaphore
            printf(1, "Consumer: consumed %d from index %d\n", item, buf->out);
            sem_post(print_sem); // Release print semaphore
            buf->out = (buf->out + 1) % BUFSIZE;
            sem_post(empty); // Signal an empty slot
        }
        wait();              // Wait for the producer to finish
        shm_close((int)buf); // Clean up shared memory
        exit();
    }
}