#include "verifier.h"

int main(void)
{
    volatile int scalar;
    volatile int array[3];
    scalar = 1;
    array[1] = scalar;
    volatile int *interior = array + 1;
    return *(interior + 1);
}

// EXPECT: line 7 store proved ae-in-bounds
// EXPECT: line 8 load proved ae-in-bounds
// EXPECT: line 8 store proved ae-in-bounds
// EXPECT: line 10 load unproved ae-bounds-unavailable
