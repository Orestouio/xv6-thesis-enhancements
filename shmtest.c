#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    int *shm1, *shm2;
    int pid;

    // Test 1: Multiple Shared Memory Regions
    printf(1, "Test 1: Opening two shared memory regions\n");
    shm1 = (int *)shm_open("/shm1", sizeof(int));
    if (shm1 == (int *)-1)
    {
        printf(1, "shm_open failed for /shm1\n");
        exit();
    }
    shm2 = (int *)shm_open("/shm2", sizeof(int));
    if (shm2 == (int *)-1)
    {
        printf(1, "shm_open failed for /shm2\n");
        exit();
    }

    *shm1 = 100;
    *shm2 = 200;
    printf(1, "Parent: Set /shm1 to %d, /shm2 to %d\n", *shm1, *shm2);

    pid = fork();
    if (pid == 0)
    {
        // Child process
        printf(1, "Child: /shm1 = %d, /shm2 = %d\n", *shm1, *shm2);
        *shm1 = 101;
        *shm2 = 201;
        printf(1, "Child: Set /shm1 to %d, /shm2 to %d\n", *shm1, *shm2);
        shm_close((int)shm1);
        shm_close((int)shm2);
        exit();
    }
    wait();
    printf(1, "Parent: /shm1 = %d, /shm2 = %d\n", *shm1, *shm2);
    shm_close((int)shm1);
    shm_close((int)shm2);

    // Test 2: Maximum Shared Memory Mappings
    printf(1, "\nTest 2: Maximum shared memory mappings\n");
    int mappings[5]; // MAX_SHM_MAPPINGS is 4, so try 5
    for (int i = 0; i < 5; i++)
    {
        char name[16];
        strcpy(name, "/shm_max"); // Copy the prefix
        name[8] = '0' + i;        // Append the digit (i ranges from 0 to 4)
        name[9] = '\0';           // Null-terminate the string
        mappings[i] = shm_open(name, sizeof(int));
        if (mappings[i] == -1)
        {
            printf(1, "shm_open failed for %s (expected for i=%d)\n", name, i);
        }
        else
        {
            printf(1, "Opened %s at address 0x%x\n", name, mappings[i]);
        }
    }
    // Clean up successful mappings
    for (int i = 0; i < 5; i++)
    {
        if (mappings[i] != -1)
            shm_close(mappings[i]);
    }

    // Test 3: Reusing Shared Memory Names
    printf(1, "\nTest 3: Reusing shared memory names\n");
    shm1 = (int *)shm_open("/shm_reuse", sizeof(int));
    if (shm1 == (int *)-1)
    {
        printf(1, "shm_open failed for /shm_reuse\n");
        exit();
    }
    *shm1 = 300;
    pid = fork();
    if (pid == 0)
    {
        // Child opens the same shared memory region
        shm2 = (int *)shm_open("/shm_reuse", sizeof(int));
        if (shm2 == (int *)-1)
        {
            printf(1, "Child: shm_open failed for /shm_reuse\n");
            exit();
        }
        printf(1, "Child: /shm_reuse = %d\n", *shm2);
        *shm2 = 301;
        printf(1, "Child: Set /shm_reuse to %d\n", *shm2);
        shm_close((int)shm2);
        exit();
    }
    wait();
    printf(1, "Parent: /shm_reuse = %d\n", *shm1);
    shm_close((int)shm1);

    // Test 4: Invalid Inputs
    printf(1, "\nTest 4: Invalid inputs\n");
    int invalid = shm_open("/shm_invalid", -1); // Negative size
    if (invalid == -1)
        printf(1, "shm_open with negative size failed (expected)\n");
    invalid = shm_open("/shm_invalid", 0); // Zero size
    if (invalid == -1)
        printf(1, "shm_open with zero size failed (expected)\n");
    invalid = shm_open("/shm_invalid", 5000); // Size > PGSIZE (4096)
    if (invalid == -1)
        printf(1, "shm_open with size > PGSIZE failed (expected)\n");
    invalid = shm_close(0x1234); // Invalid address
    if (invalid == -1)
        printf(1, "shm_close with invalid address failed (expected)\n");

    printf(1, "\nAll tests completed\n");
    exit();
}