#include <stddef.h>

int main(void)
{
    wchar_t wide[10];
    long longs[10];
    int i = 3;
    wide[i] = L'a';
    longs[i] = 1;
    return 0;
}

// EXPECT: line 8 store proved ae-in-bounds
// EXPECT: line 9 store proved ae-in-bounds
// OFFSET: line 8 12 12
// OFFSET: line 9 24 24
