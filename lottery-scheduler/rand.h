#ifndef RAND_H
#define RAND_H

// Declare the random number generator state as extern
extern unsigned int randstate;

void srand(unsigned int seed);
unsigned int rand(void);
unsigned int rand_range(unsigned int max);

#endif