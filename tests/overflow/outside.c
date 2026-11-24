#include "verifier.h"

int main(void)
{
    volatile int array[2];
    volatile int *end = &array[2];
    *end = 1;
    return 0;
}

// EXPECT: line 7 store unproved ae-out-of-bounds-if-reached
