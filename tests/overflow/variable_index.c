#include <verifier.h>

int unknown(void);

int main(void)
{
    int buffer[10] = {0};

    int seven = 7;
    buffer[seven] = 1;           /* line 10: index is exactly 7 */

    int k = unknown() ? 3 : 5;
    buffer[k] = 2;               /* line 13: index is 3 or 5 */

    int negative = -5;
    buffer[negative] = 3;        /* line 16: index is exactly -5 */

    buffer[2] = 4;               /* line 18: constant index */
    return buffer[seven];        /* line 19: load at index 7 */
}

// EXPECT: line 7 call-write proved ae-in-bounds
// EXPECT: line 10 store proved ae-in-bounds
// EXPECT: line 13 store proved ae-in-bounds
// EXPECT: line 16 store unproved ae-out-of-bounds-if-reached
// EXPECT: line 18 store proved ae-in-bounds
// EXPECT: line 19 load proved ae-in-bounds
