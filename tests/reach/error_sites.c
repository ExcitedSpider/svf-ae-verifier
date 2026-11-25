#include "verifier.h"

void reach_error(void);

int main(int argc, char **argv)
{
    (void)argv;
    int zero = 0;
    if (zero)
        verifier_unreachable();
    verifier_unreachable();
    if (argc == 2)
        reach_error();
    return 0;
}

// EXPECT: line 10 verifier_unreachable unproved ae-no-state
// EXPECT: line 11 verifier_unreachable unproved ae-reachable
// EXPECT: line 13 reach_error unproved ae-reachable
