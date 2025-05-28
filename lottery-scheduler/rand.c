#include "types.h"
#include "rand.h"

// Define the random number generator state
unsigned int randstate = 1;

void srand(unsigned int seed)
{
    randstate = seed;
    if (randstate == 0)
        randstate = 1;
}

unsigned int rand(void)
{
    unsigned int x = randstate;
    x ^= (x << 13);
    x ^= (x >> 17);
    x ^= (x << 5);
    randstate = x;
    return x & 0x7fffffff;
}

unsigned int rand_range(unsigned int max)
{
    unsigned int threshold = (0x7fffffff / max) * max;
    unsigned int r;
    do
    {
        r = rand();
    } while (r >= threshold);
    return r % max;
}