#include <stddef.h>
#include <string.h>

size_t unknown(void);

int main(void)
{
    char dst[10], src[16];
    memcpy(dst, src, 10);
    memcpy(dst, src, 11);
    memcpy(dst, src, unknown());
    memset(dst, 0, 10);
    memset(dst, 0, 11);
    memcpy(&dst[2], src, 8);
    memcpy(&dst[2], src, 9);
    memmove(dst, src, 10);
    return 0;
}

// EXPECT: line 9 call-write proved ae-in-bounds
// EXPECT: line 9 call-read proved ae-in-bounds
// EXPECT: line 10 call-write unproved ae-out-of-bounds-if-reached
// EXPECT: line 10 call-read proved ae-in-bounds
// EXPECT: line 11 call-write unproved ae-length-unknown
// EXPECT: line 11 call-read unproved ae-length-unknown
// EXPECT: line 12 call-write proved ae-in-bounds
// EXPECT: line 13 call-write unproved ae-out-of-bounds-if-reached
// EXPECT: line 14 call-write proved ae-in-bounds
// EXPECT: line 14 call-read proved ae-in-bounds
// EXPECT: line 15 call-write unproved ae-out-of-bounds-if-reached
// EXPECT: line 15 call-read proved ae-in-bounds
// EXPECT: line 16 call-write proved ae-in-bounds
// EXPECT: line 16 call-read proved ae-in-bounds
