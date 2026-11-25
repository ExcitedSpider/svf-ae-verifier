#include <verifier.h>

int main(void)
{
    int buffer[10];
    int i = verifier_nondet_int();
    buffer[i] = 1;
    return 0;
}

// EXPECT: line 7 store unproved ae-bounds-unknown
