#include "verifier.h"

int main(void)
{
    volatile int scalar;
    volatile int array[3];
    scalar = 1;
    array[1] = scalar;
    array[2] = 3;
    volatile int *interior = array + 1;
    int zero = 0;
    if (zero)
        return scalar;
    return *(interior + 1);
}

// EXPECT: line 7 store proved ae-in-bounds
// EXPECT: line 8 load proved ae-in-bounds
// EXPECT: line 8 store proved ae-in-bounds
// EXPECT: line 9 store proved ae-in-bounds
// EXPECT: line 13 load unproved ae-no-state
// EXPECT: line 14 load unproved ae-bounds-unknown
