#include <wchar.h>

int main(void)
{
    wchar_t data[4];
    wmemset(data, L'a', 5);
    return 0;
}

// EXPECT: line 6 call-write unproved ae-out-of-bounds-if-reached
