#include "verifier.h"

int main(int argc, char **argv)
{
    (void)argv;
    verifier_assert(1);
    verifier_assert(0);
    verifier_assert(argc == 2);
    verifier_eq(12, 12);
    verifier_eq(12, 13);
    __verifier_assert(1, 42);
    return 0;
}

// EXPECT: line 6 verifier_assert proved ae-condition-true
// EXPECT: line 7 verifier_assert unproved ae-must-fail-if-reached
// EXPECT: line 8 verifier_assert unproved ae-condition-unknown
// EXPECT: line 9 verifier_eq proved ae-condition-true
// EXPECT: line 10 verifier_eq unproved ae-must-fail-if-reached
// EXPECT: line 42 verifier_assert proved ae-condition-true
