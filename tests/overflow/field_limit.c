#include <string.h>

int main(void)
{
    int a[150];
    char b[512];
    char c[100];
    struct { int head; char padding[516]; int tail; } s;
    a[149] = 0;
    a[150] = 0;
    a[1000] = 0;
    a[100] = 0;
    b[600] = 0;
    c[200] = 0;
    s.head = 0;
    s.tail = 0;
    memset(a, 0, sizeof a);
    return 0;
}

// EXPECT: line 9 store unproved ae-bounds-unknown
// EXPECT: line 10 store unproved ae-bounds-unknown
// EXPECT: line 11 store unproved ae-bounds-unknown
// EXPECT: line 12 store proved ae-in-bounds
// EXPECT: line 13 store unproved ae-bounds-unknown
// EXPECT: line 14 store unproved ae-out-of-bounds-if-reached
// EXPECT: line 15 store proved ae-in-bounds
// EXPECT: line 16 store unproved ae-bounds-unknown
// EXPECT: line 17 call-write unproved ae-bounds-unknown
