#include <verifier.h>

int main(void)
{
    int buffer[10];
    int i = verifier_nondet_int();
    if (0 <= i && i < 10)
        buffer[i] = 1;
    return 0;
}

// EXPECT: line 8 store unproved ae-bounds-unknown
