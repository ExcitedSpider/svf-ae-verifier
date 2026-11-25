#include "verifier.h"

int main(void)
{
    volatile int *pointer = 0;
    return *pointer;
}

// EXPECT: line 6 load unproved ae-object-unknown
