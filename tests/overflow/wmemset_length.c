#include <wchar.h>

int main(void)
{
    wchar_t source[100];
    wmemset(source, L'C', 99);   /* line 6: writes 396 of 400 bytes */
    source[99] = L'\0';
    return 0;
}

// EXPECT: line 6 call-write proved ae-in-bounds
// EXPECT: line 7 store proved ae-in-bounds
